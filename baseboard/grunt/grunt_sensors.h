/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Definitions of sensor configuration variables which may be overridden by
 * members of the Grunt family.
 */
#ifndef __CROS_GRUNT_SENSORS_H
#define __CROS_GRUNT_SENSORS_H

/*
 * The rotation matrix from the reference frame of the base IMU to the standard
 * reference frame.  Boards within the Grunt family may alter this value in
 * board_init() as needed for.
 */
extern matrix_3x3_t base_standard_ref;

#endif  /* __CROS_GRUNT_SENSORS_H */
