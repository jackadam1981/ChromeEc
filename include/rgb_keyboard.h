/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "stdbool.h"

/* Use this instead of '3' for readability where applicable. */
#define RGB		sizeof(struct rgb_s)

struct rgbkbd_cfg {
	/* Driver for LED IC */
	const struct rgbkbd_drv * const drv;
	/* SPI/I2C port (i.e. index of spi_devices[], i2c_ports[]) */
	union {
		uint8_t i2c;
		uint8_t spi;
	};
	/* Grid size */
	uint8_t col_len;
	uint8_t row_len;
};

struct rgbkbd {
	/* Static configuration */
	const struct rgbkbd_cfg * const cfg;
	/* Current state of the port */
	enum rgbkbd_state state;
};

struct rgbkbd_drv {
	/* Reset charger chip. */
	int (*reset)(struct rgbkbd *ctx);
	/* Initialize the charger. */
	int (*init)(struct rgbkbd *ctx);
	/* Enable/disable the charger. Usually disabled means stand-by. */
	int (*enable)(struct rgbkbd *ctx, bool enable);

	/**
	 * Set the colors of multiple RGB-LEDs.
	 *
	 * @param ctx    Context.
	 * @param offset Starting LED position.
	 * @param color  Array of colors to set. Must be as long as <len>.
	 * @param len    Length of <color> array.
	 * @return enum ec_error_list.
	 */
	int (*set_color)(struct rgbkbd *ctx, uint8_t offset,
			 struct rgb_s *color, uint8_t len);
	/**
	 * Set the scale of multiple LEDs
	 *
	 * @param ctx    Context.
	 * @param offset Starting LED position.
	 * @param scale  Scale to be set.
	 * @param len    Length of LEDs to be set.
	 * @return enum ec_error_list
	 */
	int (*set_scale)(struct rgbkbd *ctx, uint8_t offset,
			 uint8_t scale, uint8_t len);
	/**
	 * Set global current control.
	 *
	 * @param level Global current control to set.
	 * @return enum ec_error_list.
	 */
	int (*set_gcc)(struct rgbkbd *ctx, uint8_t level);
};

extern struct rgbkbd rgbkbds[];
extern const uint8_t rgbkbd_count;
