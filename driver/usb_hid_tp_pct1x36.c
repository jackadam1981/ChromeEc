/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "../subsys/usb_dc/usb_dc.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "queue.h"
#include "task.h"
#include "util.h"

#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>
LOG_MODULE_DECLARE(usb_hid_tp, LOG_LEVEL_INF);

#define TP_DEV_NAME                 \
	(CONFIG_USB_HID_DEVICE_NAME \
	 "_" STRINGIFY(CONFIG_USB_DC_TOUCHPAD_PCT1X36_HID_NUM))

#define TP_NODE DT_ALIAS(usb_hid_tp)
BUILD_ASSERT(DT_NODE_EXISTS(TP_NODE),
	     "Unsupported board: usb-hid-tp devicetree alias is not defined.");

#define TP_NODE_PIX DT_INST(0, pixart_pct1x36)
#define CONFIG_PCT1X36_GPIO_INT \
	GPIO_SIGNAL(DT_PROP(DT_PROP(TP_NODE_PIX, irq), irq_pin))
#define CONFIG_PCT1X36_I2C_SLAVE_ADDR DT_REG_ADDR(TP_NODE_PIX)
#define CONFIG_PCT1X36_I2C_PORT I2C_PORT_BY_DEV(TP_NODE_PIX)

#define CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X \
	DT_PROP_OR(TP_NODE_PIX, logical_max_x, 0)
#define CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y \
	DT_PROP_OR(TP_NODE_PIX, logical_max_y, 0)
#define CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X \
	DT_PROP_OR(TP_NODE_PIX, physical_max_x, 0)
#define CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y \
	DT_PROP_OR(TP_NODE_PIX, physical_max_y, 0)

// #define TASK_EVENT_POWER TASK_EVENT_CUSTOM_BIT(0)
#define TASK_EVENT_PCT1X36_WRITE TASK_EVENT_CUSTOM_BIT(1)

#define PCT1X36_DESC_REGISTER 0x0020
#define PCT1X36_MAX_FINGERS 0X05
#define PCT1X36_DATA_MAX_LEN 2048
#define PCT1X36_REPORT_LEN 0X2A

enum {
	// Input report ID
	PCT1X36_REPORT_ID_TOUCH_PAD = 0x01,
	PCT1X36_REPORT_ID_MOUSE_REL = 0x02,
	// Feature report ID
	PCT1X36_REPORT_ID_CONTACT_COUNT_MAX = 0x03,
	PCT1X36_REPORT_ID_BUTTON_TYPE = 0x04,
	PCT1X36_REPORT_ID_INPUT_MODE = 0x05,
	PCT1X36_REPORT_ID_SWITCH_MODE = 0x06,
	PCT1X36_REPORT_ID_LATENCY_MODE = 0x07,
	// Vendor report ID
	PCT1X36_REPORT_ID_CERT_STATUS = 0x0A,
	PCT1X36_REPORT_ID_VENDOR_STATUS = 0x0B,
	PCT1X36_REPORT_ID_BURST_REGISTER = 0x41,
	PCT1X36_REPORT_ID_REG_DATA = 0x42,
	PCT1X36_REPORT_ID_USR_DATA = 0x43,
};

enum {
	OP_CODE_RESET = 0x1,
	OP_CODE_GET_REPORT = 0x2,
	OP_CODE_SET_REPORT = 0x3,
	OP_CODE_GET_IDLE = 0x4,
	OP_CODE_SET_IDLE = 0x5,
	OP_CODE_GET_PROTOCOL = 0x6,
	OP_CODE_SET_PROTOCOL = 0x7,
	OP_CODE_SET_POWER = 0x8,
};

enum {
	REPORT_TYPE_INPUT = 1,
	REPORT_TYPE_OUTPUT = 2,
	REPORT_TYPE_FEATURE = 3,
};

typedef struct pct1x36_desc {
	uint16_t hid_desc_length;
	uint16_t version;
	uint16_t report_desc_length;
	uint16_t report_desc_register;
	uint16_t input_register;
	uint16_t max_input_length;
	uint16_t output_register;
	uint16_t max_output_length;
	uint16_t command_register;
	uint16_t data_register;
	uint16_t vendor_id;
	uint16_t product_id;
	uint16_t version_id;
	uint32_t reserved;
} __attribute__((packed)) pct1x36_desc_t;

