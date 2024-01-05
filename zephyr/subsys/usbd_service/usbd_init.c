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
USBD_DESC_SERIAL_NUMBER_DEFINE(sn);

static const uint8_t attributes = (IS_ENABLED(CONFIG_HID_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_USBD_HID_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

USBD_CONFIGURATION_DEFINE(config_hid,
			  attributes,
			  CONFIG_USBD_HID_MAX_POWER);

struct usb_controller_status {
	bool suspended;
};

static struct usb_controller_status usbd_status;

bool check_usb_is_suspended(void)
{
	return usbd_status.suspended;
}

bool request_usb_wake(void)
{
	if (IS_ENABLED(CONFIG_USBD_HID_REMOTE_WAKEUP)) {
		usbd_wakeup_request(&usbd_hid);
		return usbd_status.suspended ? false : true;
	}
	return false;
}

static void usbd_hid_msg_cb(struct usbd_contex *const ctx, const struct usbd_msg *msg)
{
	LOG_INF("usbd hid message: %s", usbd_msg_type_string(msg->type));
	switch(msg->type) {
	case USBD_MSG_SUSPEND:
		usbd_status.suspended = true;
		break;
	case USBD_MSG_RESUME:
		usbd_status.suspended = false;
		break;
	default:
		break;
	}
}

static int register_fs_classes(struct usbd_contex *uds_ctx)
{
	int err = 0;

	STRUCT_SECTION_FOREACH_ALTERNATE(usbd_class_fs, usbd_class_node, c_nd) {
		/* Pull everything that is enabled in our configuration. */
		err = usbd_register_class(uds_ctx, c_nd->c_data->name,
					  USBD_SPEED_FS, 1);
		if (err) {
			LOG_ERR("Failed to register FS %s (%d)",
				c_nd->c_data->name, err);
			return err;
		}

		LOG_DBG("Register FS %s", c_nd->c_data->name);
	}

	return err;
}

static int register_hs_classes(struct usbd_contex *uds_ctx)
{
	int err = 0;

	STRUCT_SECTION_FOREACH_ALTERNATE(usbd_class_hs, usbd_class_node, c_nd) {
		/* Pull everything that is enabled in our configuration. */
		err = usbd_register_class(uds_ctx, c_nd->c_data->name,
					  USBD_SPEED_HS, 1);
		if (err) {
			LOG_ERR("Failed to register HS %s (%d)",
				c_nd->c_data->name, err);
			return err;
		}

		LOG_DBG("Register HS %s", c_nd->c_data->name);
	}

	return err;
}

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

	sn.str.use_hwinfo = false;
	err = usbd_add_descriptor(&usbd_hid, &sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		return err;
	}

	enum usbd_speed speed = usbd_caps_speed(&usbd_hid);

	err = usbd_add_configuration(&usbd_hid, speed, &config_hid);
	if (err) {
		LOG_ERR("Failed to add configuration (%d)", err);
		return err;
	}

	if (speed == USBD_SPEED_FS) {
		err = register_fs_classes(&usbd_hid);
	} else if (speed == USBD_SPEED_HS) {
		err = register_hs_classes(&usbd_hid);
	}

	// STRUCT_SECTION_FOREACH(usbd_class_node, node) {
	// 	/* Pull everything that is enabled in our configuration. */
	// 	err = usbd_register_class(&usbd_hid, node->name, 1);
	// 	if (err) {
	// 		LOG_ERR("Failed to register %s (%d)", node->name, err);
	// 		return err;
	// 	}
	// }

	err = usbd_device_set_code_triple(&usbd_hid, speed, 0, 0, 0);
	if (err) {
		LOG_ERR("Failed to set descriptor code");
		return err;
	}

	err = usbd_msg_register_cb(&usbd_hid, usbd_hid_msg_cb);
	if (err) {
		LOG_ERR("Failed to register message callback");
		return err;
	}

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
