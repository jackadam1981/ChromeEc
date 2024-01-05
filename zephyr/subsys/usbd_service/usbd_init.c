/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/iterable_sections.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(usbd_hid_init, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(usbd_hid,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_USBD_HID_VID, CONFIG_USBD_HID_PID);

USBD_DESC_LANG_DEFINE(lang);
USBD_DESC_MANUFACTURER_DEFINE(mfr, CONFIG_USBD_HID_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(product, CONFIG_USBD_HID_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(sn, CONFIG_USBD_HID_SN);

static const uint8_t attributes = (IS_ENABLED(CONFIG_HID_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_USBD_HID_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

USBD_CONFIGURATION_DEFINE(config_hid,
			  attributes,
			  CONFIG_USBD_HID_MAX_POWER);

static int usbd_hid_init(void)
{
	int err;

	err = usbd_add_descriptor(&usbd_hid, &lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd_hid, &mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&usbd_hid, &product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		return err;
	}

	sn.custom_sn = 1;
	err = usbd_add_descriptor(&usbd_hid, &sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return err;
	}

	err = usbd_add_configuration(&usbd_hid, &config_hid);
	if (err) {
		LOG_ERR("Failed to add configuration (%d)", err);
		return err;
	}

	STRUCT_SECTION_FOREACH(usbd_class_node, node) {
		/* Pull everything that is enabled in our configuration. */
		err = usbd_register_class(&usbd_hid, node->name, 1);
		if (err) {
			LOG_ERR("Failed to register %s (%d)", node->name, err);
			return err;
		}
	}

	usbd_device_set_code_triple(&usbd_hid, 0, 0, 0);

	err = usbd_init(&usbd_hid);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		return err;
	}

	err = usbd_enable(&usbd_hid);
	if (err) {
		LOG_ERR("Failed to enable device support");
		return err;
	}

	return 0;
}
SYS_INIT(usbd_hid_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
