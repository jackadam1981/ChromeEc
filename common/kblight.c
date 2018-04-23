/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
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

static struct kblight_drv *kb_drv;

int kblight_driver_register(struct kblight_drv *drv)
{
	kb_drv = drv;
	return 0;
}

int kblight_get(void)
{
	if (kb_drv)
		return kb_drv->get();
	return 0;
}

void kblight_set(int percent)
{
	if (kb_drv)
		kb_drv->set(percent);
}

int kblight_state(void)
{
	if (kb_drv)
		return kb_drv->state();
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
	r->enabled = kb_drv->state();
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

static void kblight_suspend(void)
{
	if (kb_drv && kb_drv->set)
		kb_drv->set(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, kblight_suspend, HOOK_PRIO_DEFAULT);

static void kblight_shutdown(void)
{
	if (kb_drv && kb_drv->set)
		kb_drv->set(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, kblight_shutdown, HOOK_PRIO_DEFAULT);

static void kblight_lid_change(void)
{
	if (kb_drv && kb_drv->enable)
		kb_drv->enable(lid_is_open());
}
DECLARE_HOOK(HOOK_LID_CHANGE, kblight_lid_change, HOOK_PRIO_DEFAULT);
