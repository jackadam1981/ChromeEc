/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for Chromebook keyboard backlight. */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "pwm.h"
#include "system.h"
#include "util.h"
#include "cros_board_info.h"
#include "lm3509.h"

#define PWMKBD_SYSJUMP_TAG 0x504b  /* "PK" */
#define PWM_HOOK_VERSION 1
/* Saved PWM state across sysjumps */
struct pwm_kbd_state {
	uint8_t kblight_en;
	uint8_t kblight_percent;
};

/*****************************************************************************/
/* Console commands */

static int command_kblight(int argc, char **argv)
{
	uint32_t oem_id;

	if (argc >= 2) {
		char *e;
		int i = strtoi(argv[1], &e, 0);

		if (*e)
			return EC_ERROR_PARAM1;

		if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
			if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
				|| oem_id == PROJECT_PANTHEON) {
				gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);
				/* TODO: Need add LM3509 contorl for kblight */
			} else {
				pwm_set_duty(PWM_CH_KBLIGHT, i);
			}
		}
	}

	ccprintf("Keyboard backlight: %d%%\n", pwm_get_duty(PWM_CH_KBLIGHT));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kblight, command_kblight,
			"percent",
			"Set keyboard backlight");

/*****************************************************************************/
/* Host commands */

int pwm_command_get_keyboard_backlight(struct host_cmd_handler_args *args)
{
	struct ec_response_pwm_get_keyboard_backlight *r = args->response;
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);
			/* TODO: Add for LM3509 */
		} else {
			r->percent = pwm_get_duty(PWM_CH_KBLIGHT);
			r->enabled = pwm_get_enabled(PWM_CH_KBLIGHT);
			args->response_size = sizeof(*r);
		}
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_GET_KEYBOARD_BACKLIGHT,
		     pwm_command_get_keyboard_backlight,
		     EC_VER_MASK(0));

int pwm_command_set_keyboard_backlight(struct host_cmd_handler_args *args)
{
	const struct ec_params_pwm_set_keyboard_backlight *p = args->params;
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);
			/* TODO: Add for LM3509 */
		} else {
			pwm_set_duty(PWM_CH_KBLIGHT, p->percent);
		}
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_SET_KEYBOARD_BACKLIGHT,
		     pwm_command_set_keyboard_backlight,
		     EC_VER_MASK(0));

/*****************************************************************************/
/* Hooks */

static void pwm_kblight_init(void)
{
	const struct pwm_kbd_state *prev;
	int version, size;
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);
			/* TODO: Add for LM3509 */
		} else {
			prev = (const struct pwm_kbd_state *)
				system_get_jump_tag(PWMKBD_SYSJUMP_TAG, &version, &size);
			if (prev && version == PWM_HOOK_VERSION &&
				size == sizeof(*prev)) {
				/* Restore previous state. */
				pwm_enable(PWM_CH_KBLIGHT, prev->kblight_en);
				pwm_set_duty(PWM_CH_KBLIGHT, prev->kblight_percent);
			} else {
				/* Enable keyboard backlight control, turned down */
				pwm_set_duty(PWM_CH_KBLIGHT, 0);
				pwm_enable(PWM_CH_KBLIGHT, 1);
			}
		}
	}
}
DECLARE_HOOK(HOOK_INIT, pwm_kblight_init, HOOK_PRIO_DEFAULT);

static void pwm_kblight_preserve_state(void)
{
	struct pwm_kbd_state state;
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);
			/* TODO: Add for LM3509 */
		} else {
			state.kblight_en = pwm_get_enabled(PWM_CH_KBLIGHT);
			state.kblight_percent = pwm_get_duty(PWM_CH_KBLIGHT);
		}
	}

	system_add_jump_tag(PWMKBD_SYSJUMP_TAG, PWM_HOOK_VERSION,
			    sizeof(state), &state);
}
DECLARE_HOOK(HOOK_SYSJUMP, pwm_kblight_preserve_state, HOOK_PRIO_DEFAULT);

static void kblight_resume(void)
{
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 0);
			if (lid_is_open())
				lm3509_poweron();
		} else {
			pwm_set_duty(PWM_CH_KBLIGHT, 1);
		}
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, kblight_resume, HOOK_PRIO_DEFAULT);

static void kblight_suspend(void)
{
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			gpio_set_level(GPIO_ENABLE_BACKLIGHT_L, 1);
			lm3509_poweroff();
		} else {
			pwm_set_duty(PWM_CH_KBLIGHT, 0);
		}
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, kblight_suspend, HOOK_PRIO_DEFAULT);

static void kblight_lid_change(void)
{
	uint32_t oem_id;

	if (cbi_get_oem_id(&oem_id) == EC_SUCCESS) {
		if (oem_id == PROJECT_NAMI || oem_id == PROJECT_VAYNE
			|| oem_id == PROJECT_PANTHEON) {
			if (lid_is_open())
				lm3509_poweron();
			else
				lm3509_poweroff();
		}  else {
			pwm_enable(PWM_CH_KBLIGHT, lid_is_open());
		}
	}
}
DECLARE_HOOK(HOOK_LID_CHANGE, kblight_lid_change, HOOK_PRIO_DEFAULT);
