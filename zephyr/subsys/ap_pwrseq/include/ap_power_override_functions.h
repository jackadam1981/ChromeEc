/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Declare functions that may be supplied externally.
 *
 * Some functions may have board specific implementations
 * Config items starting with CONFIG_AP_PWRSEQ_EXT_xxx
 * allow selected functions to be supplied by an external
 * board specific implementation, in which case the
 * default function is conditionally compiled out.
 *
 * These overrideable functions should have sensible default
 * implementations so that the external implementations
 * are optional. Functions that must be supplied as
 * external implementations should not be appearing here, but
 * should be considered as part of the supporting API of
 * the AP power module.
 *
 * The functions are all prepended with board_ap_power_ to indicate
 * they have optional external implementations.
 *
 * The external functions may need to access
 * devicetree poroperties for values such
 * as timeouts etc.
 */

#ifndef __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__
#define __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__

#include <devicetree.h>

/**
 * @brief Force AP shutdown
 *
 * Immediately shut down the AP.
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_FORCE_SHUTDOWN
 */
void board_ap_power_force_shutdown(void);

/**
 * @brief Check and wait for all system power good.
 *
 * Check and monitor all the power rails and return
 * when they are ready, or when a timeout occurs.
 *
 * The DTS property all-sys-pwrgd-timeout can be used
 * as the timeout.
 * Other timeouts such as vrrdy-timeout may also be used.
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_ALL_SYS_POWER_GOOD
 *
 * @return 0 All system power is good
 * @return -1 Timeout or error
 */
int board_ap_power_all_sys_power_good(void);

/**
 * @brief Called to transition from G3 to S5
 *
 * Action to start transition from G3 to S5.
 * Usually involves enabling the main power rails.
 *
 * Config properties may include pch-pwrok-delay or wait-signal-timeout.
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_ACTION_G3_S5
 */
void board_ap_power_action_g3_s5(void);

/**
 * @brief Called to transition from S3 to S0
 *
 * Action to transition from S3 to S0.
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_ACTION_S3_S0
 */
void board_ap_power_action_s3_s0(void);

/**
 * @brief Called to transition from S0 to S3
 *
 * Action to transition from S0 to S3.
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_ACTION_S0_S3
 */
void board_ap_power_action_s0_s3(void);

/**
 * @brief Assert PCH power OK signal to AP
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_ASSERT_PCH_POWER_OK
 *
 * @return 0 Success
 * @return -1 Timeout or error
 */
int board_ap_power_assert_pch_power_ok(void);

/**
 * @brief Pass through RSMRST signal to AP
 *
 * Set the RSMRST output signal to the AP according
 * to the RSMRST input signal.
 * The rsmrst-delay config property should be used as the
 * delay (if needed) for the signal.
 *
 * Selected by CONFIG_AP_PWRSEQ_BOARD_RSMRST_PASS_THROUGH
 */
void board_ap_power_rsmrst_pass_through(void);

/**
 * @brief macro to access configuration properties from DTS
 */
#define AP_PWRSEQ_DT_VALUE(p)					\
	DT_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq), p)	\

#endif /* __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__ */
