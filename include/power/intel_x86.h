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


/* GPIO for power signal */
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_S3
#define SLP_S3_SIGNAL_L VW_SLP_S3_L
#else
#define SLP_S3_SIGNAL_L GPIO_PCH_SLP_S3_L
#endif
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_S4
#define SLP_S4_SIGNAL_L VW_SLP_S4_L
#else
#define SLP_S4_SIGNAL_L GPIO_PCH_SLP_S4_L
#endif
/*
 * The SLP_S5 signal has not traditionally been connected to the EC. If virtual
 * wire support is enabled, then SLP_S5 will be available that way. Otherwise,
 * use SLP_S4's GPIO as a proxy for SLP_S5. This matches old behavior and
 * effectively prevents S4 residency.
 */
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_S5
#define SLP_S5_SIGNAL_L VW_SLP_S5_L
#else
#define SLP_S5_SIGNAL_L SLP_S4_SIGNAL_L
#endif

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
 * Wait for power-up to be allowed based on available power.
 *
 * This delays G3->S5 until there is enough power to boot the AP, waiting
 * first until the charger (if any) is ready, then for there to be sufficient
 * power.
 *
 * In case of error, the caller should not allow power-up past G3.
 *
 * @return EC_SUCCESS if OK.
 */
enum ec_error_list intel_x86_wait_power_up_ok(void);

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
