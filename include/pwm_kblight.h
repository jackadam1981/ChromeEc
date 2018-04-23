/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM Keyboard Backlight API for Chrome EC */

#ifndef __CROS_EC_PWM_KBLIGHT_H
#define __CROS_EC_PWM_KBLIGHT_H

void pwm_kblight_init(void);
void pwm_kblight_preserve_state(void);
void pwm_kblight_set(int percent);
int pwm_kblight_get(void);
void pwm_kblight_enable(int enable);
int pwm_kblight_state(void);

#endif  /* __CROS_EC_PWM_KBLIGHT_H */
