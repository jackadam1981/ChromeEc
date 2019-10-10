/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_GPIO_SIGNAL_H
#define __CROS_EC_GPIO_SIGNAL_H

/*
 * There are 3 different IO signal types used by the EC.
 * Ensure they each use a unique range of values so we can tell them apart.
 * 1) Local GPIO => 0 to 0x0FFF
 * 2) IO expander GPIO => 0x1000 to 0x1FFF
 * 3) eSPI virtual wire signals (defined in include/espi.h) => 0x2000 to 0x2FFF
 */

#define GPIO(name, pin, flags) GPIO_##name,
#define UNIMPLEMENTED(name) GPIO_##name,
#define GPIO_INT(name, pin, flags, signal) GPIO_##name,

#define GPIO_SIGNAL_START 0 /* The first valid GPIO signal is 0 */

enum gpio_signal {
	#include "gpio.wrap"
	GPIO_COUNT,
	/* Ensure that sizeof gpio_signal is large enough for ioex_signal */
	GPIO_LIMIT = 0x1000
};

#define IOEX(name, expin, flags) IOEX_##name,
#define IOEX_INT(name, expin, flags, signal) IOEX_##name,

enum ioex_signal {
	IOEX_SIGNAL_START = 0x1000, /* The first valid IOEX signal is 0x1001 */
	#include "gpio.wrap"
	IOEX_SIGNAL_END
};

/* The -1 is because VW_SIGNAL_START is not a valid VW signal. */
#define IOEX_COUNT (IOEX_SIGNAL_END - IOEX_SIGNAL_START - 1)

#endif /* __CROS_EC_GPIO_SIGNAL_H */