typedef struct cmd_data {
	uint16_t reg_idx;
	uint16_t len;
	uint8_t hid_data[PCT1X36_DATA_MAX_LEN];
} __attribute__((packed)) cmd_data_t;

typedef struct pct1x36_data {
	uint16_t reg_idx;
	uint8_t id : 4;
	uint8_t type : 2;
	uint8_t reserved : 2;
	uint8_t op_code : 4;
	uint8_t reserved_1 : 4;
	union {
		cmd_data_t cmd_data;
		struct {
			uint8_t report_id;
			cmd_data_t cmd_data;
		} l;
	} payload;
} __attribute__((packed)) pct1x36_data_t;

typedef struct pct1x36_read_data {
	uint16_t len;
	uint8_t hid_data[PCT1X36_REPORT_LEN - 2];
} __attribute__((packed)) pct1x36_read_data_t;

static const struct device *hid_dev;
static struct queue const report_queue =
	QUEUE_NULL(8, struct pct1x36_read_data);
static struct k_mutex *report_queue_mutex;
static ATOMIC_DEFINE(hid_ep_in_busy, 1);
static ATOMIC_DEFINE(pct1x36_write_busy, 1);

#define HID_EP_BUSY_FLAG 0
#define PCT1X36_WRITE_BUSY_FLAG 0

