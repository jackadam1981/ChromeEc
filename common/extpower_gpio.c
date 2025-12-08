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

#define EC_HOST_EVENT_AC_CONNECTED_MASK \
	EC_HOST_EVENT_MASK(EC_HOST_EVENT_AC_CONNECTED)
#define EC_HOST_EVENT_AC_DISCONNECTED_MASK \
	EC_HOST_EVENT_MASK(EC_HOST_EVENT_AC_DISCONNECTED)

static int debounced_extpower_presence;

void extpower_update_host_events(int is_present)
{
	uint8_t *memmap_batt_flags;
	memmap_batt_flags = host_get_memmap(EC_MEMMAP_BATT_FLAG);
	host_event_t mask;

	/* Forward notification to host */
	if (is_present) {
		*memmap_batt_flags |= EC_BATT_FLAG_AC_PRESENT;
		host_set_single_event(EC_HOST_EVENT_AC_CONNECTED);
		mask = EC_HOST_EVENT_AC_DISCONNECTED_MASK;
	} else {
		*memmap_batt_flags &= ~EC_BATT_FLAG_AC_PRESENT;
		host_set_single_event(EC_HOST_EVENT_AC_DISCONNECTED);
		mask = EC_HOST_EVENT_AC_CONNECTED_MASK;
	}

	/* Clear the mask depending upon the charging status. */
	host_clear_events_b(mask);
	host_clear_events(mask);
}

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
		/* Initialize the memory-mapped AC_PRESENT flag */
		extpower_update_host_events(debounced_extpower_presence);
	}
	/* Enable interrupts, now that we've initialized */
	gpio_enable_interrupt(GPIO_AC_PRESENT);
}
DECLARE_HOOK(HOOK_INIT, extpower_init, HOOK_PRIO_INIT_EXTPOWER);
