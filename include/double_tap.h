/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Returns 1 if double tap is triggered to wake up device, 0 otherwise.
 */
int double_tap_get_state(void);

/**
 * Sets the state of double tap which means double tap is triggered.
 */
void double_tap_set_state(void);
