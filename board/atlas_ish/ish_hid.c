/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ish_hid.h"
#include "hid_device.h"
#include "hid.h"
#include "util.h"

#define GOOGLE_VENDOR_ID		0x18d1
#define GOOGLE_CENTROIDING_PRODUCT_ID	0x5028

/* Register definition */
#define HID_DESC_REGISTER		0x0001
#define REPORT_DESC_REGISTER		0x1000
#define INPUT_REPORT_REGISTER		0x2000
#define COMMAND_REGISTER		0x3000
#define DATA_REGISTER			0x3000

/* Report ID */
#define REPORT_ID_TOUCH			0x01
#define REPORT_ID_MOUSE			0x02
#define REPORT_ID_DEVICE_CAPS		0x0A
#define REPORT_ID_DEVICE_CERT		0x0B
#define REPORT_ID_INPUT_MODE		0x0C
#define REPORT_ID_REPORTING		0x0D

#define INPUT_MODE_MOUSE		0x00
#define INPUT_MODE_TOUCH		0x03

#define ISH_HID_HEADER_SIZE		1

static const uint8_t report_desc[] = {
	/* Mouse Collection */
	0x05, 0x01,			/* Usage Page (Generic Desktop) */
	0x09, 0x02,			/* Usage (Mouse) */
	0xA1, 0x01,			/* Collection (Application) */
	0x85, REPORT_ID_MOUSE,		/* Report ID (Mouse) */
	0x09, 0x01,			/*   Usage (Pointer) */
	0xA1, 0x00,			/*   Collection (Physical) */
	0x05, 0x09,			/*     Usage Page (Button) */
	0x19, 0x01,			/*     Usage Minimum (Button 1) */
	0x29, 0x02,			/*     Usage Maximum (Button 2) */
	0x15, 0x00,			/*     Logical Minimum (0) */
	0x25, 0x01,			/*     Logical Maximum (1) */
	0x75, 0x01,			/*     Report Size (1) */
	0x95, 0x02,			/*     Report Count (2) */
	0x81, 0x02,			/*     Input (Data,Var,Abs) */
	0x95, 0x06,			/*     Report Count (6) */
	0x81, 0x03,			/*     Input (Cnst,Var,Abs) */
	0x05, 0x01,			/*     Usage Page (Generic Desktop) */
	0x09, 0x30,			/*     Usage (X) */
	0x09, 0x31,			/*     Usage (Y) */
	0x15, 0x81,			/*     Logical Minimum (-127) */
	0x25, 0x7F,			/*     Logical Maximum (127) */
	0x75, 0x08,			/*     Report Size (8) */
	0x95, 0x02,			/*     Report Count (2) */
	0x81, 0x06,			/*     Input (Data,Var,Rel) */
	0xC0,				/*   End Collection */
	0xC0,				/* End Collection */

	/* Touchpad Collection */
	0x05, 0x0D,			/* Usage Page (Digitizer) */
	0x09, 0x05,			/* Usage (Touch Pad) */
	0xA1, 0x01,			/* Collection (Application) */
	0x85, REPORT_ID_TOUCH,		/*   Report ID (Touch) */

	/* Button */
	0x05, 0x09,			/*   Usage Page (Button) */
	0x19, 0x01,			/*   Usage Minimum (0x01) */
	0x29, 0x01,			/*   Usage Maximum (0x01) */
	0x15, 0x00,			/*   Logical Minimum (0) */
	0x25, 0x01,			/*   Logical Maximum (1) */
	0x75, 0x01,			/*   Report Size (1) */
	0x95, 0x01,			/*   Report Count (1) */
	0x81, 0x02,			/*   Input (Data,Var,Abs) */

	/* Contact count */
	0x05, 0x0D,			/*   Usage Page (Digitizer) */
	0x09, 0x54,			/*   Usage (Contact count) */
	0x25, MAX_FINGERS,		/*   Logical Maximum (MAX_FINGERS) */
	0x75, 0x07,			/*   Report Size (7) */
	0x95, 0x01,			/*   Report Count (1) */
	0x81, 0x02,			/*   Input (Data,Var,Abs) */

	/* Scan time */
	0x55, 0x0C,			/*   Unit Exponent (-4) */
	0x66, 0x01, 0x10,		/*   Unit (Seconds) */
	0x47, 0xFF, 0xFF, 0x00, 0x00,	/*   Physical Maximum (65535) */
	0x27, 0xFF, 0xFF, 0x00, 0x00,	/*   Logical Maximum (65535) */
	0x75, 0x10,			/*   Report Size (16) */
	0x95, 0x01,			/*   Report Count (1) */
	0x05, 0x0D,			/*   Usage Page (Digitizers) */
	0x09, 0x56,			/*   Usage (Scan Time) */
	0x81, 0x02,			/*   Input (Data,Var,Abs) */

#define FINGER(FINGER_NUMBER)                                                 \
	/* Finger FINGER_NUMBER */                                            \
	0x05, 0x0D,			/*   Usage Page (Digitizer) */         \
	0x09, 0x22,			/*   Usage (Finger) */                 \
	0xA1, 0x02,			/*   Collection (Logical) */           \
	0x09, 0x47,			/*     Usage (Confidence) */           \
	0x09, 0x42,			/*     Usage (Tip Switch) */           \
	0x09, 0x32,			/*     Usage (In Range) */             \
	0x15, 0x00,			/*     Logical Minimum (0) */          \
	0x25, 0x01,			/*     Logical Maximum (1) */          \
	0x75, 0x01,			/*     Report Size (1) */              \
	0x95, 0x03,			/*     Report Count (3) */             \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x09, 0x51,			/*     Usage (Contact identifier) */   \
	0x25, 0x1F,			/*     Logical Maximum (31) */         \
	0x75, 0x05,			/*     Report Size (5) */              \
	0x95, 0x01,			/*     Report Count (1) */             \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x05, 0x01,			/*     Usage Page (Generic Desktop) */ \
	0x09, 0x30,			/*     Usage (X) */                    \
	0x55, 0x0E,			/*     Unit Exponent (-2) */           \
	0x65, 0x11,			/*     Unit (SI Linear, Length: cm) */ \
	0x35, 0x00,			/*     Physical Minimum (0) */         \
	0x46, 0x06, 0x04,		/*     Physical Maximum (10.3 cm) */   \
	0x26, 0x80, 0x33,		/*     Logical Maximum (13184) */      \
	0x75, 0x10,			/*     Report Size (16) */             \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x09, 0x31,			/*     Usage (Y) */                    \
	0x46, 0xA8, 0x02,		/*     Physical Maximum (6.8 cm) */    \
	0x26, 0x00, 0x22,		/*     Logical Maximum (8704) */       \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x05, 0x0D,			/*     Usage Page (Digitizer) */       \
	0x26, 0x80, 0x33,		/*     Logical Maximum (13184) */      \
	0x09, 0x48,			/*     Usage (Width) */                \
	0x75, 0x10,			/*     Report Size (16) */             \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x26, 0x00, 0x22,		/*     Logical Maximum (8704) */       \
	0x09, 0x49,			/*     Usage (Height) */               \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x09, 0x30,			/*     Usage (Tip pressure) */         \
	0x26, 0xFF, 0x00,		/*     Logical Maximum (255) */        \
	0x75, 0x08,			/*     Report Size (8) */              \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0x09, 0x3f,			/*     Usage (Azimuth Orientation) */  \
	0x16, 0x00, 0x00,		/*     Logical Minimum (0) */          \
	0x26, 0x68, 0x01,		/*     Logical Maximum (360) */        \
	0x75, 0x10,			/*     Report Size (16) */             \
	0x81, 0x02,			/*     Input (Data,Var,Abs) */         \
	0xC0,				/*   End Collection */

	FINGER(1)
	FINGER(2)
	FINGER(3)
	FINGER(4)
	FINGER(5)

#undef FINGER

	0x05, 0x0D,			/*   Usage Page (Digitizer) */
	0x85, REPORT_ID_DEVICE_CAPS,	/*   Report ID (Device Capabilities) */
	0x09, 0x55,			/*   Usage (Contact Count Maximum) */
	0x09, 0x59,			/*   Usage (Pad Type) */
	0x75, 0x08,			/*   Report Size (8) */
	0x95, 0x02,			/*   Report Count (2) */
	0x25, 0x0F,			/*   Logical Maximum (15) */
	0xB1, 0x02,			/*   Feature (Data,Var,Abs) */
	0x06, 0x00, 0xFF,		/*   Usage Page (Vendor Defined) */
	0x85, REPORT_ID_DEVICE_CERT,	/*   Report ID (Device Certification) */
	0x09, 0xC5,			/*   Usage (Vendor Usage 0xC5) */
	0x15, 0x00,			/*   Logical Minimum (0) */
	0x26, 0xFF, 0x00,		/*   Logical Maximum (255) */
	0x75, 0x08,			/*   Report Size (8) */
	0x96, 0x00, 0x01,		/*   Report Count (256) */
	0xB1, 0x02,			/*   Feature (Data,Var,Abs) */
	0xC0,				/* End Collection */

	/* Configuration Collection */
	0x05, 0x0D,			/* Usage Page (Digitizer) */
	0x09, 0x0E,			/* Usage (Configuration) */
	0xA1, 0x01,			/* Collection (Application) */
	0x85, REPORT_ID_INPUT_MODE,	/*   Report ID (Input Mode) */
	0x09, 0x22,			/*   Usage (Finger) */
	0xA1, 0x02,			/*   Collection (Logical) */
	0x09, 0x52,			/*     Usage (Input Mode) */
	0x15, 0x00,			/*     Logical Minimum (0) */
	0x25, 0x0F,			/*     Logical Maximum (15) */
	0x75, 0x08,			/*     Report Size (8) */
	0x95, 0x01,			/*     Report Count (1) */
	0xB1, 0x02,			/*     Feature (Data,Var,Abs) */
	0xC0,				/*   End Collection */
	0x09, 0x22,			/*   Usage (Finger) */
	0xA1, 0x00,			/*   Collection (Physical) */
	0x85, REPORT_ID_REPORTING,	/*     Report ID(Selective Reporting) */
	0x09, 0x57,			/*     Usage (Surface Switch) */
	0x09, 0x58,			/*     Usage (Button Switch) */
	0x75, 0x04,			/*     Report Size (4) */
	0x95, 0x02,			/*     Report Count (2) */
	0x25, 0x01,			/*     Logical Maximum (1) */
	0xB1, 0x02,			/*     Feature (Data,Var,Abs) */
	0xC0,				/*   End Collection */
	0xC0,				/* End Collection */
};

