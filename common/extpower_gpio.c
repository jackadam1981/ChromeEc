/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Pure GPIO-based external power detection */

#include "common.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "timer.h"
#include "console.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHARGER, outstr)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)

static int debounced_extpower_presence;

test_mockable int extpower_is_present(void)
{
	return debounced_extpower_presence;
}

/**
 * Deferred function to handle external power change
 */
static void extpower_deferred(void)
{
	int extpower_presence = gpio_get_level(GPIO_AC_PRESENT);

	CPRINTS("%s: extpower_presence=%d", __func__, extpower_presence);

	if (extpower_presence == debounced_extpower_presence)
		return;

	debounced_extpower_presence = extpower_presence;
	extpower_handle_update(extpower_presence);
}
DECLARE_DEFERRED(extpower_deferred);

void extpower_interrupt(enum gpio_signal signal)
{
	/* Trigger deferred notification of external power change */
	hook_call_deferred(&extpower_deferred_data,
			   CONFIG_EXTPOWER_DEBOUNCE_MS * MSEC);
}

static void extpower_init(void)
{
	debounced_extpower_presence = gpio_get_level(GPIO_AC_PRESENT);

	if (IS_ENABLED(HAS_TASK_HOSTCMD)) {
		uint8_t *memmap_batt_flags =
			host_get_memmap(EC_MEMMAP_BATT_FLAG);

		/* Initialize the memory-mapped AC_PRESENT flag */
		if (debounced_extpower_presence)
			*memmap_batt_flags |= EC_BATT_FLAG_AC_PRESENT;
		else
			*memmap_batt_flags &= ~EC_BATT_FLAG_AC_PRESENT;
	}

	/* Enable interrupts, now that we've initialized */
	gpio_enable_interrupt(GPIO_AC_PRESENT);
}
DECLARE_HOOK(HOOK_INIT, extpower_init, HOOK_PRIO_INIT_EXTPOWER);
