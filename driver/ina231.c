/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI INA231 Current/Power monitor driver.
 */

#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "timer.h"
#include "ina231.h"
#include "uart.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* 8-bit I2C base address */
#define INA231_I2C_ADDR (0x40 << 1)

uint16_t ina231_read(uint8_t idx, uint8_t reg)
{
	int res;
	int val;
	uint8_t addr = INA231_I2C_ADDR | (idx << 1);

	res = i2c_read16(I2C_PORT_MASTER, addr, reg, &val);
	if (res)
		return 0x0bad;
	return (val >> 8) | ((val & 0xff) << 8);
}

int ina231_write(uint8_t idx, uint8_t reg, uint16_t val)
{
	int res;
	uint8_t addr = INA231_I2C_ADDR | (idx << 1);
	uint16_t be_val = (val >> 8) | ((val & 0xff) << 8);

	res = i2c_write16(I2C_PORT_MASTER, addr, reg, be_val);
	if (res)
		CPRINTS("INA231 I2C write failed");
	return res;
}

static void ina231_dump(uint8_t idx)
{
	uint16_t cfg = ina231_read(idx, INA231_REG_CONFIG);
	int16_t sv = ina231_read(idx, INA231_REG_SHUNT_VOLT);
	uint16_t bv = ina231_read(idx, INA231_REG_BUS_VOLT);
	uint16_t pow = ina231_read(idx, INA231_REG_POWER);
	int16_t curr = ina231_read(idx, INA231_REG_CURRENT);
	uint16_t calib = ina231_read(idx, INA231_REG_CALIB);
	uint16_t mask = ina231_read(idx, INA231_REG_MASK);
	uint16_t alert = ina231_read(idx, INA231_REG_ALERT);

	ccprintf("Configuration: %04x\n", cfg);
	ccprintf("Shunt voltage: %04x => %d uV\n", sv, (int)sv * 25 / 10);
	ccprintf("Bus voltage  : %04x => %d mV\n", bv, (int)bv * 125 / 100);
	ccprintf("Power        : %04x\n", pow);
	ccprintf("Current      : %04x => mA\n", curr);
	ccprintf("Calibration  : %04x\n", calib);
	ccprintf("Mask/Enable  : %04x\n", mask);
	ccprintf("Alert limit  : %04x\n", alert);
}

/*****************************************************************************/
/* Console commands */

static int command_ina(int argc, char **argv)
{
	char *e;
	int idx;
	uint16_t val;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	idx = strtoi(argv[1], &e, 10);
	if (*e)
		return EC_ERROR_PARAM1;

	if (2 == argc) { /* dump all registers */
		ina231_dump(idx);
		return EC_SUCCESS;
	} else if (4 == argc) {
		val = strtoi(argv[3], &e, 16);
		if (*e)
			return EC_ERROR_PARAM3;

		if (!strcasecmp(argv[2], "config")) {
			ina231_write(idx, INA231_REG_CONFIG, val);
		} else if (!strcasecmp(argv[2], "calib")) {
			ina231_write(idx, INA231_REG_CALIB, val);
		} else if (!strcasecmp(argv[2], "mask")) {
			ina231_write(idx, INA231_REG_MASK, val);
		} else if (!strcasecmp(argv[2], "alert")) {
			ina231_write(idx, INA231_REG_ALERT, val);
		} else { /* read one register */
			ccprintf("Invalid register: %s\n", argv[1]);
			return EC_ERROR_INVAL;
		}
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(ina, command_ina,
			"[config|calib|mask|alert]",
			"INA231 power/current sensing",
			NULL);
