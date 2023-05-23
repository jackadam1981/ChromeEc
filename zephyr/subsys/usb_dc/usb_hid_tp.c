/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "clock.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "link_defs.h"
#include "queue.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(usb_hid_tp, LOG_LEVEL_INF);

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ##args)

#define USB_HID_TP_TIMESTAMP_UNIT 100 /* usec */

#define REPORT_ID_TOUCHPAD 0x01
#define REPORT_ID_DEVICE_CAPS 0x0A
#define REPORT_ID_DEVICE_CERT 0x0B

#define MAX_FINGERS 5

struct usb_hid_touchpad_report {
	uint8_t id; /* 0x01 */
	struct {
		uint16_t confidence : 1;
		uint16_t tip : 1;
		uint16_t inrange : 1;
		uint16_t id : 4;
		uint16_t pressure : 9;
		uint16_t width : 12;
		uint16_t height : 12;
		uint16_t x : 12;
		uint16_t y : 12;
	} __packed finger[MAX_FINGERS];
	uint8_t count : 7;
	uint8_t button : 1;
	uint16_t timestamp;
} __packed;

static const struct device *hid_dev;
static const int touchpad_debug;
static struct queue const report_queue = QUEUE_NULL(8, struct usb_hid_touchpad_report);
static struct k_mutex *report_queue_mutex;

#define HID_TOUCHPAD_REPORT_SIZE sizeof(struct usb_hid_touchpad_report)

/* Touchpad interface, firmware size and physical dimension. */
#define CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X 3340
#define CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y 1811
#define CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_PRESSURE 511
#define CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X 950 /* tenth of mm */
#define CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y 480 /* tenth of mm */

/* Discard TP events older than this time */
#define EVENT_DISCARD_MAX_TIME (1 * SECOND)

#define FINGER_USAGE                                                           \
	0x05, 0x0D, /*   Usage Page (Digitizer) */                             \
		0x09, 0x22, /*   Usage (Finger) */                             \
		0xA1, 0x02, /*   Collection (Logical) */                       \
		0x09, 0x47, /*     Usage (Confidence) */                       \
		0x09, 0x42, /*     Usage (Tip Switch) */                       \
		0x09, 0x32, /*     Usage (In Range) */                         \
		0x15, 0x00, /*     Logical Minimum (0) */                      \
		0x25, 0x01, /*     Logical Maximum (1) */                      \
		0x75, 0x01, /*     Report Size (1) */                          \
		0x95, 0x03, /*     Report Count (3) */                         \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0x09, 0x51, /*     Usage (0x51) Contact identifier */          \
		0x75, 0x04, /*     Report Size (4) */                          \
		0x95, 0x01, /*     Report Count (1) */                         \
		0x25, 0x0F, /*     Logical Maximum (15) */                     \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0x05, 0x0D, /*     Usage Page (Digitizer) */ /*     Logical    \
								Maximum of     \
								Pressure */    \
		0x26, (CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_PRESSURE & 0xFF),   \
		(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_PRESSURE >> 8), 0x75,     \
		0x09, /*     Report Size (9) */                                \
		0x09, 0x30, /*     Usage (Tip pressure) */                     \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0x26, 0xFF, 0x0F, /*     Logical Maximum (4095) */             \
		0x75, 0x0C, /*     Report Size (12) */                         \
		0x09, 0x48, /*     Usage (WIDTH) */                            \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0x09, 0x49, /*     Usage (HEIGHT) */                           \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0x05, 0x01, /*     Usage Page (Generic Desktop Ctrls) */       \
		0x75, 0x0C, /*     Report Size (12) */                         \
		0x55, 0x0E, /*     Unit Exponent (-2) */                       \
		0x65, 0x11, /*     Unit (System: SI Linear, Length: cm) */     \
		0x09, 0x30, /*     Usage (X) */                                \
		0x35, 0x00, /*     Physical Minimum (0) */                     \
		0x26, (CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X & 0xff),          \
		(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X >> 8), /*     Logical   \
								 Maximum */    \
		0x46, (CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X & 0xff),         \
		(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X >> 8), /*     Physical \
								  Maximum      \
								  (tenth of    \
								  mm) */       \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0x26, (CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y & 0xff),          \
		(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y >> 8), /*     Logical   \
								 Maximum */    \
		0x46, (CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y & 0xff),         \
		(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y >> 8), /*     Physical \
								  Maximum      \
								  (tenth of    \
								  mm) */       \
		0x09, 0x31, /*     Usage (Y) */                                \
		0x81, 0x02, /*     Input (Data,Var,Abs) */                     \
		0xC0 /*   End Collection */

