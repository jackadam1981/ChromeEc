/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_INTEL_X86_H
#define __CROS_EC_INTEL_X86_H

#include "power.h"

#ifdef CONFIG_INTEL_POWER_SIGNALS_COMMON
/* power signal list */
enum power_signal {
	X86_SLP_S0_DEASSERTED,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
	X86_RSMRST_L_PGOOD,
	X86_ALL_SYS_PWRGD,
#if defined(CONFIG_CHIPSET_ICELAKE)
	X86_SLP_SUS_DEASSERTED,
	X86_DSW_DPWROK,
#elif defined(CONFIG_CHIPSET_COMETLAKE)
	PP5000_A_PGOOD,
#else
	#error "define Intel AP chipset variant"
#endif
	/* Number of X86 signals */
	POWER_SIGNAL_COUNT
};
#endif /* CONFIG_INTEL_POWER_SIGNALS_COMMON */

/**
 * Handle RSMRST signal.
 *
 * @param state Current chipset state.
 */
void common_intel_x86_handle_rsmrst(enum power_state state);

/**
 * Force chipset to G3 state.
 *
 * @return power_state New chipset state.
 */
enum power_state chipset_force_g3(void);

/**
 * Handle power states.
 *
 * @param state        Current chipset state.
 * @return power_state New chipset state.
 */
enum power_state common_intel_x86_power_handle_state(enum power_state state);

/**
 * Reset RTC
 */
void board_rtc_reset(void);

#endif /* __CROS_EC_INTEL_X86_H */
