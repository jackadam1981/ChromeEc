/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LIS2MDL mag module for Chrome EC. */

#ifndef __CROS_EC_MAG_LIS2MDL_H
#define __CROS_EC_MAG_LIS2MDL_H

#include "accelgyro.h"
#include "stm_mems_common.h"

#define LIS2MDL_EN_BIT			1
#define LIS2MDL_DIS_BIT			0
#define LIS2MDL_I2C_ADDR(__x)		(__x << 1)

/*
 * 7-bit address is 0011110Xb. Where 'X' is determined
 * by the voltage on the ADDR pin
 */
#define LIS2MDL_ADDR0			LIS2MDL_I2C_ADDR(0x1e)
#define LIS2MDL_ADDR1			LIS2MDL_I2C_ADDR(0x1f)

/* Registers */
#define LIS2MDL_WHO_AM_I_REG		0x4f
#define LIS2MDL_WHOAMI_VAL		0x40

#define LIS2MDL_CFG_REG_A		0x60

/**
 * 0 0 Continuous mode. In continuous mode the device continuously
 * performs measurements and places the result in the data register.
 * The data-ready signal is generated when a new data set is ready to
 * be read. This signal can be available on the external pin by setting
 * the INT_MAG bit in CFG_REG_C (62h).
 *
 * 0 1 Single mode. When single mode is selected, the device performs a
 * single measurement, sets DRDY high and returns to idle mode.
 * Mode register return to idle mode bit values.
 */
#define LIS2MDL_CONT_MODE		0x00
#define LIS2MDL_SINGLE_MODE		0x01

#define LIS2MDL_CFG_REG_B		0x61
#define LIS2MDL_CFG_REG_C		0x62
#define LIS2MDL_STATUS_REG		0x67
#define LIS2MDL_OUT_REG			0x68

/* Registers bitmask */
#define LIS2MDL_STS_MDA_UP		0x08
#define LIS2MDL_REBOOT			0x40
#define LIS2MDL_SOFT_RST		0x20
#define LIS2MDL_BDU_MASK		0x10
#define LIS2MDL_MODE_MASK		0x03
#define LIS2MDL_LP_MASK			0x10
#define LIS2MDL_COMP_TEMP_MASK		0x80

/* Output data size */
#define LIS2MDL_OUT_REG_SIZE		OUT_XYZ_SIZE

/* Device supported modes */
#define LIS2MDL_MD_CONTINUOS_MODE	0x00
#define LIS2MDL_MD_SINGLE_MODE		0x01
#define LIS2MDL_MD_IDLE1_MODE		0x02
#define LIS2MDL_MD_IDLE2_MODE		0x03
#define LIS2MDL_MAG_MODE_MSK		0x03

/* Supported device ODRs */
#define LIS2MDL_ODR10_HZ		0x00
#define LIS2MDL_ODR20_HZ		0x04
#define LIS2MDL_ODR50_HZ		0x08
#define LIS2MDL_ODR100_HZ		0x0c
#define LIS2MDL_ODR_OFFSET		2
#define LIS2MDL_ODR_MASK		(0x3 << LIS2MDL_ODR_OFFSET)

/* Define ODR supported range */
#define LIS2MDL_MAX_ODR			100000
#define LIS2MDL_MIN_ODR			10000

/* Return ODR real value normalized to sensor capabilities */
#define LIS2MDL_ODR_TO_NORMALIZE(_odr) \
	(_odr < 20000 ? LIS2MDL_MIN_ODR : \
	 _odr < 50000 ? 20000 : \
	 _odr < LIS2MDL_MAX_ODR ? 50000 : LIS2MDL_MAX_ODR)

/* Assume _odr is normalized */
#define LIS2MDL_ODR_TO_REG(_odr) (__fls(_odr))

/* Sensor resolution in number of bits */
#define LIS2MDL_RESOLUTION		16

/*
 * Maximum sensor data range (milligauss) :
 * Spec is 1.5 mguass / LSB, so 0.15 uT / LSB.
 * 0.15 << 15 = 4915.2(uT) (49.152 Gauss)
 */
#define LIS2MDL_RANGE			4915

extern const struct accelgyro_drv lis2mdl_drv;

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
/**
 * Cascade mag LIS2MDL + acc_gyro LSM6DSM/L
 * Use internal acc_gyro FIFO
 * Use acc_gyro Master I2C interface
 * Accelerometer event driven
 * @s - Motion Sensor struct
 */
int init_lis2mdl(const struct motion_sensor_t *s);
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

#endif /* __CROS_EC_MAG_LIS2MDL_H */
