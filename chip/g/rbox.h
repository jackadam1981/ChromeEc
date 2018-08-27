/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_RBOX_H
#define __CROS_RBOX_H

/**
 * Return true if the power button output shows it is pressed
 */
int rbox_powerbtn_is_pressed(void);

/**
 * Release EC-reset if it was held since the power reset for debugging purpose.
 */
void rbox_release_ec_reset_if_held(void);

/**
 * Cancel EC-reset if the power button was holding it.
 * Future call to rbox_release_ec_reset_if_held() won't release EC-reset.
 */
void rbox_cancel_release_ec_rst(void);

/**
 * Clear the wakeup interrupts
 */
void rbox_clear_wakeup(void);
#endif  /* __CROS_RBOX_H */
