/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"
#include "peripheral_charger.h"
#include "timer.h"
#include "util.h"

/*
 * Configuration
 */

/* WLC-host can sleep and wake up by i2c. I2C is resent after this delay. */
static const int _wake_up_delay_ms = 10;
static const int _detection_interval = 100; /* msec */

/*
 * Static Parameters
 */
#define CTN730_I2C_ADDR			0x28
#define CTN730_MESSAGE_BUFFER_SIZE	0x20

/* Message Types */
#define CTN730_MESSAGE_TYPE_COMMAND	0b00
#define CTN730_MESSAGE_TYPE_RESPONSE	0b01
#define CTN730_MESSAGE_TYPE_EVENT	0b10

/* Instruction Codes */
#define WLC_HOST_CTRL_RESET			0b000000
#define WLC_HOST_CTRL_DUMP_STATUS		0b001100
#define WLC_HOST_CTRL_GENERIC_ERROR		0b001111
#define WLC_CHG_CTRL_ENABLE			0b010000
#define WLC_CHG_CTRL_DISABLE			0b010001
#define WLC_CHG_CTRL_DEVICE_STATE		0b010010
#define WLC_CHG_CTRL_CHARGING_STATE		0b010100
#define WLC_CHG_CTRL_CHARGING_INFO		0b010101

/* WLC_HOST_CTRL_RESET constants */
#define WLC_HOST_CTRL_RESET_CMD_SIZE		1
#define WLC_HOST_CTRL_RESET_RSP_SIZE		1
#define WLC_HOST_CTRL_RESET_EVT_NORMAL_MODE	0x00
#define WLC_HOST_CTRL_RESET_EVT_DOWNLOAD_MODE	0x00
#define WLC_HOST_CTRL_RESET_CMD_MODE_NORMAL	0x00
#define WLC_HOST_CTRL_RESET_CMD_MODE_DOWNLOAD	0x01

/* WLC_CHG_CTRL_ENABLE constants */
#define WLC_CHG_CTRL_ENABLE_CMD_SIZE		2
#define WLC_CHG_CTRL_ENABLE_RSP_SIZE		1

/* WLC_CHG_CTRL_DISABLE constants */
#define WLC_CHG_CTRL_DISABLE_CMD_SIZE		0
#define WLC_CHG_CTRL_DISABLE_RSP_SIZE		2

/* WLC_CHG_CTRL_DEVICE_STATE constants */
#define WLC_CHG_CTRL_DEVICE_STATE_DEVICE_DETECTED		0x00
#define WLC_CHG_CTRL_DEVICE_STATE_DEVICE_DEACTIVATED		0x01
#define WLC_CHG_CTRL_DEVICE_STATE_DEVICE_DEVICE_LOST		0x02
#define WLC_CHG_CTRL_DEVICE_STATE_DEVICE_DEVICE_BAD_VERSION	0x03
#define WLC_CHG_CTRL_DEVICE_STATE_EVT_SIZE_DETECTED		8
#define WLC_CHG_CTRL_DEVICE_STATE_EVT_SIZE			1

/* WLC_CHG_CTRL_CHARGING_STATE constants */
#define WLC_CHG_CTRL_CHARGING_STATE_CHARGE_STARTED		0x00
#define WLC_CHG_CTRL_CHARGING_STATE_CHARGE_ENDED		0x01
#define WLC_CHG_CTRL_CHARGING_STATE_CHARGE_STOPPED		0x02
#define WLC_CHG_CTRL_CHARGING_STATE_EVT_SIZE			1

/* WLC_HOST_CTRL_DUMP_STATUS constants */
#define WLC_HOST_CTRL_DUMP_STATUS_CMD_SIZE	1

/* WLC_CHG_CTRL_CHARGING_INFO constants */
#define WLC_CHG_CTRL_CHARGING_INFO_EVT_SIZE	5

static inline uint16_t htole16(uint16_t host_16bits)
{
	/* Assume host is little endian */
	return (uint16_t)(host_16bits);
}

