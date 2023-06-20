/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "util.h"
#include "usb_dc.h"

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(usb_dc, LOG_LEVEL_INF);

static enum usb_dc_status_code usb_status;

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	usb_status = status;
}

bool check_usb_is_suspended(void)
{
	return (usb_status == USB_DC_SUSPEND) ? true : false;
}

void request_usb_wake(void)
{
	if (IS_ENABLED(CONFIG_USB_DEVICE_REMOTE_WAKEUP)) {
		if (usb_status == USB_DC_SUSPEND) {
			usb_wakeup_request();
			return;
		}
	}
}

static int usb_dc_init(void)
{
	int ret = usb_enable(status_cb);

	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	return 0;

}

SYS_INIT(usb_dc_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
