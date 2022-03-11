/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Declare functions that are supplied externally.
 * The functions are all prepended with board_ap_power_ to indicate
 * they have external implementations.
 *
 * Longer term, a framework should be put in place to allow
 * these functions to be optional.
 *
 * The external functions may need to access
 * devicetree properties for values such
 * as timeouts etc.
 */

#ifndef __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__
#define __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__

#include <devicetree.h>

/**
 * @brief Force AP shutdown
 *
 * Immediately shut down the AP.
 */
void board_ap_power_force_shutdown(void);

/**
 * @brief Called to transition from G3 to S5
 *
 * Action to start transition from G3 to S5.
 * Usually involves enabling the main power rails.
 */
void board_ap_power_action_g3_s5(void);

/**
 * @brief Called to transition from S3 to S0
 *
 * Action to transition from S3 to S0.
 */
void board_ap_power_action_s3_s0(void);

/**
 * @brief Called to transition from S0 to S3
 *
 * Action to transition from S0 to S3.
 */
void board_ap_power_action_s0_s3(void);

/**
 * @brief Assert PCH power OK signal to AP
 *
 * @return 0 Success
 * @return -1 Timeout or error
 */
int board_ap_power_assert_pch_power_ok(int delay);

/**
 * @brief Pass through RSMRST signal to AP
 *
 * Set the RSMRST output signal to the AP according
 * to the RSMRST input signal.
 * The rsmrst-delay config property should be used as the
 * delay (if needed) for the signal.
 */
void board_ap_power_rsmrst_pass_through(void);

/**
 * @brief macro to access configuration properties from DTS
 */
#define AP_PWRSEQ_DT_VALUE(p)					\
	DT_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq), p)	\

#endif /* __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__ */