/* Status Codes */
enum wlc_host_status {
	WLC_HOST_STATUS_OK				= 0x00,
	WLC_HOST_STATUS_PARAMETER_ERROR			= 0x01,
	WLC_HOST_STATUS_STATE_ERROR			= 0x02,
	WLC_HOST_STATUS_VALUE_ERROR			= 0x03,
	WLC_HOST_STATUS_REJECTED			= 0x04,
	WLC_HOST_STATUS_RESOURCE_ERROR			= 0x10,
	WLC_HOST_STATUS_TXLDO_ERROR			= 0x11,
	WLC_HOST_STATUS_ANTENNA_SELECTION_ERROR		= 0x12,
	WLC_HOST_STATUS_BIST_FAILED			= 0x20,
	WLC_HOST_STATUS_BIST_NO_WLC_CAP			= 0x21,
	WLC_HOST_STATUS_BIST_TXLDO_CURRENT_OVERFLOW	= 0x22,
	WLC_HOST_STATUS_BIST_TXLDO_CURRENT_UNDERFLOW	= 0x23,
	WLC_HOST_STATUS_FW_VERSION_ERROR		= 0x30,
	WLC_HOST_STATUS_FW_VERIFICATION_ERROR		= 0x31,
	WLC_HOST_STATUS_NTAG_BLOCK_PARAMETER_ERROR	= 0x32,
	WLC_HOST_STATUS_NTAG_READ_ERROR			= 0x33,
};

struct ctn730_msg {
	uint8_t instruction : 6;
	uint8_t message_type : 2;
	uint8_t length;
	uint8_t payload[];
} __packed;

