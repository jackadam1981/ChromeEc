/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <logging/log.h>
#include "common.h"
#include "hooks.h"
#include "i2c.h"
#include "ioexpander.h"
#include "system.h"

LOG_MODULE_REGISTER(ioex_drv_fake_cros, CONFIG_GPIO_LOG_LEVEL);

#define IOEX_FAKE_PORTS 2

static int fake_value[IOEX_FAKE_PORTS];
static int fake_int_en[IOEX_FAKE_PORTS];
static int fake_flags[IOEX_FAKE_PORTS][8];

/* Initialize IO expander chip/driver */
static int fake_init(int ioex)
{
	int a, b;

	for (a = 0; a < IOEX_FAKE_PORTS; a++) {
		fake_value[a] = 0;
		fake_int_en[a] = 0;

		for (b = 0; b < 8; b++)
			fake_flags[a][b] = 0;
	}

	return EC_SUCCESS;
}

/* Get the current level of the IOEX pin */
static int fake_get_level(int ioex, int port, int mask, int *val)
{
	*val = !!(fake_value[port] & mask);

	return EC_SUCCESS;
}

/* Set the level of the IOEX pin */
static int fake_set_level(int ioex, int port, int mask, int val)
{
	if (val)
		fake_value[port] |= mask;
	else
		fake_value[port] &= ~mask;

	return EC_SUCCESS;
}

/* Get flags for the IOEX pin */
static int fake_get_flags_by_mask(int ioex, int port, int mask, int *flags)
{
	int a;

	*flags = 0;
	for (a = 0; a < 8; a++) {
		if ((1 << a) == mask) {
			*flags = fake_flags[port][a];
			break;
		}
	}

	return EC_SUCCESS;
}

/* Set flags for the IOEX pin */
static int fake_set_flags_by_mask(int ioex, int port, int mask, int flags)
{
	int a;

	for (a = 0; a < 8; a++) {
		if ((1 << a) == mask) {

			if (flags & GPIO_HIGH)
				fake_value[port] |= mask;
			if (flags & GPIO_LOW)
				fake_value[port] &= ~mask;

			fake_flags[port][a] = flags;
			break;
		}
	}

	return EC_SUCCESS;
}

static int fake_enable_interrupt(int ioex, int port, int mask, int enable)
{
	if (enable)
		fake_int_en[port] |= mask;
	else
		fake_int_en[port] &= ~mask;

	return EC_SUCCESS;
}

#ifdef CONFIG_IO_EXPANDER_SUPPORT_GET_PORT

/* Read levels for whole IO expander port */
static int fake_get_port(int ioex, int port, int *val)
{
	*val = fake_value[port];

	return EC_SUCCESS;
}

#endif

static void fake_change_pin(int port, int pin)
{
	fake_value[port] ^= (1 << pin);

	if (fake_int_en[port] & (1 << pin))
		LOG_INF("Fake pin changed: (%d:%d)=%d\n",
			port,
			pin,
			!!(fake_value[port] & (1 << pin)));
}

static void fake_tick_event(void)
{
	static int i;
	int port, pin;

	i++;

	if (i % 4 != 0)
		return;

	for (port = 0; port < IOEX_FAKE_PORTS; port++) {
		for (pin = 0; pin < 8; pin++) {
			if (fake_flags[port][pin] & GPIO_INPUT)
				fake_change_pin(port, pin);
		}
	}
}
DECLARE_HOOK(HOOK_TICK, fake_tick_event, HOOK_PRIO_DEFAULT);

/* Driver structure */
const struct ioexpander_drv fake_ioexpander_drv = {
	.init			= fake_init,
	.get_level		= fake_get_level,
	.set_level		= fake_set_level,
	.get_flags_by_mask	= fake_get_flags_by_mask,
	.set_flags_by_mask	= fake_set_flags_by_mask,
	.enable_interrupt	= fake_enable_interrupt,
#ifdef CONFIG_IO_EXPANDER_SUPPORT_GET_PORT
	.get_port		= fake_get_port,
#endif
};
