/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CEC_BITBANG_H
#define __CROS_EC_CEC_BITBANG_H

#include "driver/cec/bitbang.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 * @param edge to trigger capture timer interrupt on
 * @param timeout timeout for capture interrupt edge
 */
void cros_cec_bitbang_tmr_cap_start(int port, enum cec_cap_edge edge,
				    int timeout) __attribute__((weak));

/**
 * @brief Stop the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_tmr_cap_stop(int port) __attribute__((weak));

/**
 * @brief get the time from the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 *
 * @return time read from the timer
 */
int cros_cec_bitbang_tmr_cap_get(int port) __attribute__((weak));

/**
 * @brief enter debounce state.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_debounce_enable(int port) __attribute__((weak));

/**
 * @brief leave debounce state.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_debounce_disable(int port) __attribute__((weak));

/**
 * @brief Elevate to interrupt context.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_trigger_send(int port) __attribute__((weak));

/**
 * @brief enable the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_enable_timer(int port) __attribute__((weak));

/**
 * @brief disable the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_disable_timer(int port) __attribute__((weak));

/**
 * @brief initialize the timer for cec bitbang.
 *
 * @param dev Pointer to the device structure for the cec bitbang driver
 * instance.
 * @param port CEC port to control
 */
void cros_cec_bitbang_init_timer(int port) __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_CEC_BITBANG_H */
