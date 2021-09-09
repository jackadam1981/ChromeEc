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

#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "usb_mux.h"

/**
 * @brief BB retimer emulator backend API
 * @defgroup bb_emul BB retimer emulator
 * @{
 *
 * BB retimer emulator supports access to all its registers using I2C messages.
 * It supports not four bytes writes by padding zeros (the same as real
 * device), but show warning in that case.
 * Application may alter emulator state:
 *
 * - define a Device Tree overlay file to set default vendor ID and which
 *   inadvisable driver behaviour should be treated as errors
 * - call @ref bb_emul_set_reg and @ref bb_emul_get_reg to set and get value
 *   of BB retimers registers
 * - call bb_emul_set_err_* to change emulator behaviour on inadvisable driver
 *   behaviour
 * - call functions from emul_common_i2c.h to setup custom handlers for I2C
 *   messages
 */

/**
 * @brief Get pointer to BB retimer emulator using device tree order number.
 *
 * @param ord Device tree order number obtained from DT_DEP_ORD macro
 *
 * @return Pointer to BB retimer emulator
 */
//struct i2c_emul *bb_emul_get(int ord);

struct emul_usb_mux_mock_data {
	/** Alternative set of usb_mux_driver callbacks */
	struct usb_mux_driver *alt_drv;

	/** Number of calls specific funtion */
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

extern struct emul_usb_mux_mock_data usb_mux_mock_data[];
extern const struct usb_mux_driver emul_usb_mux_mock;

void emul_usb_mux_mock_reset(void);

/**
 * @}
 */

#endif /* __EMUL_USB_MUX_MOCK_H */

