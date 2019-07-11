/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_IO_EXPANDER_H
#define __CROS_EC_IO_EXPANDER_H
#include "console.h"

/*
 * The IO pin of IO Expander is defined by the format in gpio.inc:
 * GPIO(name, EXPIN(ioex_port, port, offset), flags).
 * The ioex_port and port will be combined and stored in the member "port"
 * (in bit filed [31:28] and [27:0] separately) of struct gpio_info.
 * Bit filed [31:28] means:
 *   0000 = regular GPIO
 *   0001 = 1st port of IO Expander chip
 *   ...
 *   1111 = 15th port of IO Expander chip
 * These macros help to convert between them.
 */
#define GPIO_EXPIN(ioex_port, port, offset) \
			(((ioex_port) + 1) << 28 | (port)), (1 << (offset))
#define IOEX_CHIP_PORT_NUM(p)       (((p) >> 28) - 1)
#define IOEX_IO_PORT_NUM(p)         ((p) & 0x0FFFFFFF)
#define IS_IOEX_PIN(p)              (!!((p) & 0xF0000000))

struct ioexpander_drv {
	int (*init)(int expander);
	/* Get the current level of a IO Expander IO pin. */
	int (*get_level)(int expander, int port, int mask, int *val);
	/* Set the level of a IO Expander IO pin. */
	int (*set_level)(int expander, int port, int mask, int val);
	/* Get flags for a IO Expander IO pin */
	int (*get_flags_by_mask)(int expander, int port, int mask, int *flags);
	/* Set flags for a IO Expander IO pin */
	int (*set_flags_by_mask)(int expander, int port, int mask, int flags);
};

struct ioexpander_config_t {
	int i2c_host_port;
	int i2c_slave_addr;
	int chip_info;
	const struct ioexpander_drv *drv;

};

extern  struct ioexpander_config_t ioex_config[];
#endif /* __CROS_EC_IOEXPANDER_H */

