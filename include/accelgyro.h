/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ACCELGYRO_H
#define __CROS_EC_ACCELGYRO_H

#include "motion_sense.h"

/* Header file for accelerometer / gyro drivers. */

/* Number of counts from accelerometer that represents 1G acceleration. */
#define ACCEL_G  1024

struct accelgyro_drv {
	/**
	 * Initialize accelerometers.
	 * @param s Pointer to sensor data pointer. Sensor data will be
	 * allocated on success.
	 * @param i2c_addr i2c slave device address
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*init)(const struct motion_sensor_t *s);

	/**
	 * Read all three accelerations of an accelerometer. Note that all
	 * three accelerations come back in counts, where ACCEL_G can be used
	 * to convert counts to engineering units.
	 * @param s Pointer to sensor data.
	 * @param x_acc Pointer to store X-axis acceleration (in counts).
	 * @param y_acc Pointer to store Y-axis acceleration (in counts).
	 * @param z_acc Pointer to store Z-axis acceleration (in counts).
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*read)(const struct motion_sensor_t *s,
		    int *const x_acc,
		    int *const y_acc,
		    int *const z_acc);

	/**
	 * Setter and getter methods for the sensor range. The sensor range
	 * defines the maximum value that can be returned from read(). As the
	 * range increases, the resolution gets worse.
	 * @param s Pointer to sensor data.
	 * @param range Range (Units are +/- G's for accel, +/- deg/s for gyro)
	 * @param rnd Rounding flag. If true, it rounds up to nearest valid
	 * value. Otherwise, it rounds down.
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*set_range)(const struct motion_sensor_t *s,
			 const int range,
			 const int rnd);
	int (*get_range)(const struct motion_sensor_t *s,
			 int * const range);

	/**
	 * Setter and getter methods for the sensor resolution.
	 * @param s Pointer to sensor data.
	 * @param range Resolution (Units are number of bits)
	 * param rnd Rounding flag. If true, it rounds up to nearest valid
	 * value. Otherwise, it rounds down.
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*set_resolution)(const struct motion_sensor_t *s,
			      const int res,
			      const int rnd);
	int (*get_resolution)(const struct motion_sensor_t *s,
			      int * const res);

	/**
	 * Setter and getter methods for the sensor output data range. As the
	 * ODR increases, the LPF roll-off frequency also increases.
	 * @param s Pointer to sensor data.
	 * @param rate Output data rate (units are mHz)
	 * @param rnd Rounding flag. If true, it rounds up to nearest valid
	 * value. Otherwise, it rounds down.
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*set_data_rate)(const struct motion_sensor_t *s,
			    const int rate,
			    const int rnd);
	int (*get_data_rate)(const struct motion_sensor_t *s,
			    int * const rate);

#ifdef CONFIG_ACCEL_INTERRUPTS
	/**
	 * Setup a one-time accel interrupt. If the threshold is low enough, the
	 * interrupt may trigger due simply to noise and not any real motion.
	 * If the threshold is 0, the interrupt will fire immediately.
	 * @param s Pointer to sensor data.
	 * @param threshold Threshold for interrupt in units of counts.
	 */
	int (*set_interrupt)(const struct motion_sensor_t *s,
			     unsigned int threshold);
#endif
};

#endif /* __CROS_EC_ACCELGYRO_H */
