/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_INTEL_X86_H
#define __CROS_EC_INTEL_X86_H

#include "common_x86.h"
#include "power.h"

#define G3S5_SHUTDOWN_REASON		CHIPSET_SHUTDOWN_WAIT
#define BATTERY_INHIBIT

/**
 * Handle RSMRST signal.
 *
 * @param state Current chipset state.
 */
void common_intel_x86_handle_rsmrst(enum power_state state);

/**
 * Get the value of PG_EC_DSW_PWROK.
 *
 * The default implementation is just to return the GPIO.  But if a
 * board doesn't have that GPIO, they may override this function.
 */
__override_proto int intel_x86_get_pg_ec_dsw_pwrok(void);

/**
 * Get the value of PG_EC_ALL_SYS_PWRGD.
 *
 * The default implementation is just to return the GPIO.  But if a
 * board doesn't have that GPIO, they may override this function.
 */
__override_proto int intel_x86_get_pg_ec_all_sys_pwrgd(void);

static inline void handle_power_failure(void)
{
	chipset_force_shutdown(CHIPSET_SHUTDOWN_POWERFAIL);
}

static inline void init_prochot(void)
{
#ifdef CONFIG_CPU_PROCHOT_ACTIVE_LOW
		gpio_set_level(GPIO_CPU_PROCHOT, 1);
#else
		gpio_set_level(GPIO_CPU_PROCHOT, 0);
#endif /* CONFIG_CPU_PROCHOT_ACTIVE_LOW */
}

#endif /* __CROS_EC_INTEL_X86_H */
