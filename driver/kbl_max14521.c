/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Intersil ILS29035 light sensor driver
 */

#include "driver/kbl_max14521.h"
#include "i2c.h"

/* I2C interface */
#define MAX14521_I2C_ADDR       0xF0
#define MAX14521_REG_DEV_ID     0x00
#define MAX14251_DEVICE_ID      0xB2
#define MAX14521_REG_PWR_MODE   0x01
#define MAX14521_REG_EL_FREQ	0x02
#define MAX14521_REG_EL_SHAPE   0x03
#define MAX14521_REG_BST_FREQ   0x04
#define MAX14521_REG_AUDIO      0x05
#define MAX14521_REG_EL1_T_V    0x06
#define MAX14521_REG_EL2_T_V    0x07
#define MAX14521_REG_EL3_T_V    0x08
#define MAX14521_REG_EL4_T_V    0x09
#define MAX14521_REG_EL_UPDATE  0x0A

static const int kblight_step[] = {
        0xE0, 0xE8, 0xF3, 0xFD
};

static const int kblight_freq[] = {
	0x00, 0x73, 0x73, 0x73
};

static int curr_step;

int max14521_init(void)
{
	/*
	 * Tell it to read continually. This uses 70uA, as opposed to nearly
	 * zero, but it makes the hook/update code cleaner (we don't want to
	 * wait 90ms to read on demand while processing hook callbacks).
	 */
	int rv;

        curr_step = 0;

	rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_PWR_MODE, 0x01);

	if(rv)
		return rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_BST_FREQ, 0x04);

	if(rv)
		return rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL1_T_V, 0xE0);

	if(rv)
		return rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL_UPDATE, 0xE0);

	if(rv)
		return rv;

	return EC_SUCCESS;	
}

int max14521_set_kblight(int step)
{
	int rv;

        if(step < 0)
                return EC_ERROR;

        if(step >= 4)
                return EC_ERROR;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL_FREQ, kblight_freq[step]);

	if(rv)
		return rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL1_T_V, kblight_step[step]);

	if(rv)
		return rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL_UPDATE, kblight_step[step]);

	if(rv)
                return rv

        curr_step = step;

	return EC_SUCCESS;
}

int max14521_get_kblight(void)
{
        return curr_step;
}
