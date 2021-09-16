/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for TCPC emulator
 */

#ifndef __EMUL_TCPC_H
#define __EMUL_TCPC_H

#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

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

#define TCPC_EMUL_REG_COUNT		0x100

struct tcpc_emul_msg {
	uint8_t *buf;
	int cnt;
	uint8_t type;
	int idx;
	struct tcpc_emul_msg *next;
};

/**
 * @brief Get pointer to BB retimer emulator using device tree order number.
 *
 * @param ord Device tree order number obtained from DT_DEP_ORD macro
 *
 * @return Pointer to BB retimer emulator
 */
struct i2c_emul *tcpc_emul_get(int ord);

/**
 * @brief Set value of given register of BB retimer
 *
 * @param emul Pointer to BB retimer emulator
 * @param reg Register address which value will be changed
 * @param val New value of the register
 */
void tcpc_emul_set_reg(struct i2c_emul *emul, int reg, uint8_t val);

void tcpc_emul_set_reg16(struct i2c_emul *emul, int reg, uint16_t val);

/**
 * @brief Get value of given register of BB retimer
 *
 * @param emul Pointer to BB retimer emulator
 * @param reg Register address
 *
 * @return Value of the register
 */
uint8_t tcpc_emul_get_reg(struct i2c_emul *emul, int reg);

uint16_t tcpc_emul_get_reg16(struct i2c_emul *emul, int reg);

/**
 * @brief Set if error should be generated when read only register is being
 *        written
 *
 * @param emul Pointer to BB retimer emulator
 * @param set Check for this error
 */
void tcpc_emul_set_err_on_ro_write(struct i2c_emul *emul, bool set);

/**
 * @brief Set if error should be generated when reserved bits of register are
 *        not set to 0 on write I2C message
 *
 * @param emul Pointer to BB retimer emulator
 * @param set Check for this error
 */
void tcpc_emul_set_err_on_rsvd_write(struct i2c_emul *emul, bool set);

/**
 * @}
 */

#endif /* __EMUL_TCPC */

