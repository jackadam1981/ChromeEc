/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This array must be defined in board.c. */
extern const int accel_addr[];

/* This enum must be defined in board.h. */
enum accel_id;

/* Link global variables for orientation. These must be defined in board.c */
extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
float rot_sense_orientation[3][3];

extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
float rot_up_direction[3][3];

extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
struct vector hinge_axis;


/* Number of counts from accelerometer that represents 1G acceleration. */
#define ACCEL_G  1024

/**
 * Read all three accelerations of an accelerometer.
 *
 * @param id Target accelerometer
 * @param x_acc Pointer to location to store X-axis acceleration.
 * @param y_acc Pointer to location to store Y-axis acceleration.
 * @param z_acc Pointer to location to store Z-axis acceleration.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int accel_read(enum accel_id id, int *x_acc, int *y_acc, int *z_acc);

/**
 * Initiailze accelerometers.
 *
 * @param id Target accelerometer
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int accel_init(enum accel_id id);