static const uint8_t device_caps[] = {
	MAX_FINGERS,	/* Contact Count Maximum */
	0x00,		/* Pad Type: Depressible click-pad */
};

/* A 256-byte default blob for the 'device certification status' feature report
 * expected by Windows.
 */
static const uint8_t device_cert[] = {
	0xFC, 0x28, 0xFE, 0x84, 0x40, 0xCB, 0x9A, 0x87,
	0x0D, 0xBE, 0x57, 0x3C, 0xB6, 0x70, 0x09, 0x88,
	0x07, 0x97, 0x2D, 0x2B, 0xE3, 0x38, 0x34, 0xB6,
	0x6C, 0xED, 0xB0, 0xF7, 0xE5, 0x9C, 0xF6, 0xC2,
	0x2E, 0x84, 0x1B, 0xE8, 0xB4, 0x51, 0x78, 0x43,
	0x1F, 0x28, 0x4B, 0x7C, 0x2D, 0x53, 0xAF, 0xFC,
	0x47, 0x70, 0x1B, 0x59, 0x6F, 0x74, 0x43, 0xC4,
	0xF3, 0x47, 0x18, 0x53, 0x1A, 0xA2, 0xA1, 0x71,
	0xC7, 0x95, 0x0E, 0x31, 0x55, 0x21, 0xD3, 0xB5,
	0x1E, 0xE9, 0x0C, 0xBA, 0xEC, 0xB8, 0x89, 0x19,
	0x3E, 0xB3, 0xAF, 0x75, 0x81, 0x9D, 0x53, 0xB9,
	0x41, 0x57, 0xF4, 0x6D, 0x39, 0x25, 0x29, 0x7C,
	0x87, 0xD9, 0xB4, 0x98, 0x45, 0x7D, 0xA7, 0x26,
	0x9C, 0x65, 0x3B, 0x85, 0x68, 0x89, 0xD7, 0x3B,
	0xBD, 0xFF, 0x14, 0x67, 0xF2, 0x2B, 0xF0, 0x2A,
	0x41, 0x54, 0xF0, 0xFD, 0x2C, 0x66, 0x7C, 0xF8,
	0xC0, 0x8F, 0x33, 0x13, 0x03, 0xF1, 0xD3, 0xC1,
	0x0B, 0x89, 0xD9, 0x1B, 0x62, 0xCD, 0x51, 0xB7,
	0x80, 0xB8, 0xAF, 0x3A, 0x10, 0xC1, 0x8A, 0x5B,
	0xE8, 0x8A, 0x56, 0xF0, 0x8C, 0xAA, 0xFA, 0x35,
	0xE9, 0x42, 0xC4, 0xD8, 0x55, 0xC3, 0x38, 0xCC,
	0x2B, 0x53, 0x5C, 0x69, 0x52, 0xD5, 0xC8, 0x73,
	0x02, 0x38, 0x7C, 0x73, 0xB6, 0x41, 0xE7, 0xFF,
	0x05, 0xD8, 0x2B, 0x79, 0x9A, 0xE2, 0x34, 0x60,
	0x8F, 0xA3, 0x32, 0x1F, 0x09, 0x78, 0x62, 0xBC,
	0x80, 0xE3, 0x0F, 0xBD, 0x65, 0x20, 0x08, 0x13,
	0xC1, 0xE2, 0xEE, 0x53, 0x2D, 0x86, 0x7E, 0xA7,
	0x5A, 0xC5, 0xD3, 0x7D, 0x98, 0xBE, 0x31, 0x48,
	0x1F, 0xFB, 0xDA, 0xAF, 0xA2, 0xA8, 0x6A, 0x89,
	0xD6, 0xBF, 0xF2, 0xD3, 0x32, 0x2A, 0x9A, 0xE4,
	0xCF, 0x17, 0xB7, 0xB8, 0xF4, 0xE1, 0x33, 0x08,
	0x24, 0x8B, 0xC4, 0x43, 0xA5, 0xE5, 0x24, 0xC2,
};