/*
 * HID: Report Descriptor
 * TODO(b/35582031): There are ways to reduce flash usage, as the
 * Finger Usage is repeated 5 times.
 */
static const uint8_t report_desc[] = {
	/* Touchpad Collection */
	0x05, 0x0D, /* Usage Page (Digitizer) */
	0x09, 0x05, /* Usage (Touch Pad) */
	0xA1, 0x01, /* Collection (Application) */
	0x85, REPORT_ID_TOUCHPAD, /*   Report ID (1, Touch) */
	/* Finger 0 */
	FINGER_USAGE,
	/* Finger 1 */
	FINGER_USAGE,
	/* Finger 2 */
	FINGER_USAGE,
	/* Finger 3 */
	FINGER_USAGE,
	/* Finger 4 */
	FINGER_USAGE,
	/* Contact count */
	0x05, 0x0D, /*   Usage Page (Digitizer) */
	0x09, 0x54, /*   Usage (Contact count) */
	0x25, MAX_FINGERS, /*   Logical Maximum (MAX_FINGERS) */
	0x75, 0x07, /*   Report Size (7) */
	0x95, 0x01, /*   Report Count (1) */
	0x81, 0x02, /*   Input (Data,Var,Abs) */
	/* Button */
	0x05, 0x01, /*   Usage Page(Generic Desktop Ctrls) */
	0x05, 0x09, /*   Usage (Button) */
	0x19, 0x01, /*   Usage Minimum (0x01) */
	0x29, 0x01, /*   Usage Maximum (0x01) */
	0x15, 0x00, /*   Logical Minimum (0) */
	0x25, 0x01, /*   Logical Maximum (1) */
	0x75, 0x01, /*   Report Size (1) */
	0x95, 0x01, /*   Report Count (1) */
	0x81, 0x02, /*   Input (Data,Var,Abs) */
	/* Timestamp */
	0x05, 0x0D, /*   Usage Page (Digitizer) */
	0x55, 0x0C, /*   Unit Exponent (-4) */
	0x66, 0x01, 0x10, /*   Unit (Seconds) */
	0x47, 0xFF, 0xFF, 0x00, 0x00, /*   Physical Maximum (65535) */
	0x27, 0xFF, 0xFF, 0x00, 0x00, /*   Logical Maximum (65535) */
	0x75, 0x10, /*   Report Size (16) */
	0x95, 0x01, /*   Report Count (1) */
	0x09, 0x56, /*   Usage (0x56, Relative Scan Time) */
	0x81, 0x02, /*   Input (Data,Var,Abs) */

	0x85, REPORT_ID_DEVICE_CAPS, /*   Report ID (Device Capabilities) */
	0x09, 0x55, /*   Usage (Contact Count Maximum) */
	0x09, 0x59, /*   Usage (Pad Type) */
	0x25, 0x0F, /*   Logical Maximum (15) */
	0x75, 0x08, /*   Report Size (8) */
	0x95, 0x02, /*   Report Count (2) */
	0xB1, 0x02, /*   Feature (Data,Var,Abs) */

	/* Page 0xFF, usage 0xC5 is device certificate. */
	0x06, 0x00, 0xFF, /*   Usage Page (Vendor Defined) */
	0x85, REPORT_ID_DEVICE_CERT, /*   Report ID (Device Certification) */
	0x09, 0xC5, /*   Usage (Vendor Usage 0xC5) */
	0x15, 0x00, /*   Logical Minimum (0) */
	0x26, 0xFF, 0x00, /*   Logical Maximum (255) */
	0x75, 0x08, /*   Report Size (8) */
	0x96, 0x00, 0x01, /*   Report Count (256) */
	0xB1, 0x02, /*   Feature (Data,Var,Abs) */

	0xC0, /* End Collection */
};

bool check_usb_is_suspended(void)
{
	/* TODO */
	return false;
}

void request_usb_wake(void)
{
	/* TODO */
}

struct tp_report {
	uint8_t report_id;
	uint8_t button;
	int16_t x;
	int16_t y;
	uint16_t pressure;
	uint8_t reserved[3];
};
/*
 * Write a report to EP, must be called with queue mutex held, and caller
 * must first check that EP is not busy.
 */
