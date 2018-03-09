/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI LM3509 LED driver.
 */

#include "i2c.h"
#include "lm3509.h"
#include "host_command.h"
#include "ec_commands.h"
#include "util.h"

inline int lm3509_write(uint8_t reg, uint8_t val)
{
	return i2c_write8(I2C_PORT_KBLIGHT, LM3509_I2C_ADDR, reg, val);
}

int lm3509_poweron(void)
{
	int ret = 0;

	/* BIT= description
	 * [2]= set both main and seconfary current same, both control by BMAIN.
	 * [1]= enable secondary current sink.
	 * [0]= enable main current sink.
	 */
	ret |= lm3509_write(LM3509_REG_GP, 0x07);
	/* Brigntness register
	 * 0x00= 0%
	 * 0x1F= 100%
	 */
	ret |= lm3509_write(LM3509_REG_BMAIN, 0x1F);

	return ret;
}

int lm3509_poweroff(void)
{
	int ret = 0;

	ret |= lm3509_write(LM3509_REG_GP, 0x00);
	ret |= lm3509_write(LM3509_REG_BMAIN, 0x00);

	return ret;
}

static int lm3509_ec_command_set_KB_light(struct host_cmd_handler_args *args)
{
	const struct ec_params_pwm_set_keyboard_backlight *p = args->params;
	int ret = 0;
	int val = 0;
	int i, j, k;
	int lm3509_Brightness[32][2] = {
	{0, 0},
	{1, 125},
	{2, 625},
	{3, 1000},
	{4, 1125},
	{5, 1313},
	{6, 1688},
	{7, 2063},
	{8, 2438},
	{9, 2813},
	{10, 3125},
	{11, 375},
	{12, 4375},
	{13, 5525},
	{14, 6250},
	{15, 7500},
	{16, 8750},
	{17, 10000},
	{18, 12500},
	{19, 15000},
	{20, 16875},
	{21, 18750},
	{22, 22500},
	{23, 26250},
	{24, 31250},
	{25, 37500},
	{26, 43750},
	{27, 52500},
	{28, 61250},
	{29, 70000},
	{30, 87500},
	{31, 100000,} };

	for (i = 0; i < 32; i++) {
		j = lm3509_Brightness[i][1];
		k = lm3509_Brightness[i-1][1];
		if (j >= (p->percent)*1000) {
			if ((j - (p->percent)*1000) > ((p->percent)*1000 - k))
				val = lm3509_Brightness[i-1][0];
			else
				val = lm3509_Brightness[i][0];
			break;
		}
	}

	ret |= lm3509_write(LM3509_REG_BMAIN, val);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_SET_KEYBOARD_BACKLIGHT,
		     lm3509_ec_command_set_KB_light,
		     EC_VER_MASK(0));