static uint8_t host_ready;
static uint8_t input_mode;

/* TODO(b/70681946): Selectively report surface contact and button state in
 * input reports based on |reporting.surface_switch| and
 * |reporting.button_switch|, respectively.
 */
struct selective_reporting {
	uint8_t surface_switch:4;
	uint8_t button_switch:4;
} __packed;

static struct selective_reporting reporting;

static int ish_hid_handle;

#define MAX_SIZEOF(a, b) (sizeof(a) > sizeof(b) ? sizeof(a) : sizeof(b))

static struct hid_descriptor hid_desc = {
	.wHIDDescLength = HID_DESC_LENGTH,
	.bcdVersion = HID_BCD_VERSION,
	.wReportDescLength = sizeof(report_desc),
	.wReportDescRegister = REPORT_DESC_REGISTER,
	.wInputRegister = INPUT_REPORT_REGISTER,
	.wMaxInputLength = MAX_SIZEOF(struct touch_report, struct mouse_report),
	.wOutputRegister = 0,
	.wMaxOutputLength = 0,
	.wCommandRegister = COMMAND_REGISTER,
	.wDataRegister = DATA_REGISTER,
	.wVendorID = GOOGLE_VENDOR_ID,
	.wProductID = GOOGLE_CENTROIDING_PRODUCT_ID,
	.wVersionID = 0x6776,
};

