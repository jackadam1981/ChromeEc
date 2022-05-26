/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <string.h>

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "rgb_keyboard.h"
#include "stddef.h"
#include "timer.h"
#include "tlc59116f.h"

#define CPRINTF(fmt, args...) cprintf(CC_RGBKBD, "TLC59116F: " fmt, ##args)
#define CPRINTS(fmt, args...) cprints(CC_RGBKBD, "TLC59116F: " fmt, ##args)

static int tlc59116f_read(struct rgbkbd *ctx, uint8_t addr, uint8_t *value)
{
	return i2c_xfer(ctx->cfg->i2c, TLC59116F_I2C_ADDR_FLAG,
			&addr, sizeof(addr), value, sizeof(*value));
}

static int tlc59116f_write(struct rgbkbd *ctx, uint8_t addr, uint8_t value)
{
	uint8_t buf[2] = {
		[0] = addr,
		[1] = value,
	};

	return i2c_xfer(ctx->cfg->i2c, TLC59116F_I2C_ADDR_FLAG,
			buf, sizeof(buf), NULL, 0);
}

static int tlc59116f_reset(struct rgbkbd *ctx)
{
	return i2c_write8(ctx->cfg->i2c, TLC59116F_RESET, 0xA5, 0x5A);
}

static int tlc59116f_init(struct rgbkbd *ctx)
{
	int i, j, rv;

	for (i = TLC59116F_PWM0; i <= TLC59116F_PWM15; i++) {
		rv = tlc59116f_write(ctx, i, 0x00);
		if (rv) {
			return rv;
		}
	}

	for (j = TLC59116F_LEDOUT0; j <= TLC59116F_LEDOUT3; j++) {
		rv = tlc59116f_write(ctx, j, TLC59116_LEDOUT_PWM);
		if (rv) {
			return rv;
		}
	}

	rv = tlc59116f_write(ctx, TLC59116F_MODE1, 0x01);
	if (rv) {
		CPRINTS("Failed to set TLC59116F normal mode");
		return rv;
	}

	return EC_SUCCESS;
}

static int tlc59116f_enable(struct rgbkbd *ctx, bool enable)
{
	uint8_t cfg;
	int rv;

	rv = tlc59116f_read(ctx, TLC59116F_MODE1, &cfg);
	if (rv) {
		return rv;
	}

	WRITE_BIT(cfg, 4, !enable);
	return tlc59116f_write(ctx, TLC59116F_MODE1, cfg);
}

static int tlc59116f_set_color(struct rgbkbd *ctx, uint8_t offset,
			     struct rgb_s *color, uint8_t len)
{
	const int frame_offset = offset * SIZE_OF_RGB;
	int i, rv;

	if (len == 1) {
		rv = tlc59116f_write(ctx, TLC59116F_PWM0 + frame_offset,
					color[0].r);
		rv |= tlc59116f_write(ctx, TLC59116F_PWM1 + frame_offset,
					color[0].g);
		rv |= tlc59116f_write(ctx, TLC59116F_PWM2 + frame_offset,
					color[0].b);
		if (rv)
			return rv;
	} else {
		for (i = 0; i < len; i++) {
			rv = tlc59116f_write(ctx, TLC59116F_PWM0 + i * 3,
					color[i].r);
			rv |= tlc59116f_write(ctx, TLC59116F_PWM1 + i * 3,
					color[i].g);
			rv |= tlc59116f_write(ctx, TLC59116F_PWM2 + i * 3,
					color[i].b);
			if (rv)
				return rv;
		}
	}

	return EC_SUCCESS;
}

static int tlc59116f_set_scale(struct rgbkbd *ctx, uint8_t offset,
			     uint8_t scale, uint8_t len)
{
	return EC_SUCCESS;
}

static int tlc59116f_set_gcc(struct rgbkbd *ctx, uint8_t level)
{
	int j, rv;

	for (j = TLC59116F_LEDOUT0; j <= TLC59116F_LEDOUT3; j++) {
		rv = tlc59116f_write(ctx, j, TLC59116_LEDOUT_GROUP);
		if (rv) {
			return rv;
		}
	}

	return tlc59116f_write(ctx, TLC59116F_GRPPWM, level);
}

const struct rgbkbd_drv tlc59116f_drv = {
	.reset = tlc59116f_reset,
	.init = tlc59116f_init,
	.enable = tlc59116f_enable,
	.set_color = tlc59116f_set_color,
	.set_scale = tlc59116f_set_scale,
	.set_gcc = tlc59116f_set_gcc,
};