#if 1
static uint8_t pct1x36_report_desc[] = {
	0X05,
	0X01,
	0X09,
	0X02,
	0Xa1,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_MOUSE_REL,
	0X09,
	0X01,
	0Xa1,
	0X00,
	0X05,
	0X09,
	0X19,
	0X01,
	0X29,
	0X02,
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X81,
	0X02,
	0X95,
	0X06,
	0X81,
	0X03,
	0X05,
	0X01,
	0X09,
	0X30,
	0X09,
	0X31,
	0X09,
	0X38,
	0X15,
	0X81,
	0X25,
	0X7f,
	0X75,
	0X08,
	0X95,
	0X03,
	0X81,
	0X06,
	0X05,
	0X0c,
	0X0a,
	0X38,
	0X02,
	0X75,
	0X08,
	0X95,
	0X01,
	0X81,
	0X06,
	0X75,
	0X08,
	0X95,
	0X03,
	0X81,
	0X03,
	0Xc0,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X05,
	0Xa1,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_TOUCH_PAD,

	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X02,
	0X09,
	0X47,
	0X09,
	0X42,
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X81,
	0X02,
	0X95,
	0X02,
	0X81,
	0X03,
	0X09,
	0X51,
	0X25,
	0X0f,
	0X75,
	0X04,
	0X95,
	0X01,
	0X81,
	0X02,
	0X05,
	0X01,
	0X09,
	0X30,
	0X75,
	0X10,
	0X55,
	0X0e,
	0X65,
	0X11,
	0X35,
	0X00,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X09,
	0X31,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X30,
	0X95,
	0X01,
	0X81,
	0X02,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X02,
	0X09,
	0X47,
	0X09,
	0X42,
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X81,
	0X02,
	0X95,
	0X02,
	0X81,
	0X03,
	0X09,
	0X51,
	0X25,
	0X0f,
	0X75,
	0X04,
	0X95,
	0X01,
	0X81,
	0X02,
	0X05,
	0X01,
	0X09,
	0X30,
	0X75,
	0X10,
	0X55,
	0X0e,
	0X65,
	0X11,
	0X35,
	0X00,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X09,
	0X31,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X30,
	0X95,
	0X01,
	0X81,
	0X02,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X02,
	0X09,
	0X47,
	0X09,
	0X42,
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X81,
	0X02,
	0X95,
	0X02,
	0X81,
	0X03,
	0X09,
	0X51,
	0X25,
	0X0f,
	0X75,
	0X04,
	0X95,
	0X01,
	0X81,
	0X02,
	0X05,
	0X01,
	0X09,
	0X30,
	0X75,
	0X10,
	0X55,
	0X0e,
	0X65,
	0X11,
	0X35,
	0X00,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X09,
	0X31,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X30,
	0X95,
	0X01,
	0X81,
	0X02,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X02,
	0X09,
	0X47,
	0X09,
	0X42,
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X81,
	0X02,
	0X95,
	0X02,
	0X81,
	0X03,
	0X09,
	0X51,
	0X25,
	0X0f,
	0X75,
	0X04,
	0X95,
	0X01,
	0X81,
	0X02,
	0X05,
	0X01,
	0X09,
	0X30,
	0X75,
	0X10,
	0X55,
	0X0e,
	0X65,
	0X11,
	0X35,
	0X00,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X09,
	0X31,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X30,
	0X95,
	0X01,
	0X81,
	0X02,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X02,
	0X09,
	0X47,
	0X09,
	0X42,
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X81,
	0X02,
	0X95,
	0X02,
	0X81,
	0X03,
	0X09,
	0X51,
	0X25,
	0X0f,
	0X75,
	0X04,
	0X95,
	0X01,
	0X81,
	0X02,
	0X05,
	0X01,
	0X09,
	0X30,
	0X75,
	0X10,
	0X55,
	0X0e,
	0X65,
	0X11,
	0X35,
	0X00,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_X >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_X >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X09,
	0X31,
	0X46,
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_PHYSICAL_MAX_Y >> 8),
	0X27,
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y & 0XFF),
	(CONFIG_USB_HID_TOUCHPAD_LOGICAL_MAX_Y >> 8),
	0X00,
	0X00,
	0X81,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X30,
	0X95,
	0X01,
	0X81,
	0X02,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X54, // CONTACT_COUNT
	0X15,
	0X00,
	0X25,
	0X05,
	0X75,
	0X08,
	0X95,
	0X01,
	0X81,
	0X02,
	0X05,
	0X09,
	0X09,
	0X01, // BUTTON_STATE_TOUCHPAD
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X01,
	0X81,
	0X02,
	0X95,
	0X07,
	0X81,
	0X03,
	0X05,
	0X0d,
	0X09,
	0X56, // SCAN_TIME
	0X55,
	0X0c,
	0X66,
	0X01,
	0X10,
	0X35,
	0X00,
	0X47,
	0Xff,
	0Xff,
	0X00,
	0X00,
	0X15,
	0X00,
	0X27,
	0Xff,
	0Xff,
	0X00,
	0X00,
	0X75,
	0X10,
	0X95,
	0X01,
	0X81,
	0X02,

	0X05,
	0X0d,
	0X09,
	0X55,
	0X15,
	0X00,
	0X25,
	0X05,
	0X75,
	0X08,
	0X95,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_CONTACT_COUNT_MAX,
	0Xb1,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X59, // BUTTON_TYPE
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X08,
	0X95,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_BUTTON_TYPE,
	0Xb1,
	0X02,
	0X05,
	0X0d,
	0X09,
	0X60, // LATENCY_MODE
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_LATENCY_MODE,
	0Xb1,
	0X02,
	0X95,
	0X07,
	0Xb1,
	0X03,
	0X06,
	0X00,
	0Xff,
	0X09,
	0Xc5, // CERT_STATUS
	0X15,
	0X00,
	0X26,
	0Xff,
	0X00,
	0X75,
	0X08,
	0X96,
	0X00,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_CERT_STATUS,
	0Xb1,
	0X02,
	0Xc0,

	0X05,
	0X0d,
	0X09,
	0X0e,
	0Xa1,
	0X01,
	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X02,
	0X09,
	0X52, // INPUT_MODE
	0X15,
	0X00,
	0X25,
	0X0a,
	0X75,
	0X08,
	0X95,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_INPUT_MODE,
	0Xb1,
	0X02,
	0Xc0,
	0X05,
	0X0d,
	0X09,
	0X22,
	0Xa1,
	0X00,
	0X09,
	0X57, // SURFACE_SWITCH
	0X09,
	0X58, // BUTTON_SWITCH
	0X15,
	0X00,
	0X25,
	0X01,
	0X75,
	0X01,
	0X95,
	0X02,
	0X85,
	PCT1X36_REPORT_ID_SWITCH_MODE,
	0Xb1,
	0X02,
	0X95,
	0X06,
	0Xb1,
	0X03,
	0Xc0,
	0Xc0,

	0X06,
	0X00,
	0Xff,
	0X09,
	0X01,
	0Xa1,
	0X01,
	0X85,
	PCT1X36_REPORT_ID_REG_DATA,
	0X09,
	0X06,
	0X15,
	0X00,
	0X26,
	0Xff,
	0X00,
	0X75,
	0X08,
	0X96,
	0X03,
	0X00,
	0Xb1,
	0X02,
	0X85,
	PCT1X36_REPORT_ID_BURST_REGISTER,
	0X09,
	0X05,
	0X15,
	0X00,
	0X26,
	0Xff,
	0X00,
	0X75,
	0X08,
	0X96,
	0X00,
	0X01,
	0Xb1,
	0X02,
	0X85,
	PCT1X36_REPORT_ID_USR_DATA,
	0X09,
	0X06,
	0X15,
	0X00,
	0X26,
	0Xff,
	0X00,
	0X75,
	0X08,
	0X96,
	0X03,
	0X00,
	0Xb1,
	0X02,
	0X85,
	PCT1X36_REPORT_ID_VENDOR_STATUS,
	0X09,
	0X11,
	0X15,
	0X00,
	0X26,
	0Xff,
	0X00,
	0X75,
	0X08,
	0X96,
	0X01,
	0X00,
	0Xb1,
	0X02,
	0Xc0,
};

