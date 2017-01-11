/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_INTEL_X86_H
#define __CROS_EC_INTEL_X86_H

#include "power.h"

enum intel_x86_chips {
	INTEL_X86_CHIP_APOLLOLAKE,
	INTEL_X86_CHIP_SKYLAKE,
};

/**
 * Handle RSMRST signal.
 *
 * @param state    Current chipset state.
 * @param soc_chip Intel X86 SoC chip.
 */
void common_intel_x86_handle_rsmrst(enum power_state state,
				    enum intel_x86_chips soc_chip);

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
 * @param soc_chip     Intel X86 SoC chip.
 * @return power_state New chipset state.
 */
enum power_state common_intel_x86_power_handle_state(enum power_state state,
						enum intel_x86_chips soc_chip);

#endif /* __CROS_EC_INTEL_X86_H */
