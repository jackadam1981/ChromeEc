/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "usbd_init.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_DECLARE(usb_device_init, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(usb_device, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_USB_DEVICE_VID, CONFIG_USB_DEVICE_PID);

USBD_DESC_LANG_DEFINE(lang);
USBD_DESC_MANUFACTURER_DEFINE(mfr, CONFIG_USB_DEVICE_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(product, CONFIG_USB_DEVICE_PRODUCT);
USBD_DESC_STRING_DEFINE(sn, CONFIG_USB_DEVICE_SN,
			USBD_DUT_STRING_SERIAL_NUMBER);

static const uint8_t attributes =
	(IS_ENABLED(CONFIG_USB_DEVICE_SELF_POWERED) ? USB_SCD_SELF_POWERED :
						      0) |
	(IS_ENABLED(CONFIG_USB_DEVICE_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP :
						       0);

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");

USBD_CONFIGURATION_DEFINE(usb_device_fs_config, attributes,
			  CONFIG_USB_DEVICE_MAX_POWER, &fs_cfg_desc);

static struct usb_msg_manager {
	msg_callback_t *callbacks;
	int callback_count;
	int max_callbacks;
} msg_manager = {
	.callbacks = NULL,
	.callback_count = 0,
	.max_callbacks = 0,
};

#if defined(CONFIG_CROS_EC_RO) && defined(CONFIG_USBD_HID_KEYBOARD)
__overridable void keyboard_state_changed(int row, int col, int is_pressed)
{
}
#endif /* defined(CONFIG_CROS_EC_RO) && defined(CONFIG_USBD_HID_KEYBOARD) */

#if defined(CONFIG_CROS_EC_RO) && defined(CONFIG_USBD_HID_TOUCHPAD)
#include "usb_hid_touchpad.h"

__overridable void set_touchpad_report(struct usb_hid_touchpad_report *report)
{
}
#endif /* defined(CONFIG_CROS_EC_RO) && defined(CONFIG_USBD_HID_TOUCHPAD) */

int request_usb_wake(void)
{
	if (IS_ENABLED(CONFIG_USB_DEVICE_REMOTE_WAKEUP)) {
		return usbd_wakeup_request(&usb_device);
	}
	return -ENOTSUP;
}

int usb_msg_callback_register(msg_callback_t callback)
{
	if (msg_manager.callback_count == msg_manager.max_callbacks) {
		msg_manager.max_callbacks++;
		msg_manager.callbacks = (msg_callback_t *)realloc(
			msg_manager.callbacks,
			msg_manager.max_callbacks * sizeof(msg_callback_t));
		if (msg_manager.callbacks == NULL) {
			LOG_ERR("failed to allocate usb message callback memory");
			return -ENOMEM;
		}
	}
	msg_manager.callbacks[msg_manager.callback_count++] = callback;
	return 0;
}

static void usb_device_msg_cb(struct usbd_context *const ctx,
			      const struct usbd_msg *msg)
{
	LOG_INF("usb message: %s", usbd_msg_type_string(msg->type));

	/* broadcast usb event */
	for (int i = 0; i < msg_manager.callback_count; i++) {
		if (msg_manager.callbacks[i]) {
			msg_manager.callbacks[i](msg->type);
		}
	}
}

static int register_fs_classes(struct usbd_context *ctx)
{
	int err = 0;

	STRUCT_SECTION_FOREACH_ALTERNATE(usbd_class_fs, usbd_class_node, c_nd)
	{
		/* Pull everything that is enabled in our configuration. */
		err = usbd_register_class(ctx, c_nd->c_data->name,
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

static int usb_device_init(void)
{
	int err;

	err = usbd_add_descriptor(&usb_device, &lang);
	if (err) {
		LOG_ERR("Failed to initialize language descriptor (%d)", err);
		goto error;
	}

	err = usbd_add_descriptor(&usb_device, &mfr);
	if (err) {
		LOG_ERR("Failed to initialize manufacturer descriptor (%d)",
			err);
		goto error;
	}

	err = usbd_add_descriptor(&usb_device, &product);
	if (err) {
		LOG_ERR("Failed to initialize product descriptor (%d)", err);
		goto error;
	}

	err = usbd_add_descriptor(&usb_device, &sn);
	if (err) {
		LOG_ERR("Failed to initialize SN descriptor (%d)", err);
		goto error;
	}

	enum usbd_speed speed = usbd_caps_speed(&usb_device);

	err = usbd_add_configuration(&usb_device, speed, &usb_device_fs_config);
	if (err) {
		LOG_ERR("Failed to add configuration (%d)", err);
		goto error;
	}

	switch (speed) {
	case USBD_SPEED_FS:
		err = register_fs_classes(&usb_device);
		break;
	case USBD_SPEED_HS:
		LOG_ERR("unsupported usb high-speed");
		err = -ENOTSUP;
		break;
	default:
		LOG_ERR("unknown usb speed(%d)", speed);
		err = -ENOTSUP;
		break;
	};
	if (err) {
		goto error;
	}

	err = usbd_device_set_code_triple(&usb_device, speed, 0, 0, 0);
	if (err) {
		LOG_ERR("Failed to set descriptor code");
		goto error;
	}

	err = usbd_msg_register_cb(&usb_device, usb_device_msg_cb);
	if (err) {
		LOG_ERR("Failed to register message callback");
		goto error;
	}

	err = usbd_init(&usb_device);
	if (err) {
		LOG_ERR("Failed to initialize device support");
		goto error;
	}

	err = usbd_enable(&usb_device);
	if (err) {
		LOG_ERR("Failed to enable device support");
		goto error;
	}

	return 0;

error:
	if (msg_manager.callbacks) {
		free(msg_manager.callbacks);
		msg_manager.callbacks = NULL;
	}
	return err;
}
SYS_INIT(usb_device_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