#define CPRINTS(fmt, args...) \
	do { \
		cprints(CC_PCHG, "CTN730: " fmt, ##args); \
		cflush(); \
	} while (0)

static const char *_text_instruction(uint8_t instruction)
{
	/* TODO: For normal build, use %pb and BINARY_VALUE(res->inst, 6) */
	switch (instruction) {
	case WLC_HOST_CTRL_RESET:
		return "RESET";
	case WLC_HOST_CTRL_DUMP_STATUS:
		return "DUMP_STATUS";
	case WLC_HOST_CTRL_GENERIC_ERROR:
		return "GENERIC_ERROR";
	case WLC_CHG_CTRL_ENABLE:
		return "ENABLE";
	case WLC_CHG_CTRL_DEVICE_STATE:
		return "DEVICE_STATE";
	case WLC_CHG_CTRL_CHARGING_STATE:
		return "CHARGING_STATE";
	case WLC_CHG_CTRL_CHARGING_INFO:
		return "CHARGING_INFO";
	default:
		return "UNDEFINED";
	}
}

static const char *_text_message_type(uint8_t type)
{
	switch (type) {
	case CTN730_MESSAGE_TYPE_COMMAND:
		return "CMD";
	case CTN730_MESSAGE_TYPE_RESPONSE:
		return "RSP";
	case CTN730_MESSAGE_TYPE_EVENT:
		return "EVT";
	default:
		return "INVALID";
	}
}

static int _i2c_read(int port, uint8_t *in, int in_len)
{
	int rv;

	memset(in, 0, in_len);

	rv = i2c_xfer(port, CTN730_I2C_ADDR, NULL, 0, in, in_len);
	if (rv)
		CPRINTS("Failed to read: %d", rv);

	if (rv == EC_ERROR_BUSY) {
		msleep(_wake_up_delay_ms);
		rv = i2c_xfer(port, CTN730_I2C_ADDR, NULL, 0, in, in_len);
		if (rv)
			CPRINTS("Failed to reread: %d", rv);
	}

	return rv;
}

static int _i2c_write(int port, const uint8_t *out, int out_len)
{
	int rv = i2c_xfer(port, CTN730_I2C_ADDR, out, out_len, NULL, 0);

	if (rv)
		CPRINTS("Failed to write: %d", rv);

	if (rv == EC_ERROR_BUSY) {
		msleep(_wake_up_delay_ms);
		rv = i2c_xfer(port, CTN730_I2C_ADDR, out, out_len, NULL, 0);
		if (rv)
			CPRINTS("Failed to rewrite: %d", rv);
	}

	return rv;
}

static int ctn730_init(struct pchg *ctx)
{
	uint8_t out[sizeof(struct ctn730_msg) + WLC_HOST_CTRL_RESET_CMD_SIZE];
	struct ctn730_msg *cmd = (void *)out;
	int rv;

	cmd->message_type = CTN730_MESSAGE_TYPE_COMMAND;
	cmd->instruction = WLC_HOST_CTRL_RESET;
	cmd->length = WLC_HOST_CTRL_RESET_CMD_SIZE;
	cmd->payload[0] = WLC_HOST_CTRL_RESET_CMD_MODE_NORMAL;

	/* TODO: Run 1 sec timeout timer. */
	rv = _i2c_write(ctx->i2c_port, out, sizeof(out));
	if (rv)
		return rv;

	/* WLC-host should send EVT_HOST_CTRL_RESET_EVT shortly. */
	return EC_SUCCESS_PENDING;
}

static int ctn730_enable(struct pchg *ctx, bool enable)
{
	uint8_t out[sizeof(struct ctn730_msg) + WLC_CHG_CTRL_ENABLE_CMD_SIZE];
	struct ctn730_msg *cmd = (void *)out;
	uint16_t *interval = (void *)cmd->payload;
	int rv;

	cmd->message_type = CTN730_MESSAGE_TYPE_COMMAND;
	if (enable) {
		cmd->instruction = WLC_CHG_CTRL_ENABLE;
		cmd->length = WLC_CHG_CTRL_ENABLE_CMD_SIZE;
		*interval = htole16(_detection_interval);
	} else {
		cmd->instruction = WLC_CHG_CTRL_DISABLE;
		cmd->length = WLC_CHG_CTRL_DISABLE_CMD_SIZE;
	}

	rv = _i2c_write(ctx->i2c_port, out, sizeof(out));
	if (rv)
		return rv;

	return EC_SUCCESS_PENDING;
}

static int ctn730_pause(struct pchg *ctx, bool pause)
{
	return EC_SUCCESS;
}

static int _process_payload_response(struct pchg *ctx, uint8_t *buf)
{
	struct ctn730_msg *res = (void *)buf;
	uint8_t inst = res->instruction;
	uint8_t len = res->length;
	int rv;

	rv = _i2c_read(ctx->i2c_port, buf, len);
	if (rv)
		return rv;

	switch (inst) {
	case WLC_HOST_CTRL_RESET:
		if (len != WLC_HOST_CTRL_RESET_RSP_SIZE
				|| buf[0] != WLC_HOST_STATUS_OK)
			return EC_ERROR_UNKNOWN;
		ctx->event = PCHG_EVENT_NONE;
		break;
	case WLC_CHG_CTRL_ENABLE:
		if (len != WLC_CHG_CTRL_ENABLE_RSP_SIZE
				|| buf[0] != WLC_HOST_STATUS_OK)
			return EC_ERROR_UNKNOWN;
		ctx->event = PCHG_EVENT_ENABLED;
		break;
	case WLC_CHG_CTRL_DISABLE:
		if (len != WLC_CHG_CTRL_DISABLE_RSP_SIZE
				|| buf[0] != WLC_HOST_STATUS_OK)
			return EC_ERROR_UNKNOWN;
		ctx->event = PCHG_EVENT_DISABLED;
		break;
	default:
		CPRINTS("Received unknown response (%d)", inst);
		ctx->event = PCHG_EVENT_NONE;
		break;
	}

	return EC_SUCCESS;
}

static int _process_payload_event(struct pchg *ctx, uint8_t *buf)
{
	struct ctn730_msg *res = (void *)buf;
	uint8_t inst = res->instruction;
	uint8_t len = res->length;
	int rv;

	rv = _i2c_read(ctx->i2c_port, buf, len);
	if (rv)
		return rv;

	switch (inst) {
	case WLC_HOST_CTRL_RESET:
		if (buf[0] == WLC_HOST_CTRL_RESET_EVT_NORMAL_MODE)
			ctx->event = PCHG_EVENT_INITIALIZED;
		else
			return EC_ERROR_INVAL;
		break;
	case WLC_HOST_CTRL_GENERIC_ERROR:
		break;
	case WLC_CHG_CTRL_DEVICE_STATE:
		switch (buf[0]) {
		case WLC_CHG_CTRL_DEVICE_STATE_DEVICE_DETECTED:
			if (len != WLC_CHG_CTRL_DEVICE_STATE_EVT_SIZE_DETECTED)
				return EC_ERROR_INVAL;
			ctx->event = PCHG_EVENT_DEVICE_DETECTED;
			break;
		case WLC_CHG_CTRL_DEVICE_STATE_DEVICE_DEVICE_LOST:
			if (len != WLC_CHG_CTRL_DEVICE_STATE_EVT_SIZE)
				return EC_ERROR_INVAL;
			ctx->event = PCHG_EVENT_DEVICE_LOST;
			break;
		default:
			return EC_ERROR_INVAL;
		}
		break;
	case WLC_CHG_CTRL_CHARGING_STATE:
		if (len != WLC_CHG_CTRL_CHARGING_STATE_EVT_SIZE)
			return EC_ERROR_INVAL;
		switch (buf[0]) {
		case WLC_CHG_CTRL_CHARGING_STATE_CHARGE_STARTED:
			ctx->event = PCHG_EVENT_CHARGE_STARTED;
			break;
		case WLC_CHG_CTRL_CHARGING_STATE_CHARGE_ENDED:
			ctx->event = PCHG_EVENT_CHARGE_END;
			break;
		case WLC_CHG_CTRL_CHARGING_STATE_CHARGE_STOPPED:
			ctx->event = PCHG_EVENT_CHARGE_ERROR;
			break;
		default:
			return EC_ERROR_INVAL;
		}
		break;
	case WLC_CHG_CTRL_CHARGING_INFO:
		if (len != WLC_CHG_CTRL_CHARGING_INFO_EVT_SIZE || buf[0] > 100)
			return EC_ERROR_INVAL;
		ctx->event = PCHG_EVENT_CHARGE_UPDATE;
		ctx->battery_percent = buf[0];
		break;
	default:
		CPRINTS("Received unknown event (%d)", inst);
		break;
	}

	return EC_SUCCESS;
}

static int ctn730_get_event(struct pchg *ctx)
{
	uint8_t buf[CTN730_MESSAGE_BUFFER_SIZE];
	struct ctn730_msg *res = (void *)buf;
	int i2c_port = ctx->i2c_port;
	int rv;

	/* Read message header */
	rv = _i2c_read(i2c_port, buf, sizeof(*res));
	if (rv)
		return rv;

	CPRINTS("Response header: %s %s LEN=%d",
		_text_message_type(res->message_type),
		_text_instruction(res->instruction), res->length);

	if (sizeof(buf) < res->length) {
		CPRINTS("Response size (%d) exceeds buffer", res->length);
		return EC_ERROR_OVERFLOW;
	}

	if (res->message_type == CTN730_MESSAGE_TYPE_RESPONSE) {
		/* TODO: Check 1 sec timeout. */
		return _process_payload_response(ctx, buf);
	} else if (res->message_type == CTN730_MESSAGE_TYPE_EVENT) {
		return _process_payload_event(ctx, buf);
	}

	CPRINTS("Invalid message type (%d)", res->message_type);
	return EC_ERROR_UNKNOWN;
}

static int ctn730_get_error_info(struct pchg *ctx)
{
	return EC_SUCCESS;
}

struct pchg_drv ctn730_drv = {
	.init = ctn730_init,
	.enable = ctn730_enable,
	.pause = ctn730_pause,
	.get_event = ctn730_get_event,
	.get_error_info = ctn730_get_error_info,
};

static int cc_ctn730(int argc, char **argv)
{
	int port;
	struct pchg *ctx;
	char *end;
	uint8_t buf[CTN730_MESSAGE_BUFFER_SIZE];
	struct ctn730_msg *cmd = (void *)buf;
	struct ctn730_msg *res = (void *)buf;
	timestamp_t deadline;
	int timeout;

	if (argc < 4)
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[1], &end, 0);
	if (*end || port < 0 || pchg_count <= port)
		return EC_ERROR_PARAM2;

	ctx = &pchgs[port];

	if (!strcasecmp(argv[2], "dump")) {
		int tag;

		tag = strtoi(argv[3], &end, 0);

		if (*end || tag < 0 || 0x07 < tag)
			return EC_ERROR_PARAM3;

		gpio_disable_interrupt(ctx->irq_pin);

		cmd->message_type = CTN730_MESSAGE_TYPE_COMMAND;
		cmd->instruction = WLC_HOST_CTRL_DUMP_STATUS;
		cmd->length = WLC_HOST_CTRL_DUMP_STATUS_CMD_SIZE;
		cmd->payload[0] = tag;

		_i2c_write(ctx->i2c_port, buf, sizeof(*cmd) + cmd->length);

		deadline.val = get_time().val + 1 * SECOND;
		timeout = 0;

		/* Busy loop */
		while (gpio_get_level(ctx->irq_pin) == 0 && !timeout) {
			udelay(1 * MSEC);
			timeout = timestamp_expired(deadline, NULL);
		}

		if (timeout) {
			ccprintf("Response timeout\n");
			gpio_enable_interrupt(ctx->irq_pin);
			return EC_ERROR_TIMEOUT;
		}

		_i2c_read(ctx->i2c_port, buf, sizeof(*res));
		ccprintf("Response header: %s, %s, LEN=%d\n",
			_text_message_type(res->message_type),
			_text_instruction(res->instruction), res->length);

		if (res->length > sizeof(buf)) {
			ccprintf("Response size exceeds buffer\n");
			gpio_enable_interrupt(ctx->irq_pin);
			return EC_ERROR_OVERFLOW;
		}

		_i2c_read(ctx->i2c_port, buf, res->length);
		ccprintf("Response payload: status=0x%x tag=0x%x len=%d\n",
			 buf[0], buf[1], buf[2]);

		gpio_enable_interrupt(ctx->irq_pin);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ctn730, cc_ctn730,
			"<port> dump <tag>",
			"Control ctn730");