static void write_tp_report(struct usb_hid_touchpad_report *report)
{
	LOG_INF("[%5d][%d][%d]:(btn:%x, X:%4d, Y:%4d, Pressure:%d\r\n",
	report->timestamp, report->id, report->count, report->button,
	report->finger->x, report->finger->y, report->finger->pressure);

	struct tp_report tp_rep_inst = {
		.report_id = 0,
		.button = 0,
		.x = 0,
		.y = 0,
		.pressure = 0
	};

	struct tp_report *tp_rep = &tp_rep_inst;

	tp_rep->report_id = report->id;
	tp_rep->button = report->button;
	tp_rep->pressure = report->finger->pressure;
	tp_rep->x = report->finger->x;
	tp_rep->y = report->finger->y;

	/* Write TouchPad Report */
	int ret = hid_int_ep_write(hid_dev, (uint8_t *)tp_rep,
			sizeof(tp_rep), NULL);
	if (ret) {
		LOG_ERR("HID write error\n");
	}

	request_usb_wake();
}


static void hid_tp_proc_queue(void);
DECLARE_DEFERRED(hid_tp_proc_queue);

static void hid_tp_proc_queue(void)
{
	struct usb_hid_touchpad_report report;
	uint16_t now;
	int trimming = 0;

	mutex_lock(report_queue_mutex);

	/* EP is busy, or nothing in queue: do nothing. */
	if (queue_count(&report_queue) == 0)
		goto unlock;

	now = __hw_clock_source_read() / USB_HID_TP_TIMESTAMP_UNIT;

	if (check_usb_is_suspended()) {

		request_usb_wake();

		/* Let's trim old events from the queue, if any. */
		trimming = 1;
	} else {
		hook_call_deferred(&hid_tp_proc_queue_data, -1);
	}

	if (touchpad_debug)
		LOG_DBG("TPQ t=%d (%d)", trimming, queue_count(&report_queue));

	while (queue_count(&report_queue) > 0) {
		int delta;

		queue_peek_units(&report_queue, &report, 0, 1);

		delta = (int)((uint16_t)(now - report.timestamp)) *
			USB_HID_TP_TIMESTAMP_UNIT;

		if (touchpad_debug)
			LOG_DBG("evt t=%d d=%d", report.timestamp, delta);

		/* Drop old events */
		if (delta > EVENT_DISCARD_MAX_TIME) {
			queue_advance_head(&report_queue, 1);
			continue;
		}

		if (trimming) {
			/*
			 * If we stil fail to resume, this will discard the
			 * event after the timeout expires.
			 */
			hook_call_deferred(&hid_tp_proc_queue_data,
					   EVENT_DISCARD_MAX_TIME - delta);
		} else {
			queue_advance_head(&report_queue, 1);
			write_tp_report(&report);
		}
		break;
	}

unlock:
	mutex_unlock(report_queue_mutex);
}

void set_touchpad_report(struct usb_hid_touchpad_report *report)
{
	static int print_full = 1;

	mutex_lock(report_queue_mutex);

	/* USB/EP ready and nothing in queue, just write the report. */
	if (!check_usb_is_suspended() && (queue_count(&report_queue) == 0)) {
		write_tp_report(report);
		mutex_unlock(report_queue_mutex);
		return;
	}

	/* Else add to queue, dropping oldest event if needed. */
	if (touchpad_debug)
		LOG_DBG("sTP t=%d", report->timestamp);
	if (queue_is_full(&report_queue)) {
		if (print_full)
			LOG_WRN("TP queue full\n");
		print_full = 0;

		queue_advance_head(&report_queue, 1);
	} else {
		print_full = 1;
	}
	queue_add_unit(&report_queue, report);

	mutex_unlock(report_queue_mutex);

	hid_tp_proc_queue();
}

static enum usb_dc_status_code usb_status;

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	usb_status = status;
}

static int usb_hid_tp_init(void)
{
	hid_dev = device_get_binding("HID_0");

	if (hid_dev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return 0;
	}

	usb_hid_register_device(hid_dev,
				report_desc, sizeof(report_desc),
				NULL);

	usb_hid_init(hid_dev);

	int ret = usb_enable(status_cb);

	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return -1;
	}

	return 0;

}
SYS_INIT(usb_hid_tp_init, APPLICATION, 1);
