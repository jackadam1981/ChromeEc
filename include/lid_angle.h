/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lid angle module for Chrome EC */

#ifndef __CROS_EC_LID_ANGLE_H
#define __CROS_EC_LID_ANGLE_H

/**
 * Update the lid angle module with the most recent lid angle calculation. Then
 * use the lid angle history to enable/disable keyboard scanning when chipset
 * is suspended.
 *
 * @lid_ang Lid angle.
 */
void lidangle_keyscan_update(float lid_ang);

/**
 * Getter and setter methods for the keyboard disable angle. This angle is
 * the angle past which the keyboard is disabled as a wake source in S3.
 */
int lid_angle_get_kb_dis_angle(void);
void lid_angle_set_kb_dis_angle(int ang);

#endif  /* __CROS_EC_LID_ANGLE_H */