/* A 256-byte default blob for the 'device certification status' feature report.
 */
static uint8_t device_cert_response[] = {
	PCT1X36_REPORT_ID_CERT_STATUS,

	0xFC,
	0x28,
	0xFE,
	0x84,
	0x40,
	0xCB,
	0x9A,
	0x87,
	0x0D,
	0xBE,
	0x57,
	0x3C,
	0xB6,
	0x70,
	0x09,
	0x88,
	0x07,
	0x97,
	0x2D,
	0x2B,
	0xE3,
	0x38,
	0x34,
	0xB6,
	0x6C,
	0xED,
	0xB0,
	0xF7,
	0xE5,
	0x9C,
	0xF6,
	0xC2,
	0x2E,
	0x84,
	0x1B,
	0xE8,
	0xB4,
	0x51,
	0x78,
	0x43,
	0x1F,
	0x28,
	0x4B,
	0x7C,
	0x2D,
	0x53,
	0xAF,
	0xFC,
	0x47,
	0x70,
	0x1B,
	0x59,
	0x6F,
	0x74,
	0x43,
	0xC4,
	0xF3,
	0x47,
	0x18,
	0x53,
	0x1A,
	0xA2,
	0xA1,
	0x71,
	0xC7,
	0x95,
	0x0E,
	0x31,
	0x55,
	0x21,
	0xD3,
	0xB5,
	0x1E,
	0xE9,
	0x0C,
	0xBA,
	0xEC,
	0xB8,
	0x89,
	0x19,
	0x3E,
	0xB3,
	0xAF,
	0x75,
	0x81,
	0x9D,
	0x53,
	0xB9,
	0x41,
	0x57,
	0xF4,
	0x6D,
	0x39,
	0x25,
	0x29,
	0x7C,
	0x87,
	0xD9,
	0xB4,
	0x98,
	0x45,
	0x7D,
	0xA7,
	0x26,
	0x9C,
	0x65,
	0x3B,
	0x85,
	0x68,
	0x89,
	0xD7,
	0x3B,
	0xBD,
	0xFF,
	0x14,
	0x67,
	0xF2,
	0x2B,
	0xF0,
	0x2A,
	0x41,
	0x54,
	0xF0,
	0xFD,
	0x2C,
	0x66,
	0x7C,
	0xF8,
	0xC0,
	0x8F,
	0x33,
	0x13,
	0x03,
	0xF1,
	0xD3,
	0xC1,
	0x0B,
	0x89,
	0xD9,
	0x1B,
	0x62,
	0xCD,
	0x51,
	0xB7,
	0x80,
	0xB8,
	0xAF,
	0x3A,
	0x10,
	0xC1,
	0x8A,
	0x5B,
	0xE8,
	0x8A,
	0x56,
	0xF0,
	0x8C,
	0xAA,
	0xFA,
	0x35,
	0xE9,
	0x42,
	0xC4,
	0xD8,
	0x55,
	0xC3,
	0x38,
	0xCC,
	0x2B,
	0x53,
	0x5C,
	0x69,
	0x52,
	0xD5,
	0xC8,
	0x73,
	0x02,
	0x38,
	0x7C,
	0x73,
	0xB6,
	0x41,
	0xE7,
	0xFF,
	0x05,
	0xD8,
	0x2B,
	0x79,
	0x9A,
	0xE2,
	0x34,
	0x60,
	0x8F,
	0xA3,
	0x32,
	0x1F,
	0x09,
	0x78,
	0x62,
	0xBC,
	0x80,
	0xE3,
	0x0F,
	0xBD,
	0x65,
	0x20,
	0x08,
	0x13,
	0xC1,
	0xE2,
	0xEE,
	0x53,
	0x2D,
	0x86,
	0x7E,
	0xA7,
	0x5A,
	0xC5,
	0xD3,
	0x7D,
	0x98,
	0xBE,
	0x31,
	0x48,
	0x1F,
	0xFB,
	0xDA,
	0xAF,
	0xA2,
	0xA8,
	0x6A,
	0x89,
	0xD6,
	0xBF,
	0xF2,
	0xD3,
	0x32,
	0x2A,
	0x9A,
	0xE4,
	0xCF,
	0x17,
	0xB7,
	0xB8,
	0xF4,
	0xE1,
	0x33,
	0x08,
	0x24,
	0x8B,
	0xC4,
	0x43,
	0xA5,
	0xE5,
	0x24,
	0xC2,
};

