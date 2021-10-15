/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "peripheral_charger.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

/*
 * Configuration
 */

/* Print additional data */
#define CPS8100_DEBUG

#define CPS8100_I2C_ADDR_H		0x31
#define CPS8100_I2C_ADDR_L		0x30

/* High address registers (commands?) */
#define CPS8100_REGH_PASSWORD		0xf500
#define CPS8100_REGH_WRITE_MODE		0xf505
#define CPS8100_REGH_ADDRESS		0xf503

/* Registers */
#define CPS8100_REG_IC_INFO		0x000087dc

/* TODO: Check datasheet how to wake up and how long it takes to wake up. */
static const int _wake_up_delay_ms = 10;

/* Buffer size for i2c read & write */
#define CPS8100_MESSAGE_BUFFER_SIZE	0x20

struct cps8100_msg {
	/* Data address */
	uint8_t addr[2];
	/* Data. Can be used for read as well. */
	uint8_t data[2];
} __packed;

/* This driver isn't compatible with big endian. */
BUILD_ASSERT(__BYTE_ORDER__  == __ORDER_LITTLE_ENDIAN__);

#define CPRINTS(fmt, args...) cprints(CC_PCHG, "CPS8100: " fmt, ##args)

static int _i2c_write(int port, int addr, const uint8_t *buf, size_t len)
{
	int rv;

	/* Assumes a write is always 2 bytes. */
	rv = i2c_xfer(port, addr, buf, len, NULL, 0);

	if (rv) {
		msleep(_wake_up_delay_ms);
		rv = i2c_xfer(port, addr, buf, len, NULL, 0);
	}
	if (rv)
		CPRINTS("Failed to write: %d", rv);

	return rv;
}

static int cps8100_set_password(int port)
{
	uint8_t buf[4];

	buf[0] = 0xf5;
	buf[1] = 0x00;	/* Password register address */
	buf[2] = 0xe5;
	buf[3] = 0x19;	/* Password */

	return _i2c_write(port, CPS8100_I2C_ADDR_H, buf, 4);
}

static int cps8100_set_write_mode(int port, uint8_t mode)
{
	uint8_t buf[4];

	buf[0] = 0xf5;
	buf[1] = 0x05;
	buf[2] = mode;
	buf[3] = 0x00;

	return _i2c_write(port, CPS8100_I2C_ADDR_H, buf, 4);
}

static int cps8100_set_high_address(int port, uint32_t addr)
{
	uint8_t buf[4];

	buf[0] = 0xf5;
	buf[1] = 0x03;
	buf[2] = (addr >> 24) & 0xff;
	buf[3] = (addr >> 16) & 0xff;

	return _i2c_write(port, CPS8100_I2C_ADDR_H, buf, 4);
}

__maybe_unused static int cps8100_read8(int port, uint32_t reg, uint8_t *val)
{
	uint8_t buf[CPS8100_MESSAGE_BUFFER_SIZE];

	if (cps8100_set_password(port) ||
	    cps8100_set_write_mode(port, 0x00) ||
	    cps8100_set_high_address(port, reg))
		return EC_ERROR_UNKNOWN;

	/* Set low 16 bits of register address and read a byte. */
	buf[0] = (reg >> 8) & 0xff;
	buf[1] = (reg >> 0) & 0xff;

	return i2c_xfer(port, CPS8100_I2C_ADDR_L, buf, 2,
			(void *)val, sizeof(*val));
}

__maybe_unused static int cps8100_read16(int port, uint32_t reg, uint16_t *val)
{
	uint8_t buf[CPS8100_MESSAGE_BUFFER_SIZE];

	if (cps8100_set_password(port) ||
	    cps8100_set_write_mode(port, 0x01) ||
	    cps8100_set_high_address(port, reg))
		return EC_ERROR_UNKNOWN;

	/* Set low 16 bits of register address and read a byte. */
	buf[0] = (reg >> 8) & 0xff;
	buf[1] = (reg >> 0) & 0xff;

	return i2c_xfer(port, CPS8100_I2C_ADDR_L, buf, 2,
			(void *)val, sizeof(*val));
}

static int cps8100_read32(int port, uint32_t reg, uint32_t *val)
{
	uint8_t buf[CPS8100_MESSAGE_BUFFER_SIZE];

	if (cps8100_set_password(port) ||
	    cps8100_set_write_mode(port, 0x02) ||
	    cps8100_set_high_address(port, reg))
		return EC_ERROR_UNKNOWN;

	/* Set low 16 bits of register address and read a byte. */
	buf[0] = (reg >> 8) & 0xff;
	buf[1] = (reg >> 0) & 0xff;

	return i2c_xfer(port, CPS8100_I2C_ADDR_L, buf, 2,
			(void *)val, sizeof(*val));
}

static int cps8100_reset(struct pchg *ctx)
{
	gpio_set_level(GPIO_EC_QI_RESET_L, 0);
	/* TODO: Validate */
	udelay(15);
	gpio_set_level(GPIO_EC_QI_RESET_L, 1);

	return EC_SUCCESS_IN_PROGRESS;
}

static int cps8100_init(struct pchg *ctx)
{
	uint32_t ic_info;
	int rv;

	rv = cps8100_read32(ctx->cfg->i2c_port, CPS8100_REG_IC_INFO, &ic_info);
	if (rv != EC_SUCCESS) {
		rv = cps8100_read32(ctx->cfg->i2c_port, CPS8100_REG_IC_INFO,
				    &ic_info);
		if (rv)
			return rv;
	}

	CPRINTS("IC=0x%08x", ic_info);

	return EC_SUCCESS;
}

static int cps8100_enable(struct pchg *ctx, bool enable)
{
	return EC_SUCCESS;
}

static int cps8100_get_event(struct pchg *ctx)
{
	return EC_SUCCESS;
}

static int cps8100_get_soc(struct pchg *ctx)
{
	return EC_SUCCESS;
}

static int cps8100_update_open(struct pchg *ctx)
{
	return EC_SUCCESS;
}

static int cps8100_update_write(struct pchg *ctx)
{
	return EC_SUCCESS;
}

static int cps8100_update_close(struct pchg *ctx)
{
	return EC_SUCCESS;
}

const struct pchg_drv cps8100_drv = {
	.reset = cps8100_reset,
	.init = cps8100_init,
	.enable = cps8100_enable,
	.get_event = cps8100_get_event,
	.get_soc = cps8100_get_soc,
	.update_open = cps8100_update_open,
	.update_write = cps8100_update_write,
	.update_close = cps8100_update_close,
};

static int cc_cps8100(int argc, char **argv)
{
	struct pchg *ctx;
	char *end;
	int port;

	if (argc < 4)
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[1], &end, 0);
	if (*end || port < 0 || pchg_count <= port)
		return EC_ERROR_PARAM2;

	ctx = &pchgs[port];

	if (!strcasecmp(argv[2], "reset")) {
		cps8100_reset(ctx);
		cps8100_init(ctx);
	} else {
		return EC_ERROR_PARAM2;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(cps8100, cc_cps8100,
			"<port> reset",
			"Control CPS8100");
