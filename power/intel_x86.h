/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_INTEL_X86_H
#define __CROS_EC_INTEL_X86_H

#include "power.h"

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
 * Reset the CPU and/or chipset.
 *
 * @param cold_reset	If !=0, force a cold reset of the CPU and chipset;
 *			if 0, just pulse the reset line to the CPU.
 */
void ap_chipset_reset(int cold_reset);

/**
 * Set force cold reset is executed.
 */
void ap_set_force_cold_reset(void);

#endif /* __CROS_EC_INTEL_X86_H */
