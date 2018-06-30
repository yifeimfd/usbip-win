#include "stub_driver.h"

#include "stub_dev.h"
#include "dbgcommon.h"
#include "stub_dbg.h"
#include "stub_usbd.h"
#include "devconf.h"

PUSB_CONFIGURATION_DESCRIPTOR get_usb_dsc_conf(usbip_stub_dev_t *devstub, UCHAR idx);

#ifdef DBG

const char *
dbg_info_intf(PUSBD_INTERFACE_INFORMATION info_intf)
{
	static char	buf[128];

	if (info_intf == NULL)
		return "<null>";

	dbg_snprintf(buf, 128, "num:%hhu,alt:%hhu", info_intf->InterfaceNumber, info_intf->AlternateSetting);

	return buf;
}

const char *
dbg_info_pipe(PUSBD_PIPE_INFORMATION info_pipe)
{
	static char	buf[128];

	if (info_pipe == NULL)
		return "<null>";

	dbg_snprintf(buf, 128, "epaddr:%hhx", info_pipe->EndpointAddress);

	return buf;
}

#endif

static PUSBD_INTERFACE_INFORMATION
dup_info_intf(PUSBD_INTERFACE_INFORMATION info_intf)
{
	PUSBD_INTERFACE_INFORMATION	info_intf_copied;
	int	size_info = INFO_INTF_SIZE(info_intf);

	info_intf_copied = ExAllocatePoolWithTag(NonPagedPool, size_info, USBIP_STUB_POOL_TAG);
	if (info_intf_copied == NULL) {
		DBGE(DBG_GENERAL, "dup_info_intf: out of memory\n");
		return NULL;
	}
	RtlCopyMemory(info_intf_copied, info_intf, size_info);
	return info_intf_copied;
}

static BOOLEAN
build_infos_intf(devconf_t *devconf, PUSBD_INTERFACE_INFORMATION infos_intf)
{
	PUSBD_INTERFACE_INFORMATION	info_intf;
	unsigned	i;

	info_intf = infos_intf;
	for (i = 0; i < devconf->bNumInterfaces; i++) {
		devconf->infos_intf[i] = dup_info_intf(info_intf);
		if (devconf->infos_intf[i] == NULL) {
			DBGE(DBG_GENERAL, "build_infos_intf: out of memory\n");
			return FALSE;
		}
		info_intf = (PUSBD_INTERFACE_INFORMATION)((PUCHAR)info_intf + INFO_INTF_SIZE(info_intf));
	}
	return TRUE;
}

devconf_t *
create_devconf(PUSB_CONFIGURATION_DESCRIPTOR dsc_conf, USBD_CONFIGURATION_HANDLE hconf, PUSBD_INTERFACE_INFORMATION infos_intf)
{
	devconf_t	*devconf;
	int	size_devconf;

	size_devconf = sizeof(devconf_t) - sizeof(PUSBD_INTERFACE_INFORMATION) +
		dsc_conf->bNumInterfaces * sizeof(PUSBD_INTERFACE_INFORMATION);
	devconf = (devconf_t *)ExAllocatePoolWithTag(NonPagedPool, size_devconf, USBIP_STUB_POOL_TAG);
	if (devconf == NULL) {
		DBGE(DBG_GENERAL, "create_devconf: out of memory\n");
		return NULL;
	}

	devconf->bConfigurationValue = dsc_conf->bConfigurationValue;
	devconf->bNumInterfaces = dsc_conf->bNumInterfaces;
	devconf->hConf = hconf;
	RtlZeroMemory(devconf->infos_intf, sizeof(PUSBD_INTERFACE_INFORMATION) * devconf->bNumInterfaces);

	if (!build_infos_intf(devconf, infos_intf)) {
		free_devconf(devconf);
		return NULL;
	}

	return devconf;
}

void
free_devconf(devconf_t *devconf)
{
	unsigned	i;

	if (devconf == NULL)
		return;
	for (i = 0; i < devconf->bNumInterfaces; i++) {
		if (devconf->infos_intf[i] == NULL)
			break;
		ExFreePoolWithTag(devconf->infos_intf[i], USBIP_STUB_POOL_TAG);
	}
	ExFreePoolWithTag(devconf, USBIP_STUB_POOL_TAG);
}

void
update_devconf(devconf_t *devconf, PUSBD_INTERFACE_INFORMATION info_intf)
{
	unsigned	i;

	for (i = 0; i < devconf->bNumInterfaces; i++) {
		PUSBD_INTERFACE_INFORMATION	info_intf_exist;

		info_intf_exist = devconf->infos_intf[i];
		if (info_intf->InterfaceNumber == info_intf->InterfaceNumber) {
			PUSBD_INTERFACE_INFORMATION	info_intf_new;

			info_intf_new = dup_info_intf(info_intf);
			if (info_intf_new == NULL) {
				DBGE(DBG_DEVCONF, "update_devconf: out of memory\n");
				return;
			}
			ExFreePoolWithTag(info_intf_exist, USBIP_STUB_POOL_TAG);
			devconf->infos_intf[i] = info_intf_new;
			return;
		}
	}

	DBGE(DBG_DEVCONF, "update_devconf: non-existent interface info: %s\n", dbg_info_intf(info_intf));
}

PUSBD_INTERFACE_INFORMATION
get_info_intf(devconf_t *devconf, UCHAR intf_num)
{
	int	i;

	if (devconf == NULL)
		return NULL;

	for (i = 0; i < devconf->bNumInterfaces; i++) {
		PUSBD_INTERFACE_INFORMATION	info_intf;

		info_intf = devconf->infos_intf[i];
		if (info_intf->InterfaceNumber == intf_num)
			return info_intf;
	}
	return NULL;
}

PUSBD_PIPE_INFORMATION
get_intf_info_pipe(PUSBD_INTERFACE_INFORMATION info_intf, UCHAR epaddr)
{
	unsigned	i;

	for (i = 0; i < info_intf->NumberOfPipes; i++) {
		PUSBD_PIPE_INFORMATION	info_pipe;

		info_pipe = info_intf->Pipes + i;
		if (info_pipe->EndpointAddress == epaddr)
			return info_pipe;
	}

	return NULL;
}

PUSBD_PIPE_INFORMATION
get_info_pipe(devconf_t *devconf, UCHAR epaddr)
{
	unsigned	i;

	if (devconf == NULL)
		return NULL;

	for (i = 0; i < devconf->bNumInterfaces; i++) {
		PUSBD_INTERFACE_INFORMATION	info_intf;
		PUSBD_PIPE_INFORMATION		info_pipe;

		info_intf = devconf->infos_intf[i];
		info_pipe = get_intf_info_pipe(info_intf, epaddr);
		if (info_pipe != NULL)
			return info_pipe;
	}

	return NULL;
}