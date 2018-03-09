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

inline int lm3509_read(uint8_t reg, int *val)
{
	return i2c_read8(I2C_PORT_KBLIGHT, LM3509_I2C_ADDR, reg, val);
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

const int lm3509_brightness[32] = {
	0, 125, 625, 1000, 1125, 1313,
	1688, 2063, 2438, 2813, 3125,
	3750, 4375, 5525, 6250, 7500,
	8750, 10000, 12500, 15000, 16875,
	18750, 22500, 26250, 31250, 37500,
	43750, 52500, 61250, 70000, 87500, 100000};

/*****************************************************************************/
/* Host commands */

static int hc_set_kb_light(struct host_cmd_handler_args *args)
{
	const struct ec_params_pwm_set_keyboard_backlight *p = args->params;
	int ret = 0;
	int val = 0;
	int i, level_high, level_low;

	for (i = 1; i < 32; i++) {
		/* If level set by host cmd is zero, break the loop */
		if ((p->percent) == 0) {
			val = 0;
			break;
		}

		level_high = lm3509_brightness[i];
		level_low = lm3509_brightness[i-1];
		/* Compare the level set by host cmd with lm3509 brightness
		 * level and calculate the nearest lm3509 brightness value
		 */
		if (level_high >= (p->percent)*1000) {
			if ((level_high - (p->percent)*1000) >
				((p->percent)*1000 - level_low))
				val = i-1;
			else
				val = i;
			break;
		}
	}

	ret |= lm3509_write(LM3509_REG_BMAIN, val);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_SET_KEYBOARD_BACKLIGHT,
		     hc_set_kb_light,
		     EC_VER_MASK(0));

static int hc_get_kb_light(struct host_cmd_handler_args *args)
{
	struct ec_response_pwm_get_keyboard_backlight *r = args->response;
	int ret = 0;
	int val = 0;

	ret = lm3509_read(LM3509_REG_BMAIN, &val);
	if (!ret) {
		/* Round return vlaue to the nearest integer */
		r->percent = (lm3509_brightness[val&0x1F]+500)/1000;
		if ((val&0x1F) > 0)
			r->enabled = 1;

		args->response_size = sizeof(*r);

		return EC_RES_SUCCESS;
	} else
		return EC_RES_ERROR;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_GET_KEYBOARD_BACKLIGHT,
		     hc_get_kb_light,
		     EC_VER_MASK(0));

/*****************************************************************************/