/* Device capabilities feature report. */
static uint8_t device_caps_response[] = {
	PCT1X36_REPORT_ID_CONTACT_COUNT_MAX,

	PCT1X36_MAX_FINGERS, /* Contact Count Maximum */
};

static uint8_t device_TYPE_response[] = {
	PCT1X36_REPORT_ID_BUTTON_TYPE,

	0,
};
#endif

// static struct i2c_hid_descriptor hid_desc;
static pct1x36_desc_t hid_desc = {
	.hid_desc_length = sizeof(hid_desc),
	.version = 0x0100,
	.report_desc_length = sizeof(pct1x36_report_desc),
	.report_desc_register = 0x0021,
	.input_register = 0x0024,
	.max_input_length = PCT1X36_REPORT_LEN,
	.output_register = 0x0025,
	.max_output_length = 0x0000,
	.command_register = 0x0022,
	.data_register = 0x0023,
	.vendor_id = 0x093a,
	.product_id = 0x0255,
	.version_id = 0,
	.reserved = 0,
};
static pct1x36_data_t pct1x36_dat;
static pct1x36_read_data_t pct1x36_buf;
static int pct1x36_burst_cnt;

static void hid_tp_proc_queue(void);
DECLARE_DEFERRED(hid_tp_proc_queue);

static void write_tp_report(struct pct1x36_read_data *report)
{
	if (!atomic_test_and_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
		int ret = hid_int_ep_write(hid_dev, (uint8_t *)report->hid_data,
					   report->len - 2, NULL);

		if (ret) {
			LOG_ERR("hid tp write error, %d", ret);
		}
	}
}

static int pct1x36_write(void)
{
	if (pct1x36_dat.id < 0X0F) {
		if (i2c_xfer(CONFIG_PCT1X36_I2C_PORT,
			     CONFIG_PCT1X36_I2C_SLAVE_ADDR,
			     (uint8_t *)&pct1x36_dat,
			     pct1x36_dat.payload.cmd_data.len + 6, NULL, 0)) {
			return -ENOTSUP;
		}
	} else {
		if (i2c_xfer(CONFIG_PCT1X36_I2C_PORT,
			     CONFIG_PCT1X36_I2C_SLAVE_ADDR,
			     (uint8_t *)&pct1x36_dat,
			     pct1x36_dat.payload.l.cmd_data.len + 7, NULL, 0)) {
			return -ENOTSUP;
		}
	}

	if (pct1x36_dat.id == 0X0F) {
		if ((pct1x36_dat.payload.l.report_id ==
		     PCT1X36_REPORT_ID_REG_DATA) ||
		    (pct1x36_dat.payload.l.report_id ==
		     PCT1X36_REPORT_ID_USR_DATA)) {
			if (pct1x36_dat.payload.l.cmd_data.hid_data[2] & 0x10) {
				pct1x36_dat.op_code = OP_CODE_GET_REPORT;
				if (i2c_xfer(CONFIG_PCT1X36_I2C_PORT,
					     CONFIG_PCT1X36_I2C_SLAVE_ADDR,
					     (uint8_t *)&pct1x36_dat, 7,
					     (uint8_t *)&pct1x36_dat.payload.l
						     .cmd_data.len,
					     pct1x36_dat.payload.l.cmd_data
						     .len)) {
					return -ENOTSUP;
				}
			}
		}
	}
	return 0;
}