#undef MAX_SIZEOF

static size_t fill_report(uint8_t *buffer, uint8_t report_id, const void *data,
			size_t data_len)
{
	size_t response_len = ISH_HID_HEADER_SIZE + data_len;

	buffer[0] = report_id;
	memcpy(buffer + ISH_HID_HEADER_SIZE, data, data_len);
	return response_len;
}


#define MAX_REPORT_CNT	2

struct touch_report touch_reports[MAX_REPORT_CNT];
struct mouse_report mouse_reports[MAX_REPORT_CNT];
int report_active_index;

int ish_hid_send_report(int report_index)
{
	size_t report_size;
	uint8_t *data;

	if (!host_ready)
		return 1;
	if (report_index >= MAX_REPORT_CNT)
		return 1;
	if (report_index < 0)
		report_index = report_active_index;
	if (input_mode == INPUT_MODE_TOUCH) {
		data = (uint8_t *)&(touch_reports[report_index]);
		report_size = sizeof(struct touch_report);
	} else {
		data = (uint8_t *)&(mouse_reports[report_index]);
		report_size = sizeof(struct mouse_report);
	}
	return hid_subsys_send_input_report(ish_hid_handle, data, report_size);
}

int ish_hid_initialize(int hid_handle)
{
	uint32_t i;

	ish_hid_handle = hid_handle;
	input_mode = INPUT_MODE_MOUSE;
	reporting.surface_switch = 1;
	reporting.button_switch = 1;
	for (i = 0; i < MAX_REPORT_CNT; i++) {
		touch_reports[i].rid = REPORT_ID_TOUCH;
		mouse_reports[i].rid = REPORT_ID_MOUSE;
	}
	return 0;
}

