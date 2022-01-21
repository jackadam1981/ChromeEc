/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_INTEL_X86_H
#define __CROS_EC_INTEL_X86_H

#include "common_x86.h"
#include "espi.h"
#include "power.h"

/**
 * Handle RSMRST signal.
 *
 * @param state Current chipset state.
 */
void common_intel_x86_handle_rsmrst(enum power_state state);

/**
 * Handle power states.
 *
 * @param state        Current chipset state.
 * @return power_state New chipset state.
 */
enum power_state common_intel_x86_power_handle_state(enum power_state state);

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

#endif /* __CROS_EC_INTEL_X86_H */
