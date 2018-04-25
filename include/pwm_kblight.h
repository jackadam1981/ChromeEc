/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM Keyboard Backlight API for Chrome EC */

#ifndef __CROS_EC_PWM_KBLIGHT_H
#define __CROS_EC_PWM_KBLIGHT_H

/**
 * Register pwm as the keyboard backlight underlying interface.
 * This function will register the callback function for the
 * keyboard backlight driver.
 */
void pwm_kblight_register(void);

#endif  /* __CROS_EC_PWM_KBLIGHT_H */
