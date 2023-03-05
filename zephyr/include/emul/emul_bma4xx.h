/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_BMA4XX_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_BMA4XX_H_

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c_emul.h>

/**
 * @brief Reset the state of the bma4xx emulator.
 *
 * @param emul The emulator to reset.
 */
void bma4xx_emul_reset(const struct emul *emul);

struct motion_sensor_t *bma4xx_emul_get_sensor_data(const struct emul *emul);

/**
 * @brief Returns pointer to i2c_common_emul_data for argument emul
 *
 * @param emul Pointer to LIS2DW12 emulator
 * @return Pointer to i2c_common_emul_data from argument emul
 */
struct i2c_common_emul_data *
emul_bma4xx_get_i2c_common_data(const struct emul *emul);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_BMA4XX_H_ */
