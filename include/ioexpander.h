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
	uint16_t ioex_port;

	/* IO port number in IO expander */
	uint16_t port;

	/* Bitmask on that port (1 << N; 0 = signal not implemented) */
	uint32_t mask;

	/* Flags - the same as the GPIO flags */
	uint32_t flags;
};

/* Signal information from board.c.  Must match order from enum ioex_signal. */
extern const struct ioex_info ioex_list[];

#define DUMMY_IOEX_BANK       0
#define IOEX_EXPIN(ioex_port, port, index) (ioex_port), (port), BIT(index)

struct ioexpander_drv {
	int (*init)(int expander);
	int (*get_level)(int expander, int port, int mask, int *val);
	int (*set_level)(int expander, int port, int mask, int val);
	int (*get_flags_by_mask)(int expander, int port, int mask, int *flags);
	int (*set_flags_by_mask)(int expander, int port, int mask, int flags);
	int chip_info;
};

struct ioexpander_config_t {
	int i2c_host_port;
	int i2c_slave_addr;
	int chip_info;
	const struct ioexpander_drv *drv;

};

extern  struct ioexpander_config_t ioex_config[];

int ioex_get_flags_by_mask(int ioex, int port, int mask, int *flags);
int ioex_set_flags_by_mask(int ioex, int port, int mask, int flags);
int ioex_get_flags(enum ioex_signal signal, int *flags);
int ioex_set_flags(enum ioex_signal signal, int flags);
int ioex_get_level(enum ioex_signal signal, int *val);
int ioex_set_level(enum ioex_signal signal, int value);
int ioex_init(int ioex);
const char *ioex_get_name(enum ioex_signal signal);
#endif /* __CROS_EC_IOEXPANDER_H */