static int pct1x36_get_report(const struct device *dev,
			      struct usb_setup_packet *setup, int32_t *len,
			      uint8_t **data)
{
	switch (setup->wValue & 0xFF) {
	case PCT1X36_REPORT_ID_CONTACT_COUNT_MAX:
		*data = device_caps_response;
		*len = sizeof(device_caps_response);
		return 0;
	case PCT1X36_REPORT_ID_BUTTON_TYPE:
		*data = device_TYPE_response;
		*len = sizeof(device_TYPE_response);
		return 0;
	case PCT1X36_REPORT_ID_CERT_STATUS:
		*data = device_cert_response;
		*len = sizeof(device_cert_response);
		return 0;
	}

	if ((setup->wValue & 0xFF) < 0X0F) {
		*data = pct1x36_dat.payload.cmd_data.hid_data;
		*len = pct1x36_dat.payload.cmd_data.len;
	} else {
		*data = pct1x36_dat.payload.l.cmd_data.hid_data;
		*len = pct1x36_dat.payload.l.cmd_data.len;
	}
	return 0;
}

static int pct1x36_set_report(const struct device *dev,
			      struct usb_setup_packet *setup, int32_t *len,
			      uint8_t **data)
{
	if ((setup->wValue & 0xFF) >= 0X0F) {
		if (!atomic_test_and_set_bit(pct1x36_write_busy,
					     PCT1X36_WRITE_BUSY_FLAG)) {
			pct1x36_dat.reg_idx = hid_desc.command_register;
			pct1x36_dat.id = setup->wValue & 0x0F;
			pct1x36_dat.type = REPORT_TYPE_FEATURE;
			pct1x36_dat.reserved = 0;
			pct1x36_dat.op_code = OP_CODE_SET_REPORT;
			pct1x36_dat.reserved_1 = 0;
			if ((setup->wValue & 0xFF) < 0X0F) {
				pct1x36_dat.payload.cmd_data.reg_idx =
					hid_desc.data_register;
				pct1x36_dat.payload.cmd_data.len = *len + 2;
				memcpy(pct1x36_dat.payload.cmd_data.hid_data,
				       *data, *len);
				task_set_event(TASK_ID_TOUCHPAD,
					       TASK_EVENT_PCT1X36_WRITE);
			} else {
				pct1x36_dat.id = 0X0F;
				pct1x36_dat.payload.l.report_id =
					setup->wValue & 0XFF;
				pct1x36_dat.payload.l.cmd_data.reg_idx =
					hid_desc.data_register;
				if ((setup->wValue & 0xFF) ==
				    PCT1X36_REPORT_ID_BURST_REGISTER) {
					memcpy(pct1x36_dat.payload.l.cmd_data
							       .hid_data +
						       pct1x36_burst_cnt + 1,
					       (*data) + 1, (*len) - 1);
					pct1x36_burst_cnt += ((*len) - 1);
					if (pct1x36_burst_cnt >= 256) {
						pct1x36_burst_cnt = 0;
						pct1x36_dat.payload.l.cmd_data
							.hid_data[0] =
							PCT1X36_REPORT_ID_BURST_REGISTER;
						pct1x36_dat.payload.l.cmd_data
							.len = 259;
						task_set_event(
							TASK_ID_TOUCHPAD,
							TASK_EVENT_PCT1X36_WRITE);
					} else {
						atomic_clear_bit(
							pct1x36_write_busy,
							PCT1X36_WRITE_BUSY_FLAG);
					}
				} else {
					pct1x36_burst_cnt = 0;
					pct1x36_dat.payload.l.cmd_data.len =
						*len + 2;
					memcpy(pct1x36_dat.payload.l.cmd_data
						       .hid_data,
					       *data, *len);
					task_set_event(
						TASK_ID_TOUCHPAD,
						TASK_EVENT_PCT1X36_WRITE);
				}
			}
			return 0;
		}
	}
	return 0;
}

