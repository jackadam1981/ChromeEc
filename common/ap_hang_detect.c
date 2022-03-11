/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AP hang detect logic */

#include "ap_hang_detect.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "power_button.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) \
	cprints(CC_CHIPSET, "hang_detect: " format, ##args)
#define CPRINTS_IDX(format, args...) \
	cprints(CC_CHIPSET, "hang_detect[%d]: " format, ##args)

#define HANG_TIMER_COUNT 4

static struct ec_params_hang_detect_v1 hdparams[HANG_TIMER_COUNT];
static int active[HANG_TIMER_COUNT]; /* Is hang detect timer active? */
static char timer_why[HANG_TIMER_COUNT][16] = {0};
static timestamp_t timers[HANG_TIMER_COUNT];

static void handle_hang(int i)
{
	/* If we're no longer active, nothing to do */
	if (!active[i])
		return;

	active[i] = 0;

	CPRINTS_IDX("AP hang detected after '%s'!", i, timer_why[i]);

	strzcpy(timer_why[i], "hang detected", sizeof(timer_why[i]));

	if (hdparams[i].flags & EC_HANG_SEND_EVENT_ON_HANG) {
		CPRINTS_IDX("sending hang host event", i);
		host_set_single_event(EC_HOST_EVENT_HANG_DETECT);
	}

	if (hdparams[i].flags & EC_HANG_WARM_RESET_ON_HANG) {
		CPRINTS_IDX("triggering warm reboot", i);
		host_set_single_event(EC_HOST_EVENT_HANG_REBOOT);
		chipset_reset(CHIPSET_RESET_HANG_REBOOT);
	}
}

/**
 * Check for expired timers.
 * Only check once a second because hang detection does not need to be
 * high fidelity.
 */
static void check_timers(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++)
		if (active[i] && timestamp_expired(timers[i], NULL))
			handle_hang(i);
}
DECLARE_HOOK(HOOK_SECOND, check_timers, HOOK_PRIO_DEFAULT);

/**
 * Start the hang detect timers.
 */
static void hang_detect_start(const char *why, int i)
{
	/* If already active, don't restart timer */
	if (active[i])
		return;

	if (hdparams[i].timeout_msec) {
		CPRINTS_IDX("timer started on '%s'", i, why);
		active[i] = true;
		strzcpy(timer_why[i], why, sizeof(timer_why[i]));
		timers[i].val = get_time().val +
				hdparams[i].timeout_msec * MSEC;
	}
}

/**
 * Stop the hang detect timers.
 */
static void hang_detect_stop(const char *why, int i)
{
	if (active[i]) {
		CPRINTS_IDX("'%s' timer stopped on '%s'", i, timer_why[i], why);
		strzcpy(timer_why[i], why, sizeof(timer_why[i]));
	}

	active[i] = 0;
}

void hang_detect_stop_on_host_command(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++)
		if (hdparams[i].flags & EC_HANG_STOP_ON_HOST_COMMAND)
			hang_detect_stop("host cmd", i);
}

/*****************************************************************************/
/* Hooks */

static void hang_detect_power_button(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++) {
		if (power_button_is_pressed()) {
			if (hdparams[i].flags & EC_HANG_START_ON_POWER_PRESS)
				hang_detect_start("power button", i);
		} else {
			if (hdparams[i].flags & EC_HANG_STOP_ON_POWER_RELEASE)
				hang_detect_stop("power button", i);
		}
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, hang_detect_power_button,
	     HOOK_PRIO_DEFAULT);

static void hang_detect_lid(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++) {
		if (lid_is_open()) {
			if (hdparams[i].flags & EC_HANG_START_ON_LID_OPEN)
				hang_detect_start("lid open", i);
		} else {
			if (hdparams[i].flags & EC_HANG_START_ON_LID_CLOSE)
				hang_detect_start("lid close", i);
		}
	}
}
DECLARE_HOOK(HOOK_LID_CHANGE, hang_detect_lid, HOOK_PRIO_DEFAULT);

static void hang_detect_resume(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++)
		if (hdparams[i].flags & EC_HANG_START_ON_RESUME)
			hang_detect_start("resume", i);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, hang_detect_resume, HOOK_PRIO_DEFAULT);

static void hang_detect_suspend(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++)
		if (hdparams[i].flags & EC_HANG_STOP_ON_SUSPEND)
			hang_detect_stop("suspend", i);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, hang_detect_suspend, HOOK_PRIO_DEFAULT);

static void hang_detect_reset(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++)
		if (hdparams[i].flags & EC_HANG_STOP_ON_RESET)
			hang_detect_stop("reset", i);
}
DECLARE_HOOK(HOOK_CHIPSET_RESET, hang_detect_reset, HOOK_PRIO_DEFAULT);

static void hang_detect_shutdown(void)
{
	for (int i = 0; i < HANG_TIMER_COUNT; i++) {
		/* Stop the timers */
		hang_detect_stop("shutdown", i);

		/* Disable hang detection; it must be enabled every boot */
		memset(&hdparams[i], 0, sizeof(hdparams[i]));
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, hang_detect_shutdown, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Host command */

static enum ec_status
hang_detect_host_command(struct host_cmd_handler_args *args)
{
	const struct ec_params_hang_detect_v1 *p = args->params;
	int i = p->timer_index;

	/* Handle stopping hang timer on request */
	if (p->flags & EC_HANG_STOP_NOW) {
		hang_detect_stop("ap request", i);

		/* Ignore the other params */
		return EC_RES_SUCCESS;
	}

	/* Handle starting hang timer on request */
	if (p->flags & EC_HANG_START_NOW) {
		hang_detect_start("ap request", i);

		/* Ignore the other params */
		return EC_RES_SUCCESS;
	}

	/* If hang detect transitioning to disabled, stop timers */
	if (hdparams[i].flags && !p->flags)
		hang_detect_stop("ap flags=0", i);

	/* Save new params */
	hdparams[i] = *p;
	CPRINTS_IDX("flags=0x%x, timeout=%dms", i, hdparams[i].flags,
		    hdparams[i].timeout_msec);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_HANG_DETECT,
		     hang_detect_host_command,
		     EC_VER_MASK(1));

/*****************************************************************************/
/* Console command */

static int command_hang_detect(int argc, char **argv)
{
	char *e;
	int i = 0;

	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	if (argc == 2) {
		i = strtoi(argv[1], &e, 10);
		if (i < 0 || i > HANG_TIMER_COUNT-1)
			return EC_ERROR_PARAM1;
	}

	ccprintf("index:  %d\n", i);

	ccprintf("flags:  0x%x\n", hdparams[i].flags);

	ccprintf("timeout:  %dms\n", hdparams[i].timeout_msec);

	ccputs("status: ");
	if (active[i])
		ccprintf("active since '%s'\n", timer_why[i]);
	else if (timer_why[i][0])
		ccprintf("inactive since '%s'\n", timer_why[i]);
	else
		ccputs("inactive\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hangdet, command_hang_detect,
			"<timer_index>",
			"Print hang detect state");
