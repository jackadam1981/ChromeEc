/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for usb mux mock
 */

#ifndef __EMUL_USB_MUX_MOCK_H
#define __EMUL_USB_MUX_MOCK_H

#include "usb_mux.h"

/**
 * @brief usb mux mock backend API
 * @defgroup emul_usb_mux_mock usb mux mock
 * @{
 *
 * Usb mux mock implements all usb_mux_driver functions and allows to test
 * usb_mux code. @ref usb_mux_mock_data can be used to alter mock state and
 * check number of calls to driver functions.
 *
 * To select mock instance in usb_muxes entry, i2c_addr_flags should be set
 * to instance number.
 */


/**
 * @brief Contains how many times each usb mux driver function was called since
 *        last reset, values that should be return from each usb mux driver
 *        function and last state set by set callback. It allows to set
 *        alternative set of usb mux driver functions to be called instead of
 *        usb mux mock default callbacks.
 */
struct emul_usb_mux_mock_data {
	/** Alternative set of usb_mux_driver callbacks */
	struct usb_mux_driver *alt_drv;

	/** Number of calls specific function */
	int num_set_calls;
	int num_get_calls;
	int num_init_calls;
	int num_enter_lpm_calls;
	int num_chipset_reset_calls;

	/** Return codes of functions */
	int set_ret;
	int get_ret;
	int init_ret;
	int enter_lpm_ret;
	int chipset_reset_ret;

	/** State of mux set using set callback */
	mux_state_t state;
};

/**
 * @brief Reset usb_mux_mock_data for all usb muxes to default state
 */
extern struct emul_usb_mux_mock_data usb_mux_mock_data[];

/**
 * @brief Driver structure of usb mux mock. It should be used in usb_muxes.
 */
extern const struct usb_mux_driver emul_usb_mux_mock;

/**
 * @brief Reset usb_mux_mock_data for all usb muxes to default state
 */
void emul_usb_mux_mock_reset(void);

/**
 * @}
 */

#endif /* __EMUL_USB_MUX_MOCK_H */
