/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for KB8010 retimer emulator
 */

#ifndef __EMUL_KB8010_H
#define __EMUL_KB8010_H

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

/**
 * @brief KB8010 retimer emulator backend API
 * @defgroup kb8010_emul KB8010 retimer emulator
 * @{
 *
 * KB8010 retimer emulator supports access to all its registers using I2C
 * messages. Application may alter emulator state:
 *
 * - call @ref kb8010_emul_set_reg and @ref kb8010_emul_get_reg to set and get
 * value of KB8010 retimers registers
 * - call functions from emul_common_i2c.h to setup custom handlers for I2C
 *   messages
 */

/**
 * @brief Set value of given register of KB8010 retimer
 *
 * @param emul Pointer to KB8010 retimer emulator
 * @param reg Register address which value will be changed
 * @param val New value of the register
 */
void kb8010_emul_set_reg(const struct emul *emul, int reg, uint8_t val);

/**
 * @brief Get value of given register of KB8010 retimer
 *
 * @param emul Pointer to KB8010 retimer emulator
 * @param reg Register address
 *
 * @return Value of the register
 */
uint8_t kb8010_emul_get_reg(const struct emul *emul, int reg);

/**
 * @brief Reset the kb8010 emulator
 *
 * @param emul The emulator to reset
 */
void kb8010_emul_reset(const struct emul *emul);

/**
 * @brief Returns pointer to i2c_common_emul_data for given emul
 *
 * @param emul Pointer to kb8010 retimer emulator
 * @return Pointer to i2c_common_emul_data for emul argument
 */
struct i2c_common_emul_data *
emul_kb8010_get_i2c_common_data(const struct emul *emul);

/**
 * @}
 */

#endif /* __EMUL_KB8010 */
