/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include "usb_kb_desc.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usb_dc_trans, LOG_LEVEL_INF);


static const struct device *hdev;
static enum usb_dc_status_code usb_status;

/* HID : Report Descriptor */
static const uint8_t hid_report_desc[] = {
	KEYBOARD_BASE_DESC

#ifdef KEYBOARD_VENDOR_DESC
	KEYBOARD_VENDOR_DESC
#endif

#ifdef CONFIG_USB_HID_KEYBOARD_VIVALDI
	KEYBOARD_TOP_ROW_DESC KEYBOARD_TOP_ROW_FEATURE_DESC
#endif
	0xC0 /* End Collection */
};

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	usb_status = status;
}

/* The standard Chrome OS keyboard matrix table. See HUT 1.12v2 Table 12 and
 * https://www.w3.org/TR/DOM-Level-3-Events-code .
 *
 * Assistant key is mapped as 0xf0, but this key code is never actually send.
 */

#define KEYBOARD_COLS_MAX 13
#define KEYBOARD_ROWS 8

const uint8_t keycodes[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ 0x00, 0x00, 0xe0, 0xe3, 0xe4, HID_KEYBOARD_ASSISTANT_KEY, 0x00,
	  0x00 },
	{ 0xe3, 0x29, 0x2b, 0x35, 0x04, 0x1d, 0x1e, 0x14 },
	{ 0x3a, 0x3d, 0x3c, 0x3b, 0x07, 0x06, 0x20, 0x08 },
	{ 0x05, 0x0a, 0x17, 0x22, 0x09, 0x19, 0x21, 0x15 },
	{ 0x43, 0x40, 0x3f, 0x3e, 0x16, 0x1b, 0x1f, 0x1a },
	{ 0x87, 0x00, 0x30, 0x00, 0x0e, 0x36, 0x25, 0x0c },
	{ 0x11, 0x0b, 0x1c, 0x23, 0x0d, 0x10, 0x24, 0x18 },
	{ 0x00, 0x00, 0x64, 0x00, 0x00, 0xe1, 0x00, 0xe5 },
	{ 0x2e, 0x34, 0x2F, 0x2d, 0x33, 0x38, 0x27, 0x13 },
	{ 0x00, 0x42, 0x41, 0x68, 0x0f, 0x37, 0x26, 0x12 },
	{ 0xe6, 0x00, 0x89, 0x00, 0x31, 0x00, 0xe2, 0x00 },
	{ 0x00, 0x2a, 0x00, 0x31, 0x28, 0x2c, 0x51, 0x52 },
	{ 0x00, 0x8a, 0x00, 0x8b, 0x00, 0x00, 0x4f, 0x50 },
};

#if 1
#define MAX_KEYS_PRESSED 6

static struct hid_keyboard_report {
    uint8_t modifier_keys;
    uint8_t reserved;
    uint8_t keys[MAX_KEYS_PRESSED];
} __packed hid_report;


static void queue_hid_report(void) {

	hid_int_ep_write(hdev,
		(uint8_t*)&hid_report,
		sizeof(hid_report),
		NULL);

}

void keyboard_state_changed(int row, int col, int is_pressed)
{
	uint8_t keycode = keycodes[col][row];

	if (!keycode) {
		printk("Unknown key at %d/%d\n", row, col);
		return;
	}

	if (is_pressed) {
		/* Add the key to the HID report if it's not already present */
		printk("Key pressed at %d/%d\n", row, col);
		for (int i = 0; i < MAX_KEYS_PRESSED; i++) {
			if (hid_report.keys[i] == keycode) {
			/* Key is already present in report, nothing to do */
				return;
			}

			if (hid_report.keys[i] == 0) {
			/* Found an empty slot in the report, add the key */
				hid_report.keys[i] = keycode;
				break;
			}
		}
	} else {
		/* Remove the key from the HID report */
		printk("Key released at %d/%d\n", row, col);
		for (int i = 0; i < MAX_KEYS_PRESSED; i++) {
			if (hid_report.keys[i] == keycode) {
			/* Shift remaining keys down to fill the empty slot */
				for (int j = i; j < MAX_KEYS_PRESSED - 1; j++) {
					hid_report.keys[j] =
						hid_report.keys[j + 1];
				}
				/* Clear the last slot */
				hid_report.keys[MAX_KEYS_PRESSED - 1] = 0;
				break;
			}
		}
	}
	/* Send the updated HID report */
	queue_hid_report();
}
#else
void keyboard_state_changed(int row, int col, int is_pressed)
{
	int ret;
	uint8_t report[8];
	uint8_t keycode = keycodes[col][row];

	if (!keycode) {
		LOG_INF("Unknown key at %d/%d\n", row, col);
		return;
	}

	LOG_INF("Key pressed keycode:%x pressed:%d\n", keycode, is_pressed);

	memset(&report, 0, sizeof(report));
	if (is_pressed) {
		report[2] = keycode;
	}

	ret = hid_int_ep_write(hdev, report, sizeof(report), NULL);
	if (ret) {
		LOG_INF("HID write error, %d", ret);
	}

	//queue_keycode_event(keycode, is_pressed);
}

#endif /* #if 0 */

/* Initialize power sequence system state */
static int usb_dc_trans_init(void)
{
	int ret;

	LOG_INF("Starting application");

	hdev = device_get_binding("HID_0");
	if (hdev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return -ENODEV;
	}

	LOG_INF("HID Device: dev %p", hdev);

	usb_hid_register_device(hdev, hid_report_desc, sizeof(hid_report_desc),
				NULL);

	if (usb_hid_set_proto_code(hdev, HID_BOOT_IFACE_CODE_NONE)) {
		LOG_WRN("Failed to set Protocol Code");
	}

	usb_hid_init(hdev);

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	return 0;
}

/*
 * The initialization must occur after system I/O initialization that
 * the signals depend upon, such as GPIO, ADC etc.
 */
SYS_INIT(usb_dc_trans_init, APPLICATION, 1);
