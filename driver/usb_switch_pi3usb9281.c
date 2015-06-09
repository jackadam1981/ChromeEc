/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB3281 USB port switch driver.
 */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "pi3usb9281.h"
#include "util.h"

 /* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* 8-bit I2C address */
#define PI3USB9281_I2C_ADDR (0x25 << 1)

/* Delay values */
#define PI3USB9281_SW_RESET_DELAY 20

static inline void select_chip(struct pi3usb9281_config *chip)
{
	if (chip->mux_lock) {
		mutex_lock(chip->mux_lock);
		gpio_set_level(chip->mux_gpio, chip->mux_gpio_level);
	}
}

static inline void unselect_chip(struct pi3usb9281_config *chip)
{
	if (chip->mux_lock)
		/* Just release the mutex, no need to change the mux gpio */
		mutex_unlock(chip->mux_lock);
}

void pi3usb9281_init(struct pi3usb9281_config *chip)
{
	uint8_t dev_id;

	dev_id = pi3usb9281_read(chip, PI3USB9281_REG_DEV_ID);

	if (dev_id != PI3USB9281_DEV_ID && dev_id != PI3USB9281_DEV_ID_A)
		CPRINTS("PI3USB9281 invalid ID 0x%02x", dev_id);
}

uint8_t pi3usb9281_read(struct pi3usb9281_config *chip, uint8_t reg)
{
	int res, val;

	select_chip(chip);
	res = i2c_read8(chip->i2c_port, PI3USB9281_I2C_ADDR, reg, &val);
	unselect_chip(chip);

	if (res)
		return 0xee;

	return val;
}

int pi3usb9281_write(struct pi3usb9281_config *chip, uint8_t reg, uint8_t val)
{
	int res;

	select_chip(chip);
	res = i2c_write8(chip->i2c_port, PI3USB9281_I2C_ADDR, reg, val);
	unselect_chip(chip);

	if (res)
		CPRINTS("PI3USB9281 I2C write failed");
	return res;
}

/* Write control register, taking care to correctly set reserved bits. */
static int pi3usb9281_write_ctrl(struct pi3usb9281_config *chip, uint8_t ctrl)
{
	return pi3usb9281_write(chip, PI3USB9281_REG_CONTROL,
				(ctrl & PI3USB9281_CTRL_MASK) |
				PI3USB9281_CTRL_RSVD_1);
}

int pi3usb9281_enable_interrupts(struct pi3usb9281_config *chip)
{
	uint8_t ctrl = pi3usb9281_read(chip, PI3USB9281_REG_CONTROL);

	if (ctrl == 0xee)
		return EC_ERROR_UNKNOWN;

	return pi3usb9281_write_ctrl(chip, ctrl & ~PI3USB9281_CTRL_INT_DIS);
}

int pi3usb9281_disable_interrupts(struct pi3usb9281_config *chip)
{
	uint8_t ctrl = pi3usb9281_read(chip, PI3USB9281_REG_CONTROL);
	int rv;

	if (ctrl == 0xee)
		return EC_ERROR_UNKNOWN;

	rv = pi3usb9281_write_ctrl(chip, ctrl | PI3USB9281_CTRL_INT_DIS);
	pi3usb9281_get_interrupts(chip);
	return rv;
}

int pi3usb9281_set_interrupt_mask(struct pi3usb9281_config *chip, uint8_t mask)
{
	return pi3usb9281_write(chip, PI3USB9281_REG_INT_MASK, ~mask);
}

int pi3usb9281_get_interrupts(struct pi3usb9281_config *chip)
{
	return pi3usb9281_read(chip, PI3USB9281_REG_INT);
}

int pi3usb9281_get_device_type(struct pi3usb9281_config *chip)
{
	return pi3usb9281_read(chip, PI3USB9281_REG_DEV_TYPE) & 0x77;
}

int pi3usb9281_get_charger_status(struct pi3usb9281_config *chip)
{
	return pi3usb9281_read(chip, PI3USB9281_REG_CHG_STATUS) & 0x1f;
}

int pi3usb9281_get_ilim(int device_type, int charger_status)
{
	/* Limit USB port current. 500mA for not listed types. */
	int current_limit_ma = 500;

	if (charger_status & PI3USB9281_CHG_CAR_TYPE1 ||
	    charger_status & PI3USB9281_CHG_CAR_TYPE2)
		current_limit_ma = 3000;
	else if (charger_status & PI3USB9281_CHG_APPLE_1A)
		current_limit_ma = 1000;
	else if (charger_status & PI3USB9281_CHG_APPLE_2A)
		current_limit_ma = 2000;
	else if (charger_status & PI3USB9281_CHG_APPLE_2_4A)
		current_limit_ma = 2400;
	else if (device_type & PI3USB9281_TYPE_CDP)
		current_limit_ma = 1500;
	else if (device_type & PI3USB9281_TYPE_DCP)
		current_limit_ma = 500;

	return current_limit_ma;
}

int pi3usb9281_get_vbus(struct pi3usb9281_config *chip)
{
	int vbus = pi3usb9281_read(chip, PI3USB9281_REG_VBUS);
	if (vbus == 0xee)
		return EC_ERROR_UNKNOWN;

	return !!(vbus & 0x2);
}

int pi3usb9281_reset(struct pi3usb9281_config *chip)
{
	int rv = pi3usb9281_write(chip, PI3USB9281_REG_RESET, 0x1);

	if (!rv)
		/* Reset takes ~15ms. Wait for 20ms to be safe. */
		msleep(PI3USB9281_SW_RESET_DELAY);

	return rv;
}

int pi3usb9281_set_switch_manual(struct pi3usb9281_config *chip, int val)
{
	uint8_t ctrl = pi3usb9281_read(chip, PI3USB9281_REG_CONTROL);

	if (ctrl == 0xee)
		return EC_ERROR_UNKNOWN;

	if (val)
		ctrl &= ~PI3USB9281_CTRL_AUTO;
	else
		ctrl |= PI3USB9281_CTRL_AUTO;

	return pi3usb9281_write_ctrl(chip, ctrl);
}

int pi3usb9281_set_pins(struct pi3usb9281_config *chip, uint8_t val)
{
	return pi3usb9281_write(chip, PI3USB9281_REG_MANUAL, val);
}

int pi3usb9281_set_switches(struct pi3usb9281_config *chip, int open)
{
	uint8_t ctrl = pi3usb9281_read(chip, PI3USB9281_REG_CONTROL);

	if (ctrl == 0xee)
		return EC_ERROR_UNKNOWN;

	if (open)
		ctrl &= ~PI3USB9281_CTRL_SWITCH_AUTO;
	else
		ctrl |= PI3USB9281_CTRL_SWITCH_AUTO;

	return pi3usb9281_write_ctrl(chip, ctrl);
}
