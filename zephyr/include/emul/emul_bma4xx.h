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

/** Get the I2C interface emulator used by this emulator. */
struct i2c_common_emul_data *bma4xx_emul_get_i2c(const struct emul *emul);

struct motion_sensor_t *bma4xx_emul_get_sensor_data(const struct emul *emul);
int bma4xx_emul_get_sensor_num(const struct emul *emul);

/** Return whether the accelerometer is enabled (currently sensing). */
bool bma4xx_emul_is_accel_enabled(const struct emul *emul);

/** Set whether the accelerometer is enabled (currently sensing). */
void bma4xx_emul_set_accel_enabled(const struct emul *emul, bool enabled);

/** Get the sensor's current sensing range, as a positive integer in gs. */
uint8_t bma4xx_emul_get_accel_range(const struct emul *emul);

/** Get the sensor's current output data rate, in milliHz. */
uint32_t bma4xx_emul_get_odr(const struct emul *emul);

/** Set the sensor's current acceleration reading, in milli-g on each axis. */
void bma4xx_emul_set_accel_data(const struct emul *emul, int x, int y, int z);

/** Get the current offset register values, XYZ. */
void bma4xx_emul_get_offset(const struct emul *emul, int8_t (*offset)[3]);

/** Return the current value of the NV_CONF register. */
uint8_t bma4xx_emul_get_nv_conf(const struct emul *emul);

/**
 * Return true if the FIFO is enabled.
 *
 * Only headerless mode with accel data only is supported, so when this is true
 * it also implies that bits of FIFO_CONFIG_1 other than fifo_acc_en are clear.
 */
bool bma4xx_emul_is_fifo_enabled(const struct emul *emul);

/** Queue data with provided size to be read from the FIFO. */
void bma4xx_emul_set_fifo_data(const struct emul *emul,
			       const uint8_t *fifo_data, uint16_t data_sz);
/**
 * Return the current interrupt configuration.
 *
 * Provided pointers are out-parameters for the INT1_IO_CTRL register and
 * whether interrupts are in latched mode.
 *
 * @return value of the INT_MAP_DATA register
 */
uint8_t bma4xx_emul_get_interrupt_config(const struct emul *emul,
					 uint8_t *int1_io_ctrl,
					 bool *latched_mode);

/**
 * @brief Returns pointer to i2c_common_emul_data for argument emul
 *
 * @param emul Pointer to BMA4xx emulator
 * @return Pointer to i2c_common_emul_data from argument emul
 */
struct i2c_common_emul_data *
emul_bma4xx_get_i2c_common_data(const struct emul *emul);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_BMA4XX_H_ */
