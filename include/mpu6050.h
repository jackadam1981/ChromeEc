/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MPU6050 driver.
 */

#ifndef MPU6050_H
#define MPU6050_H

#define MPU6050_REG_SMPLRT_DIV	0x19
#define MPU6050_REG_CONFIG	0x1a
#define MPU6050_REG_GYRO_CONFIG	0x1b
#define MPU6050_REG_GYRO_XOUT_H	0x43
#define MPU6050_REG_GYRO_XOUT_L	0x44
#define MPU6050_REG_GYRO_YOUT_H	0x45
#define MPU6050_REG_GYRO_YOUT_L	0x46
#define MPU6050_REG_GYRO_ZOUT_H	0x47
#define MPU6050_REG_GYRO_ZOUT_L	0x48
#define MPU6050_REG_PWR_MGMT_1	0x6b
#define MPU6050_REG_WHO_AM_I	0x75

#endif  /* MPU6050_H */