int ish_hid_get_hid_descriptor(int hid_handle, uint8_t *buf, size_t buf_size)
{
	memcpy(buf, &hid_desc, sizeof(hid_desc));
	return sizeof(hid_desc);
}

int ish_hid_get_report_descriptor(int hid_handle, uint8_t *buf, size_t buf_size)
{
	memcpy(buf, &report_desc, sizeof(report_desc));
	host_ready = 1;
	return sizeof(report_desc);
}

int ish_hid_get_feature_report(int hid_handle, uint8_t report_id, uint8_t *buf,
				size_t buf_size)
{
	size_t response_len;

	switch (report_id) {
	case REPORT_ID_TOUCH:
		memcpy(buf, &(touch_reports[report_active_index]),
			sizeof(struct touch_report));
		response_len = sizeof(struct touch_report);
		break;
	case REPORT_ID_MOUSE:
		memcpy(buf, &(mouse_reports[report_active_index]),
			sizeof(struct mouse_report));
		response_len = sizeof(struct mouse_report);
		break;
	case REPORT_ID_DEVICE_CAPS:
		response_len = fill_report(buf, report_id, &device_caps,
					sizeof(device_caps));
		break;
	case REPORT_ID_DEVICE_CERT:
		response_len = fill_report(buf, report_id, &device_cert,
					sizeof(device_cert));
		break;
	case REPORT_ID_INPUT_MODE:
		response_len = fill_report(buf, report_id, &input_mode,
					sizeof(input_mode));
		break;
	case REPORT_ID_REPORTING:
		response_len = fill_report(buf, report_id, &reporting,
					sizeof(reporting));
		break;
	default:
		response_len = 0;
		break;
	}
	return response_len;
}

int ish_hid_set_feature_report(int hid_handle, uint8_t report_id,
				const uint8_t *data, size_t data_size)
{
	switch (report_id) {
	case REPORT_ID_INPUT_MODE:
		memcpy(&input_mode, data + 1, sizeof(input_mode));
		break;
	case REPORT_ID_REPORTING:
		memcpy(&reporting, data + 1, sizeof(reporting));
		break;
	default:
		break;
	}
	return 0;
}

int ish_hid_get_input_report(int hid_handle, uint8_t report_id,
				uint8_t *buf, size_t buf_size)
{
	size_t response_len;

	if (input_mode == INPUT_MODE_TOUCH) {
		memcpy(buf, &(touch_reports[report_active_index]),
			sizeof(struct touch_report));
		response_len = sizeof(struct touch_report);
	} else {
		memcpy(buf, &mouse_reports[report_active_index],
			sizeof(struct mouse_report));
		response_len = sizeof(struct mouse_report);
	}
	return response_len;
}

static struct hid_callbacks ish_hid_cbs = {
	.initialize = ish_hid_initialize,
	.get_hid_descriptor = ish_hid_get_hid_descriptor,
	.get_report_descriptor = ish_hid_get_report_descriptor,
	.get_feature_report = ish_hid_get_feature_report,
	.set_feature_report = ish_hid_set_feature_report,
	.get_input_report = ish_hid_get_input_report,
};

static struct hid_device ish_hid_device = {
	.dev_class = 1,
	.pid = 1,
	.vid = 0,
	.cbs = &ish_hid_cbs,
};

HID_DEVICE_ENTRY(ish_hid_device);
