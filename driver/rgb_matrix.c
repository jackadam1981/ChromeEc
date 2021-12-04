/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "board.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#undef DEBUG

/* Console output macros */
#define CPUTS(outstr) cputs(CC_RGB_MATRIX, outstr)
#define CPRINTF(fmt, args...) cprintf(CC_RGB_MATRIX, "RGBM: " fmt, ##args)
#define CPRINTS(fmt, args...) cprints(CC_RGB_MATRIX, "RGBM: " fmt, ##args)

#define SPI(id) (&(spi_devices[id]))

#define GRID_ROW	6
#define GRID_COL	11
#define GRID_SIZE	(GRID_ROW * GRID_COL)
#define RGB		sizeof(struct rgb_s)

#define IS31FL3743B_CMD_ID	0b101
#define IS31FL3743B_PAGE_PWM	0
#define IS31FL3743B_PAGE_SCLAE	1

#define IS31FL3743B_REG_CONFIG		0x00
#define IS31FL3743B_REG_GCC		0x01
#define IS31FL3743B_REG_PD_PU		0x02
#define IS31FL3743B_REG_SPREAD_SPECTRUM	0x25

struct is31fl_cmd {
	uint8_t page: 4;
	uint8_t id: 3;
	uint8_t read: 1;
} __packed;

struct is31fl_msg {
	struct is31fl_cmd cmd;
	uint8_t addr;
	uint8_t payload[];
} __packed;

static struct rgb_s grid[2][GRID_SIZE];

/**
 * Set colors of multiple RGB-LEDs (or an entire matrix).
 *
 * @param spi    SPI chip ID (or chip select).
 * @param offset Starting LED position.
 * @param color  Array of colors to set. Must be as long as <len>.
 * @param len    Length of <color> array.
 * @return
 */
static int rgb_set_color(uint8_t spi,
			 uint8_t offset, struct rgb_s *color, uint8_t len)
{
	uint8_t buf[sizeof(struct is31fl_msg) + RGB * GRID_SIZE];
	struct is31fl_msg *msg = (void *)buf;
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

	return spi_transaction(SPI(spi), buf, frame_len, NULL, 0);
}

/**
 * Set PWM for a single register or multiple registers.
 *
 * @param addr  Address of register. Base is 0.
 * @param len   Number of registers to set.
 * @param level PWM level to set.
 * @return
 */
static int rgb_set_pwm(uint8_t spi, uint8_t addr, uint8_t len, uint8_t level)
{
	uint8_t buf[sizeof(struct is31fl_msg) + RGB * GRID_SIZE];
	struct is31fl_msg *msg = (void *)buf;
	int i;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = IS31FL3743B_PAGE_PWM;

	if (addr + len + sizeof(*msg) > sizeof(buf))
		return EC_ERROR_OVERFLOW;

	msg->addr = addr + 1;	/* Address base is 1. */
	for (i = 0; i < len; i++)
		msg->payload[i] = level;

	return spi_transaction(SPI(spi), buf, len + sizeof(*msg), NULL, 0);
}

static int rgb_set_scale(uint8_t spi, uint8_t addr, uint8_t len, uint8_t level)
{
	uint8_t buf[sizeof(struct is31fl_msg)
		    + 3 /* RGB */ * 6 /* Columns */ * 11 /* Rows */];
	struct is31fl_msg *msg = (void *)buf;
	int i;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = IS31FL3743B_PAGE_SCLAE;

	if (addr + len + sizeof(*msg) > sizeof(buf))
		return EC_ERROR_OVERFLOW;

	msg->addr = addr + 1;	/* Address base is 1. */
	for (i = 0; i < len; i++)
		msg->payload[i] = level;

	return spi_transaction(SPI(spi), buf, len + sizeof(*msg), NULL, 0);
}

static int rgb_set_gcc(uint8_t spi, uint8_t level)
{
	uint8_t buf[8];
	struct is31fl_msg *msg = (void *)buf;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = 2;
	msg->addr = IS31FL3743B_REG_GCC;
	msg->payload[0] = level;

	return spi_transaction(SPI(spi), buf, sizeof(*msg) + 1, NULL, 0);
}

static int rgb_set_config(uint8_t spi, uint8_t value)
{
	uint8_t buf[8];
	struct is31fl_msg *msg = (void *)buf;
	const int len = 1;

	msg->cmd.read = 0;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = 2;
	msg->addr = IS31FL3743B_REG_CONFIG;
	msg->payload[0] = value;

	return spi_transaction(SPI(spi), buf, sizeof(*msg) + len, NULL, 0);
}

static int rgb_get_pd_pu(uint8_t spi, uint8_t *value)
{
	uint8_t buf[8];
	struct is31fl_msg *msg = (void *)buf;

	msg->cmd.read = 1;
	msg->cmd.id = IS31FL3743B_CMD_ID;
	msg->cmd.page = 2;
	msg->addr = IS31FL3743B_REG_PD_PU;

	return spi_transaction(SPI(spi), buf, sizeof(*msg), value, 1);
}

void rgb_matrix_task(void *u)
{
	uint32_t event;
	uint8_t val;
	struct rgb_s color = {};
	int rv;

	gpio_set_level(GPIO_RGB_MATRIX_POWER, 1);
	msleep(10);

	if (IS_ENABLED(DEBUG)) {
		rv = rgb_get_pd_pu(0, &val);
		CPRINTS("Get PD/PU. val=0x%02x (rv=%d)", val, rv);
		rv = rgb_set_pwm(0, 0x00, RGB * GRID_SIZE, 0xb5);
		CPRINTS("Set PWM. rv=%d", rv);
	}

	gpio_set_level(GPIO_RGB_MATRIX_SDB_L, 0);

	/* Disable software shutdown. */
	rv = rgb_set_config(0, 0x08 | 1);
	rv |= rgb_set_config(1, 0x08 | 1);
	CPRINTS("Set Config. rv=%d", rv);

	/* Set Global Current Control register */
	rv = rgb_set_gcc(0, 0xff);
	rv |= rgb_set_gcc(1, 0xff);
	CPRINTS("Set GCC. rv=%d", rv);

	rv = rgb_set_scale(0, 0x00, RGB * GRID_SIZE, 0x7f);
	rv |= rgb_set_scale(1, 0x00, RGB * GRID_SIZE, 0x7f);
	CPRINTS("Set Scale. rv=%d", rv);

	gpio_set_level(GPIO_RGB_MATRIX_SDB_L, 1);

	while (1) {
		int i;
		const int step = 32;

		event = task_wait_event(100 * MSEC);
		CPRINTS("event=0x%08x", event);
		rgb_set_color(0, 0, grid[0], GRID_SIZE);
		rgb_set_color(1, 0, grid[1], GRID_SIZE);
		color.r += step;
		if (color.r == 0) {
			color.g += step;
			if (color.g == 0)
				color.b += step;
		}

		for (i = 1; i < GRID_SIZE; i++)
			grid[1][GRID_SIZE-i] = grid[1][GRID_SIZE-i-1];
		grid[1][0] = grid[0][GRID_SIZE-1];
		for (i = 1; i < GRID_SIZE; i++)
			grid[0][GRID_SIZE-i] = grid[0][GRID_SIZE-i-1];
		grid[0][0] = color;
	}
}
