/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Used to track sleep/deep sleep.
 */

#include "common.h"
#include "console.h"
#include "hwtimer.h"
#include "hooks.h"
#include "registers.h"
#include "extension.h"
#include "endian.h"
#include "sleep_diagnostics.h"
#include "system.h"
#include "timer.h"
#include "tpm_vendor_cmds.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

struct sleep_info_response state;
static timestamp_t reset_time;
static timestamp_t entered_sleep_time;
static uint32_t sleep_scale;

static void set_ds_time(uint32_t time_s)
{
	GREG32(PMU, PWRDN_SCRATCH20) = time_s;
	state.ds_time = time_s;
}

static void reset_sleep_info(void)
{
	reset_time = get_time();
	state.total_sleep_time = 0;
	set_ds_time(0);
}

static int process_sc_cmd(enum sleep_info_subcommand cmd)
{
	if (cmd == SLEEPV_US)
		sleep_scale = 1;
	else if (cmd == SLEEPV_MS)
		sleep_scale = MSEC;
	else if (cmd == SLEEPV_RESETNEXT)
		state.reset_next_sleep = 1;
	else if (cmd != SLEEPV_RESET)
		return EC_ERROR_PARAM1;

	CPRINTS("Clearing sleep info");
	reset_sleep_info();
	return VENDOR_RC_SUCCESS;
}

static int command_sc(int argc, char **argv)
{
	ccprintf("deep sleep time: %u s\n", GREG32(PMU, PWRDN_SCRATCH20));
	ccprintf("sleep time: ");

	if (sleep_scale == MSEC)
		ccprintf("%.3u", state.total_sleep_time);
	else
		ccprintf("%.6u", state.total_sleep_time);

	ccprintf(" s in %.6u s\n", time_since32(reset_time));

	if (argc > 1) {
		enum sleep_info_subcommand cmd = SLEEPV_RESET;

		if (!strcasecmp("us", argv[1]))
			cmd = SLEEPV_US;
		else if (!strcasecmp("ms", argv[1]))
			cmd = SLEEPV_MS;
		else if (!strcasecmp("resetnext", argv[1]))
			cmd = SLEEPV_RESETNEXT;
		else if (strcasecmp("reset", argv[1]))
			return EC_ERROR_PARAM1;
		process_sc_cmd(cmd);
	}
	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(sc, command_sc,
			     "[us|ms|resetnext|reset]",
			     "Keep track of sleep in us|ms. "
			     "Reset total time while entering sleep next or "
			     "reset now.");

void board_left_sleep(void)
{
	if (!entered_sleep_time.val)
		return;
	state.total_sleep_time += (time_since32(entered_sleep_time) /
				   sleep_scale);
}

void board_entered_sleep(void)
{
	if (state.reset_next_sleep) {
		state.reset_next_sleep = 0;
		reset_sleep_info();
	}
	entered_sleep_time = get_time();
}
static enum vendor_cmd_rc vc_get_sleep_info(enum vendor_cmd_cc code,
						 void *buf,
						 size_t input_size,
						 size_t *response_size)
{
	struct sleep_info_response response = {};

	response.total_time = htobe32(time_since32(reset_time));
	response.total_sleep_time = htobe32(state.total_sleep_time);
	response.sleep_scale = htobe32(sleep_scale);
	response.ds_time = htobe32(state.ds_time);

	CPRINTS("%s: done", __func__);
	*response_size = sizeof(response);
	memcpy(buf, &response, sizeof(response));
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_GET_SLEEP_INFO, vc_get_sleep_info);

static enum vendor_cmd_rc vc_set_sleep_info(enum vendor_cmd_cc code,
				     void *buf,
				     size_t input_size,
				     size_t *response_size)
{
	enum sleep_info_subcommand cmd;

	*response_size = 0; /* Just in case there is an error. */

	if (input_size != sizeof(cmd)) {
		CPRINTS("%s: sleep info size %d %d", __func__, input_size);
		return VENDOR_RC_BOGUS_ARGS;
	}
	memcpy(&cmd, buf, input_size);
	if (process_sc_cmd(cmd)) {
		CPRINTS("%s: invalid cmd %d", __func__, cmd);
		return VENDOR_RC_BOGUS_ARGS;
	}

	CPRINTS("%s: ran sleep info cmd %d", __func__, cmd);
	*response_size = 0;
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_SET_SLEEP_INFO, vc_set_sleep_info);

void board_config_pre_init(void)
{
	sleep_scale = 1;
	set_ds_time(GREG32(PMU, PWRDN_SCRATCH20) +
		    (__hw_clock_source_read() / SECOND));
}

static void sleep_info_init(void)
{
	if (!(system_get_reset_flags() & RESET_FLAG_HIBERNATE))
		GREG32(PMU, PWRDN_SCRATCH20) = 0;

	reset_time = get_time();
}
DECLARE_HOOK(HOOK_INIT, sleep_info_init, HOOK_PRIO_FIRST);
