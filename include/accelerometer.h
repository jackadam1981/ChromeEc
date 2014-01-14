/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Read all three accelerations of an accelerometer.
 *
 * @param addr Slave address of accelerometer.
 * @param x_acc Pointer to location to store X-axis acceleration.
 * @param y_acc Pointer to location to store Y-axis acceleration.
 * @param z_acc Pointer to location to store Z-axis acceleration.
 */
int read_accel(int addr, int *x_acc, int *y_acc, int *z_acc);

/**
 * Initiailze accelerometers.
 *
 * @param addr Slave address of accelerometer.
 */
void accel_init(int addr);
