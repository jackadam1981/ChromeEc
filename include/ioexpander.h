/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_IOEXPANDER_H
#define __CROS_EC_IOEXPANDER_H

/* IO expander signal definition structure */
struct ioex_info {
	/* Signal name */
	const char *name;

	/* IO expander port number */
	uint16_t ioex;

	/* IO port number in IO expander */
	uint16_t port;

	/* Bitmask on that port (1 << N) */
	uint32_t mask;

	/* Flags - the same as the GPIO flags */
	uint32_t flags;
};

/* Signal information from board.c.  Must match order from enum ioex_signal. */
extern const struct ioex_info ioex_list[];

/*
 *  Define the IO expander IO in gpio.inc by the format:
 *    IOEX(name, EXPIN(ioex_port, port, offset), flags)
 *      - name: the name of this IO pin
 *      - EXPIN(ioex, port, offset)
 *         - ioex: the IO expander port (defined in board.c) this IO
 *                 pin belongs to.
 *         - port: the port number in the IO expander chip.
 *         - offset: the bit offset in the port above.
 *      - flags: the same as the flags of GPIO.
 *
 */
#define IOEX_EXPIN(ioex, port, index) (ioex), (port), BIT(index)

struct ioexpander_drv {
	/* Initialize IO expander chip/driver */
	int (*init)(int ioex);
	/* Get the current level of the IOEX pin */
	int (*get_level)(int ioex, int port, int mask, int *val);
	/* Set the level of the IOEX pin */
	int (*set_level)(int ioex, int port, int mask, int val);
	/* Get flags for the IOEX pin */
	int (*get_flags_by_mask)(int ioex, int port, int mask, int *flags);
	/* Set flags for the IOEX pin */
	int (*set_flags_by_mask)(int ioex, int port, int mask, int flags);
};

struct ioexpander_config_t {
	/* Physical I2C port connects to the IO expander chip. */
	int i2c_host_port;
	/* I2C slave address */
	int i2c_slave_addr;
	/*
	 * The extra variable used to store information which may be required
	 * by the IO expander chip.
	 */
	int chip_info;
	/*
	 * Pointer to the specific IO expander chip's ops defined in
	 * the struct ioexpander_drv.
	 */
	const struct ioexpander_drv *drv;
};

extern struct ioexpander_config_t ioex_config[];

/* Get flags for the IOEX pin by mask */
int ioex_get_flags_by_mask(int ioex, int port, int mask, int *flags);
/* Set flags for the IOEX pin by mask */
int ioex_set_flags_by_mask(int ioex, int port, int mask, int flags);
/* Get flags for the IOEX signal */
int ioex_get_flags(enum ioex_signal signal, int *flags);
/* Set flags for the IOEX signal */
int ioex_set_flags(enum ioex_signal signal, int flags);
/* Get the current level of the IOEX signal */
int ioex_get_level(enum ioex_signal signal, int *val);
/* Set the level of the IOEX signal */
int ioex_set_level(enum ioex_signal signal, int value);
/* Initialize IO expander chip/driver */
int ioex_init(int ioex);
/* Get the name for the IOEX signal */
const char *ioex_get_name(enum ioex_signal signal);
#endif /* __CROS_EC_IOEXPANDER_H */

