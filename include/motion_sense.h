/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Header for motion_sense.c */

#ifndef __CROS_EC_MOTION_SENSE_H
#define __CROS_EC_MOTION_SENSE_H

/* Link global variables for orientation. These must be defined in board.c */
extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
float rot_relative_sensor_orientation[3][3];

extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
float rot_base_to_up_direction[3][3];

extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
float rot_around_hinge[3][3];

extern
#ifndef CONFIG_ACCEL_CALIBRATE
const
#endif
struct vector hinge_axis;


/* 3-D vector structure. */
struct vector {
	int x;
	int y;
	int z;
};

#endif /* __CROS_EC_MOTION_SENSE_H */
