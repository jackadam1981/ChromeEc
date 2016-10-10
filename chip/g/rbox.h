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
 * Return true if power button rbox output override is enabled
 */
int rbox_powerbtn_override_is_enabled(void);

/**
 * Disable the output override
 */
void rbox_powerbtn_disable_override(void);

/**
 * Override power button output to simulate a power button press
 *
 * @param powerbtn_val	1 to simulate press 0 for release
 */
void rbox_powerbtn_override(int powerbtn_val);
#endif  /* __CROS_RBOX_H */
