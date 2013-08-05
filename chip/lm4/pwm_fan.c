/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for Chromebook fans */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lm4_pwm.h"
#include "pwm.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "thermal.h"
#include "timer.h"
#include "util.h"

/* Maximum RPM for fan controller */
#define MAX_RPM 0x1fff
/* Max PWM for fan controller */
#define MAX_PWM 0x1ff
/*
 * Scaling factor for requested/actual RPM for CPU fan.  We need this because
 * the fan controller on Blizzard filters tach pulses that are less than 64
 * 15625Hz ticks apart, which works out to ~7000rpm on an unscaled fan.  By
 * telling the controller we actually have twice as many edges per revolution,
 * the controller can handle fans that actually go twice as fast.  See
 * crosbug.com/p/7718.
 */
#define CPU_FAN_SCALE 2

#define PWMFAN_SYSJUMP_TAG 0x5046  /* "PF" */
#define PWM_HOOK_VERSION 1
/* Saved PWM state across sysjumps */
struct pwm_fan_state {
	uint16_t fan_rpm;
	uint8_t fan_en;
	char pad; /* Pad to multiple of 4 bytes. */
};

/*****************************************************************************/
/* Console commands */

static int command_fan_info(int argc, char **argv)
{
	ccprintf("Actual: %4d rpm\n", pwm_get_rpm(PWM_CH_FAN));
	ccprintf("Target: %4d rpm\n", pwm_get_target_rpm(PWM_CH_FAN));
	ccprintf("Duty:   %d%%\n", pwm_get_duty(PWM_CH_FAN));
	ccprintf("Stalled:%d\n", pwm_is_stalled(PWM_CH_FAN) ? "yes" : "no");
	ccprintf("Mode:   %s\n", pwm_get_rpm_mode(PWM_CH_FAN) ? "rpm" : "duty");
	ccprintf("Enable: %s\n",
		 pwm_get_enabled(PWM_CH_FAN) ? "yes" : "no");
#ifdef BOARD_link				/* HEY: Slippy? */
	ccprintf("Power:  %s\n",
		 gpio_get_level(GPIO_PGOOD_5VALW) ? "yes" : "no");
#endif

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(faninfo, command_fan_info,
			NULL,
			"Print fan info",
			NULL);

static int command_fan_set(int argc, char **argv)
{
	int rpm = 0;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	rpm = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	/* Move the fan to automatic control */
	pwm_set_rpm_mode(PWM_CH_FAN, 1);

	/* Always enable the fan */
	pwm_enable(PWM_CH_FAN, 1);

#ifdef HAS_TASK_THERMAL
	/* Disable thermal engine automatic fan control. */
	thermal_control_fan(0);
#endif

	pwm_set_target_rpm(PWM_CH_FAN, rpm);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fanset, command_fan_set,
			"rpm",
			"Set fan speed",
			NULL);

static int ec_command_fan_duty(int argc, char **argv)
{
	int percent = 0;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	percent = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	ccprintf("Setting fan duty cycle to %d%%\n", percent);
	pwm_set_duty(PWM_CH_FAN, percent);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fanduty, ec_command_fan_duty,
			"percent",
			"Set fan duty cycle",
			NULL);

/*****************************************************************************/
/* Host commands */

int pwm_command_get_fan_target_rpm(struct host_cmd_handler_args *args)
{
	struct ec_response_pwm_get_fan_rpm *r = args->response;

	r->rpm = pwm_get_target_rpm(PWM_CH_FAN);
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_GET_FAN_TARGET_RPM,
		     pwm_command_get_fan_target_rpm,
		     EC_VER_MASK(0));

int pwm_command_set_fan_target_rpm(struct host_cmd_handler_args *args)
{
	const struct ec_params_pwm_set_fan_target_rpm *p = args->params;

#ifdef HAS_TASK_THERMAL
	thermal_control_fan(0);
#endif
	pwm_set_rpm_mode(PWM_CH_FAN, 1);
	pwm_set_target_rpm(PWM_CH_FAN, p->rpm);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_SET_FAN_TARGET_RPM,
		     pwm_command_set_fan_target_rpm,
		     EC_VER_MASK(0));

int pwm_command_fan_duty(struct host_cmd_handler_args *args)
{
	const struct ec_params_pwm_set_fan_duty *p = args->params;
	pwm_set_duty(PWM_CH_FAN, p->percent);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_SET_FAN_DUTY,
		     pwm_command_fan_duty,
		     EC_VER_MASK(0));

/*****************************************************************************/
/* Hooks */

static void pwm_fan_init(void)
{
	const struct pwm_fan_state *prev;
	uint16_t *mapped;
	int version, size;
	int i;

	prev = (const struct pwm_fan_state *)
		system_get_jump_tag(PWMFAN_SYSJUMP_TAG, &version, &size);
	if (prev && version == PWM_HOOK_VERSION && size == sizeof(*prev)) {
		/* Restore previous state. */
		pwm_enable(PWM_CH_FAN, prev->fan_en);
		pwm_set_target_rpm(PWM_CH_FAN, prev->fan_rpm);
	} else {
		/* Set initial fan speed to maximum */
		pwm_set_target_rpm(PWM_CH_FAN, -1);
	}

	/* Initialize memory-mapped data */
	mapped = (uint16_t *)host_get_memmap(EC_MEMMAP_FAN);
	for (i = 0; i < EC_FAN_SPEED_ENTRIES; i++)
		mapped[i] = EC_FAN_SPEED_NOT_PRESENT;
}
DECLARE_HOOK(HOOK_INIT, pwm_fan_init, HOOK_PRIO_DEFAULT + 1);

static void pwm_fan_second(void)
{
	uint16_t *mapped = (uint16_t *)host_get_memmap(EC_MEMMAP_FAN);

	if (pwm_is_stalled(PWM_CH_FAN)) {
		mapped[0] = EC_FAN_SPEED_STALLED;
		/*
		 * Issue warning.  As we have thermal shutdown
		 * protection, issuing warning here should be enough.
		 */
		host_set_single_event(EC_HOST_EVENT_THERMAL);
		cprintf(CC_PWM, "[%T Fan stalled!]\n");
	} else {
		mapped[0] = pwm_get_rpm(PWM_CH_FAN);
	}
}
DECLARE_HOOK(HOOK_SECOND, pwm_fan_second, HOOK_PRIO_DEFAULT);

static void pwm_fan_preserve_state(void)
{
	struct pwm_fan_state state;

	state.fan_en = pwm_get_enabled(PWM_CH_FAN);
	state.fan_rpm = pwm_get_target_rpm(PWM_CH_FAN);

	system_add_jump_tag(PWMFAN_SYSJUMP_TAG, PWM_HOOK_VERSION,
			    sizeof(state), &state);
}
DECLARE_HOOK(HOOK_SYSJUMP, pwm_fan_preserve_state, HOOK_PRIO_DEFAULT);

static void pwm_fan_resume(void)
{
	pwm_enable(PWM_CH_FAN, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, pwm_fan_resume, HOOK_PRIO_DEFAULT);

static void pwm_fan_suspend(void)
{
	pwm_enable(PWM_CH_FAN, 0);
	pwm_set_target_rpm(PWM_CH_FAN, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, pwm_fan_suspend, HOOK_PRIO_DEFAULT);
