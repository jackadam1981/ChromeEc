/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_GYROSCOPE_H
#define __CROS_EC_GYROSCOPE_H

/**< Header file for gyroscope drivers. */

/**
 * Read all three axis angular rate of an gyroscope. Note that all three
 * axis angular rate come back in counts in 16-bit word in two's complement.
 *
 * @param id Target gyroscope
 * @param x_gyro Pointer to store X-axis angular rate (in counts).
 * @param y_gyro Pointer to store Y-axis angular rate (in counts).
 * @param z_gyro Pointer to store Z-axis angular rate (in counts).
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int gyro_read(const enum accel_id id, int * const x_gyro, int * const y_gyro,
		int * const z_gyro);

/**
 * Initialize gyroscopes.
 *
 * @param id Target gyroscope
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int gyro_init(const enum accel_id id);

/**
 * Setter and getter methods for the sensor range. The sensor range defines
 * the maximum value that can be returned from gyro_read(). As the range
 * increases, the resolution gets worse.
 *
 * @param id Target gyroscope
 * @param range Range (Units are +/- deg/s for gyro)
 * @param rnd Rounding flag. If true, it rounds up to nearest valid value.
 *                Otherwise, it rounds down.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int gyro_set_range(const enum accel_id id, const int range, const int rnd);
int gyro_get_range(const enum accel_id id, int * const range);


/**
 * Setter and getter methods for the sensor output data range. As the ODR
 * increases, the LPF roll-off frequency also increases.
 *
 * @param id Target gyroscope
 * @param rate Output data rate (units are mHz)
 * @param rnd Rounding flag. If true, it rounds up to nearest valid value.
 *                Otherwise, it rounds down.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int gyro_set_datarate(const enum accel_id id, const int rate, const int rnd);
int gyro_get_datarate(const enum accel_id id, int * const rate);

#endif /* __CROS_EC_GYROSCOPE_H */
