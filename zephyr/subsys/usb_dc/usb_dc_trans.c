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

#define USB_HID_SAMPLE_TEST

#ifdef USB_HID_SAMPLE_TEST
static bool configured;
static const struct device *hdev;
static struct k_work report_send;
static ATOMIC_DEFINE(hid_ep_in_busy, 1);

#define HID_EP_BUSY_FLAG	0
#define REPORT_ID_1		0x01
#define REPORT_PERIOD		K_SECONDS(2)

static struct report {
	uint8_t id;
	uint8_t value;
} __packed report_1 = {
	.id = REPORT_ID_1,
	.value = 0,
};

static void report_event_handler(struct k_timer *dummy);
static K_TIMER_DEFINE(event_timer, report_event_handler, NULL);

/*
 * Simple HID Report Descriptor
 * Report ID is present for completeness, although it can be omitted.
 * Output of "usbhid-dump -d 2fe3:0006 -e descriptor":
 *  05 01 09 00 A1 01 15 00    26 FF 00 85 01 75 08 95
 *  01 09 00 81 02 C0
*/
#if 0
static const uint8_t hid_report_desc[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
	HID_LOGICAL_MIN8(0x00),
	HID_LOGICAL_MAX16(0xFF, 0x00),
	HID_REPORT_ID(REPORT_ID_1),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(1),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
	HID_INPUT(0x02),
	HID_END_COLLECTION,
};
#else
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
#endif

#if 0
static void send_report(struct k_work *work)
{
	int ret, wrote;

	if (!atomic_test_and_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
		ret = hid_int_ep_write(hdev, (uint8_t *)&report_1,
				       sizeof(report_1), &wrote);
		if (ret != 0) {
			/*
			 * Do nothing and wait until host has reset the device
			 * and hid_ep_in_busy is cleared.
			 */
			LOG_ERR("Failed to submit report");
		} else {
			LOG_DBG("Report submitted");
		}
	} else {
		LOG_DBG("HID IN endpoint busy");
	}
}
#endif

static void int_in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);
	if (!atomic_test_and_clear_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
		LOG_WRN("IN endpoint callback without preceding buffer write");
	}
}

/*
 * On Idle callback is available here as an example even if actual use is
 * very limited. In contrast to report_event_handler(),
 * report value is not incremented here.
 */
static void on_idle_cb(const struct device *dev, uint16_t report_id)
{
	LOG_DBG("On idle callback");
	k_work_submit(&report_send);
}

static void report_event_handler(struct k_timer *dummy)
{
	/* Increment reported data */
	report_1.value++;
	k_work_submit(&report_send);
}

static void protocol_cb(const struct device *dev, uint8_t protocol)
{
	LOG_INF("New protocol: %s", protocol == HID_PROTOCOL_BOOT ?
		"boot" : "report");
}

static const struct hid_ops ops = {
	.int_in_ready = int_in_ready_cb,
	.on_idle = on_idle_cb,
	.protocol_change = protocol_cb,
};

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	switch (status) {
	case USB_DC_RESET:
		configured = false;
		break;
	case USB_DC_CONFIGURED:
		if (!configured) {
			int_in_ready_cb(hdev);
			configured = true;
		}
		break;
	case USB_DC_SOF:
		break;
	default:
		LOG_DBG("status %u unhandled", status);
		break;
	}
}

#endif /* #ifdef USB_HID_SAMPLE_TEST */


#ifdef USB_KEYCODE_TRANS

#define MAX_KEYS_PRESSED 6

static struct hid_keyboard_report {
    uint8_t modifier_keys;
    uint8_t reserved;
    uint8_t keys[MAX_KEYS_PRESSED];
} __packed hid_report;


/* The standard Chrome OS keyboard matrix table. See HUT 1.12v2 Table 12 and
 * https://www.w3.org/TR/DOM-Level-3-Events-code .
 *
 * Assistant key is mapped as 0xf0, but this key code is never actually send.
 */
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

static void queue_hid_report(void) {
	/* Queue the HID report to be sent to the USB host */
	/**/
	usb_hid_tx(USB_IFACE_HID_KEYBOARD,
		(uint8_t*)&hid_report,
		sizeof(hid_report));
	/**/
}

#if 1
void keyboard_state_changed(int row, int col, int is_pressed)
{
	uint8_t keycode = keycodes[col][row];

	if (!keycode) {
		printk("Unknown key at %d/%d\n", row, col);
		return;
	}

	if (is_pressed) {
		/* Add the key to the HID report if it's not already present */
		printk("Key pressed\n", row, col);
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
		printk("Key released\n", row, col);
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
	const struct device *hid_dev;

	hid_dev = device_get_binding("HID_0");

	uint8_t keycode = keycodes[col][row];

	if (!keycode) {
		LOG_INF("Unknown key at %d/%d\n", row, col);
		return;
	}

	LOG_INF("Key pressed keycode:%x pressed:%d\n", keycode, is_pressed);

	memset(&report, 0, sizeof(report));
	if (is_pressed) {
		report[2] = keycode;
	} else {
		report[2] = 0x00;
	}

	ret = hid_int_ep_write(hid_dev, report, sizeof(report), NULL);
	if (ret) {
		LOG_INF("HID write error, %d", ret);
	}

	//queue_keycode_event(keycode, is_pressed);
}

#endif /* #if 0 */

#endif /* #ifdef USB_KEYCODE_TRANS */

/* Initialize power sequence system state */
static int usb_dc_trans_init(void)
{

#ifdef USB_HID_SAMPLE_TEST


	int ret;

	LOG_INF("Starting application");

	hdev = device_get_binding("HID_0");
	if (hdev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return -ENODEV;
	}

	LOG_INF("HID Device: dev %p", hdev);

	usb_hid_register_device(hdev, hid_report_desc, sizeof(hid_report_desc),
				&ops);

	//atomic_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG);
	//k_timer_start(&event_timer, REPORT_PERIOD, REPORT_PERIOD);

	if (usb_hid_set_proto_code(hdev, HID_BOOT_IFACE_CODE_NONE)) {
		LOG_WRN("Failed to set Protocol Code");
	}

	usb_hid_init(hdev);

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	//k_work_init(&report_send, send_report);
#endif /* #ifdef USB_HID_SAMPLE_TEST */
	return 0;
}

/*
 * The initialization must occur after system I/O initialization that
 * the signals depend upon, such as GPIO, ADC etc.
 */
SYS_INIT(usb_dc_trans_init, APPLICATION, 1);