static void int_in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);
	atomic_clear_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG);
}

static const struct hid_ops ops = {
	.get_report = pct1x36_get_report,
	.set_report = pct1x36_set_report,
	.int_in_ready = int_in_ready_cb,
};

static void set_pct1x36_report(struct pct1x36_read_data *report)
{
	static int print_full = 1;

	if (!hid_dev || !check_usb_is_configured()) {
		return;
	}

	mutex_lock(report_queue_mutex);

	if (!check_usb_is_suspended()) {
		if (queue_is_empty(&report_queue)) {
			write_tp_report(report);
			mutex_unlock(report_queue_mutex);
			return;
		}
	} else {
		if (!request_usb_wake()) {
			mutex_unlock(report_queue_mutex);
			return;
		}
	}

	if (queue_is_full(&report_queue)) {
		if (print_full)
			LOG_WRN("touchpad queue full\n");
		print_full = 0;

		queue_advance_head(&report_queue, 1);
	} else {
		print_full = 1;
	}
	queue_add_unit(&report_queue, report);

	mutex_unlock(report_queue_mutex);

	hook_call_deferred(&hid_tp_proc_queue_data, 0);
}

static void hid_tp_proc_queue(void)
{
	struct pct1x36_read_data report;

	mutex_lock(report_queue_mutex);

	/* clear queue if the usb dc status is reset or disconected */
	if (!check_usb_is_configured() && !check_usb_is_suspended()) {
		queue_remove_units(&report_queue, NULL,
				   queue_count(&report_queue));
		mutex_unlock(report_queue_mutex);
		return;
	}

	if (queue_is_empty(&report_queue)) {
		mutex_unlock(report_queue_mutex);
		return;
	}

	queue_peek_units(&report_queue, &report, 0, 1);

	write_tp_report(&report);

	queue_advance_head(&report_queue, 1);

	mutex_unlock(report_queue_mutex);
	hook_call_deferred(&hid_tp_proc_queue_data, 1 * MSEC);
}

void touchpad_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_TOUCHPAD);
}

void touchpad_task(void *u)
{
	uint32_t event;

	pct1x36_burst_cnt = 0;
	atomic_clear_bit(pct1x36_write_busy, PCT1X36_WRITE_BUSY_FLAG);

	i2c_xfer(CONFIG_PCT1X36_I2C_PORT, CONFIG_PCT1X36_I2C_SLAVE_ADDR, NULL,
		 0, (uint8_t *)&pct1x36_buf, hid_desc.max_input_length);

	while (1) {
		event = task_wait_event(-1);

		if (event & TASK_EVENT_WAKE) {
			i2c_xfer(CONFIG_PCT1X36_I2C_PORT,
				 CONFIG_PCT1X36_I2C_SLAVE_ADDR, NULL, 0,
				 (uint8_t *)&pct1x36_buf,
				 hid_desc.max_input_length);

			if (pct1x36_buf.len >= 2 &&
			    pct1x36_buf.len <= hid_desc.max_input_length) {
				set_pct1x36_report(&pct1x36_buf);
			}
		}

		if (event & TASK_EVENT_PCT1X36_WRITE) {
			pct1x36_write();
			atomic_clear_bit(pct1x36_write_busy,
					 PCT1X36_WRITE_BUSY_FLAG);
		}
	}
}

static int usb_hid_tp_init(void)
{
	hid_dev = device_get_binding(TP_DEV_NAME);

	if (!hid_dev) {
		LOG_ERR("failed to get hid device");
		return -ENXIO;
	}

	gpio_enable_interrupt(CONFIG_PCT1X36_GPIO_INT);

	usb_hid_register_device(hid_dev, pct1x36_report_desc,
				hid_desc.report_desc_length, &ops);

	usb_hid_init(hid_dev);
	atomic_clear_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG);
	return 0;
}

SYS_INIT(usb_hid_tp_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
