/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "rgb_keyboard.h"
#include "spi.h"
#include "stddef.h"
#include "string.h"

#undef _DEBUG

#define CPRINTF(fmt, args...) cprintf(CC_RGBKBD, "RGBKBD: " fmt, ##args)
#define CPRINTS(fmt, args...) cprints(CC_RGBKBD, "RGBKBD: " fmt, ##args)

#define SPI(id) (&(spi_devices[id]))

#define IS31FL3743B_ROW_SIZE	6
#define IS31FL3743B_COL_SIZE	11
#define IS31FL3743B_GRID_SIZE	(IS31FL3743B_COL_SIZE * IS31FL3743B_ROW_SIZE)
#define IS31FL3743B_BUF_SIZE	(RGB * IS31FL3743B_GRID_SIZE)

#define IS31FL3743B_CMD_ID	0b101
#define IS31FL3743B_PAGE_PWM	0
#define IS31FL3743B_PAGE_SCALE	1

#define IS31FL3743B_REG_CONFIG		0x00
#define IS31FL3743B_REG_GCC		0x01
#define IS31FL3743B_REG_PD_PU		0x02
#define IS31FL3743B_REG_SPREAD_SPECTRUM	0x25

struct is31fl3743b_cmd {
	uint8_t page: 4;
	uint8_t id: 3;
	uint8_t read: 1;
} __packed;

struct is31fl3743b_msg {
	struct is31fl3743b_cmd cmd;
	uint8_t addr;
	uint8_t payload[];
} __packed;

static int _get_reg8(struct rgbkbd *ctx, uint8_t addr, uint8_t *value)
{
	uint8_t buf[8];
	struct is31fl3743b_msg *msg = (void *)buf;
	const int frame_len = sizeof(*msg);

	msg->cmd.read = 1;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = 2;
	msg->addr = addr;

	return spi_transaction(SPI(ctx->cfg->spi), buf, frame_len, value, 1);
}

static int _reset(struct rgbkbd *ctx)
{
	return EC_SUCCESS;
}

static int _set_config(struct rgbkbd *ctx, uint8_t value)
{
	uint8_t buf[8];
	struct is31fl3743b_msg *msg = (void *)buf;
	const int frame_len = sizeof(*msg) + 1;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = 2;
	msg->addr = IS31FL3743B_REG_CONFIG;
	msg->payload[0] = value;

	return spi_transaction(SPI(ctx->cfg->spi), buf, frame_len, NULL, 0);
}

static int _enable(struct rgbkbd *ctx, bool enable)
{
	gpio_set_level(GPIO_RGBKBD_SDB_L, enable ? 1 : 0);

	return EC_SUCCESS;
}

static int _set_color(struct rgbkbd *ctx, uint8_t offset, struct rgb_s *color,
		      uint8_t len)
{
	uint8_t buf[sizeof(struct is31fl3743b_msg) + IS31FL3743B_BUF_SIZE];
	struct is31fl3743b_msg *msg = (void *)buf;
	const int frame_len = len * RGB + sizeof(*msg);
	const int frame_offset = offset * RGB;
	int i;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = IS31FL3743B_PAGE_PWM;

	if (frame_offset + frame_len > sizeof(buf))
		return EC_ERROR_OVERFLOW;

	msg->addr = frame_offset + 1;	/* Register addr base is 1. */
	for (i = 0; i < len; i++) {
		msg->payload[i * RGB +0] = color[i].r;
		msg->payload[i * RGB +1] = color[i].g;
		msg->payload[i * RGB +2] = color[i].b;
	}

	return spi_transaction(SPI(ctx->cfg->spi), buf, frame_len, NULL, 0);
}

static int _set_scale(struct rgbkbd *ctx, uint8_t offset, uint8_t scale,
		      uint8_t len)
{
	uint8_t buf[sizeof(struct is31fl3743b_msg) + IS31FL3743B_BUF_SIZE];
	struct is31fl3743b_msg *msg = (void *)buf;
	const int frame_len = len * RGB + sizeof(*msg);

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = IS31FL3743B_PAGE_SCALE;

	if (offset + frame_len > sizeof(buf))
		return EC_ERROR_OVERFLOW;

	msg->addr = offset + 1;	/* Address base is 1. */
	memset(msg->payload, scale, len * RGB);

	return spi_transaction(SPI(ctx->cfg->spi), buf, frame_len, NULL, 0);
}

static int _set_gcc(struct rgbkbd *ctx, uint8_t level)
{
	uint8_t buf[8];
	struct is31fl3743b_msg *msg = (void *)buf;
	const int frame_len = sizeof(*msg) + 1;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = 2;
	msg->addr = IS31FL3743B_REG_GCC;
	msg->payload[0] = level;

	return spi_transaction(SPI(ctx->cfg->spi), buf, frame_len, NULL, 0);
}

static int _init(struct rgbkbd *ctx)
{
	int rv;

	rv = _enable(ctx, true);
	rv |= _set_config(ctx, 0x08 | 0);
	rv |= _set_gcc(ctx, 0x80);
	rv |= _set_scale(ctx, 0x00, IS31FL3743B_BUF_SIZE, 0x7f);
	rv |= _set_config(ctx, 0x08 | 1);
	if (rv)
		return rv;

	if (IS_ENABLED(_DEBUG)) {
		uint8_t val;
		rv = _get_reg8(ctx, IS31FL3743B_REG_PD_PU, &val);
		CPRINTS("PD/PU. val=0x%02x (rv=%d)", val, rv);
	}

	return EC_SUCCESS;
}

const struct rgbkbd_drv is31fl3743b_drv = {
	.reset = _reset,
	.init = _init,
	.enable = _enable,
	.set_color = _set_color,
	.set_scale = _set_scale,
	.set_gcc = _set_gcc,
};
