/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_LED_PWM_H__
#define __CROS_EC_LED_PWM_H__

#define COMPAT_PWM	cros_ec_multi_pwm_leds

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM)
/*
 * Enum representing the multi-PWM LED handlers.
 * One is created for each instance of the multi-PWM LED
 * handlers.
 */
enum led_pwm_hand {
	DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_TYPE_INDEX_ENUM)
};

DT_FOREACH_STATUS_OKAY(COMPAT_PWM, GEN_ACTION_ENUM_LIST)

/*
 * PWM functions.
 */
void pwm_led_init(void);
void pwm_get_led_brightness(enum led_pwm_hand h, uint8_t *br);
void pwm_set_led_brightness(enum led_pwm_hand h, const uint8_t *br);
void pwm_set_led_action(enum led_pwm_hand handler, int action);
void pwm_led_shutdown(enum led_pwm_hand handler);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_PWM) */

#endif /* __CROS_EC_LED_PWM_H__ */
