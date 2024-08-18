/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "hooks.h"
#include "usb_dc.h"

#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>
LOG_MODULE_DECLARE(usb_dc, LOG_LEVEL_INF);

struct usb_controller_status {
	bool suspended;
	bool configured;
};

#ifdef CONFIG_USB_DEVICE_REMOTE_WAKEUP
static void usb_pm_change_notify_hooks(void)
{
	hook_notify(HOOK_USB_PM_CHANGE);
}
DECLARE_DEFERRED(usb_pm_change_notify_hooks);
#endif

struct usb_controller_status usb_dc_status;

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	switch (status) {
	case USB_DC_RESET:
		usb_dc_status.configured = false;
		usb_dc_status.suspended = false;
		break;
	case USB_DC_CONFIGURED:
		usb_dc_status.configured = true;
		break;
	case USB_DC_DISCONNECTED:
		usb_dc_status.configured = false;
		usb_dc_status.suspended = false;
		break;
	case USB_DC_SUSPEND:
		usb_dc_status.suspended = true;
#ifdef CONFIG_USB_DEVICE_REMOTE_WAKEUP
		hook_call_deferred(&usb_pm_change_notify_hooks_data, 0);
#endif
		break;
	case USB_DC_RESUME:
		usb_dc_status.suspended = false;
#ifdef CONFIG_USB_DEVICE_REMOTE_WAKEUP
		hook_call_deferred(&usb_pm_change_notify_hooks_data, 0);
#endif
		break;
	default:
		break;
	}
}

int usb_is_remote_wakeup_enabled(void)
{
	if (IS_ENABLED(CONFIG_USB_DEVICE_REMOTE_WAKEUP)) {
		return (usb_get_remote_wakeup_status()) ? 1 : 0;
	}

	return 0;
}

int usb_is_suspended(void)
{
	return usb_dc_status.suspended ? 1 : 0;
}

bool check_usb_is_configured(void)
{
	return usb_dc_status.configured;
}

bool request_usb_wake(void)
{
	if (IS_ENABLED(CONFIG_USB_DEVICE_REMOTE_WAKEUP)) {
		usb_wakeup_request();
		return usb_dc_status.suspended ? false : true;
	}
	return false;
}

static int usb_dc_init(void)
{
	int ret = usb_enable(status_cb);

	if (ret != 0) {
		LOG_ERR("failed to enable usb");
		return ret;
	}

	return 0;
}

SYS_INIT(usb_dc_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
