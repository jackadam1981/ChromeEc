/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for Chromebook keyboard backlight. */

#include "common.h"
#include "pwm.h"
#include "system.h"
#include "util.h"

#define PWMKBD_SYSJUMP_TAG 0x504b  /* "PK" */
#define PWM_HOOK_VERSION 1
/* Saved PWM state across sysjumps */
struct pwm_kbd_state {
	uint8_t kblight_en;
	uint8_t kblight_percent;
};

void pwm_kblight_init(void)
{
	const struct pwm_kbd_state *prev;
	int version, size;

	prev = (const struct pwm_kbd_state *)
		system_get_jump_tag(PWMKBD_SYSJUMP_TAG, &version, &size);
	if (prev && version == PWM_HOOK_VERSION && size == sizeof(*prev)) {
		/* Restore previous state. */
		pwm_enable(PWM_CH_KBLIGHT, prev->kblight_en);
		pwm_set_duty(PWM_CH_KBLIGHT, prev->kblight_percent);
	} else {
		/* Enable keyboard backlight control, turned down */
		pwm_set_duty(PWM_CH_KBLIGHT, 0);
		pwm_enable(PWM_CH_KBLIGHT, 1);
	}
}

void pwm_kblight_preserve_state(void)
{
	struct pwm_kbd_state state;

	state.kblight_en = pwm_get_enabled(PWM_CH_KBLIGHT);
	state.kblight_percent = pwm_get_duty(PWM_CH_KBLIGHT);

	system_add_jump_tag(PWMKBD_SYSJUMP_TAG, PWM_HOOK_VERSION,
			    sizeof(state), &state);
}

void pwm_kblight_enable(int enable)
{
	pwm_enable(PWM_CH_KBLIGHT, enable);
}

void pwm_kblight_set(int percent)
{
	pwm_set_duty(PWM_CH_KBLIGHT, percent);
}

int pwm_kblight_get(void)
{
	return pwm_get_duty(PWM_CH_KBLIGHT);
}

int pwm_kblight_state(void)
{
	return pwm_get_enabled(PWM_CH_KBLIGHT);
}
