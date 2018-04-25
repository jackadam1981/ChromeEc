/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chromebook keyboard backlight. */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "system.h"
#include "util.h"
#include "kblight.h"
#include "pwm_kblight.h"
#include "task.h"

static struct kblight_drv *kb_drv;

static int percentage;

static void _kblight_set(int percent)
{
	if (kb_drv && kb_drv->set)
		kb_drv->set(percentage);
	else
		ccprintf("Missing kb_drv or kb_drv->set()\n");
}

static int _kblight_get(void)
{
	if (kb_drv && kb_drv->get)
		return kb_drv->get();
	ccprintf("Missing kb_drv or kb_drv->get()\n");
	return 0;
}

static void kblight_deferred(void);
DECLARE_DEFERRED(kblight_deferred);
static void kblight_deferred(void)
{
	int curr;
	static int retry_counter;

	if (percentage == _kblight_get()) {
		retry_counter = 0;
		return;
	}

	_kblight_set(percentage);
	curr = _kblight_get();

	/*
	 * If kblight_set() is called during configuration, or some errors
	 * are happened when calling kb_drv->get()/set(), reschedule the
	 * deferred function with a delay to avoid starvation and watchdog
	 * timeout.
	 */
	if (curr != percentage) {
		/*
		 * Retry 3 times, if the update keeps failure, stop to defer
		 * function to avoid infinite loop. Print error instead.
		 */
		if (retry_counter >= 3) {
			ccprintf("Update fail, expected:% got:%d\n",
				 percentage, curr);
		} else {
			hook_call_deferred(&kblight_deferred_data, 100*MSEC);
			++retry_counter;
		}
	} else {
		retry_counter = 0;
	}
}

int kblight_driver_register(struct kblight_drv *drv)
{
	kb_drv = drv;
	return 0;
}

int kblight_get(void)
{
	/*
	 * acpi interrupt will call this function,
	 * don't use __wait_evt() related api
	 */
	return percentage;
}

void kblight_set(int percent)
{
	/* Adjust the brightness if the value is out of range */
	if (percent < 0) {
		ccprintf("Set brightness value must >= 0\n");
		percent = 0;
	} else if (percent > 100) {
		ccprintf("Set brightness value must <= 100\n");
		percent = 100;
	}
	/*
	 * acpi interrupt will call this function,
	 * don't use __wait_evt() related api
	 */
	percentage = percent;
	hook_call_deferred(&kblight_deferred_data, 0);
}

void kblight_enable(int enable)
{
	if (kb_drv && kb_drv->enable)
		kb_drv->enable(enable);
	else
		ccprintf("Missing kb_drv or kb_drv->enable()\n");
}

int kblight_is_enable(void)
{
	if (kb_drv && kb_drv->is_enable)
		return kb_drv->is_enable();
	ccprintf("Missing kb_drv or kb_drv->is_enable()\n");
	return 0;
}

/*****************************************************************************/
/* Console commands */

static int command_kblight(int argc, char **argv)
{
	if (argc >= 2) {
		char *e;
		int i = strtoi(argv[1], &e, 0);

		if (*e)
			return EC_ERROR_PARAM1;
		kb_drv->set(i);
	}

	ccprintf("Keyboard backlight: %d%%\n", kb_drv->get());
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kblight, command_kblight,
			"percent",
			"Set keyboard backlight");

/*****************************************************************************/
/* Host commands */

static int command_get_keyboard_backlight(struct host_cmd_handler_args *args)
{
	struct ec_response_get_keyboard_backlight *r = args->response;

	r->percent = kb_drv->get();
	r->enabled = kb_drv->is_enable();
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_KEYBOARD_BACKLIGHT,
		     command_get_keyboard_backlight,
		     EC_VER_MASK(0));

static int command_set_keyboard_backlight(struct host_cmd_handler_args *args)
{
	const struct ec_params_set_keyboard_backlight *p = args->params;

	kb_drv->set(p->percent);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_SET_KEYBOARD_BACKLIGHT,
		     command_set_keyboard_backlight,
		     EC_VER_MASK(0));

/*****************************************************************************/
/* Hooks */

static void kblight_init(void)
{
	if (kb_drv && kb_drv->init)
		kb_drv->init();
}
DECLARE_HOOK(HOOK_INIT, kblight_init, HOOK_PRIO_DEFAULT);

static void kblight_preserve_state(void)
{
	if (kb_drv && kb_drv->preserve_state)
		kb_drv->preserve_state();
}
DECLARE_HOOK(HOOK_SYSJUMP, kblight_preserve_state, HOOK_PRIO_DEFAULT);

static void kblight_resume(void)
{
	kblight_enable(lid_is_open());
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, kblight_resume, HOOK_PRIO_DEFAULT);

static void kblight_suspend(void)
{
	kblight_enable(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, kblight_suspend, HOOK_PRIO_DEFAULT);

static void kblight_shutdown(void)
{
	kblight_enable(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, kblight_shutdown, HOOK_PRIO_DEFAULT);

static void kblight_lid_change(void)
{
	kblight_enable(lid_is_open());
}
DECLARE_HOOK(HOOK_LID_CHANGE, kblight_lid_change, HOOK_PRIO_DEFAULT);
