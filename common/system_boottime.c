/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "system.h"
#include "util.h"

/* Content of g_boot_time will be lost on sysjump */
static struct ec_boot_time_data g_boot_time;

/* Updates ap boot time */
void update_ap_boot_time(enum boot_time_param param)
{
	uint64_t t = get_time().val;

	switch (param) {
	case ARAIL:
		g_boot_time.arail = t;
		break;
	case RSMRST:
		g_boot_time.rsmrst = t;
		break;
	case ESPIRST:
		g_boot_time.espirst = t;
		break;
	case PLTRST_LOW:
		g_boot_time.pltrst_low = t;
		g_boot_time.cnt++;
		break;
	case PLTRST_HIGH:
		g_boot_time.pltrst_high = t;
		break;
	case EC_CUR_TIME:
		g_boot_time.ec_cur_time = t;
		break;
	case RESET_CNT:
		t = g_boot_time.cnt = 0;
		break;
	}
	ccprintf("Boot Time: %d, %lld\n", param, t);
}

/* Returns system boot time data */
static enum ec_status
host_command_get_boot_time(struct host_cmd_handler_args *args)
{
	struct ec_boot_time_data *boot_time = args->response;

	/* update current time */
	update_ap_boot_time(EC_CUR_TIME);

	/* copy data from g_boot_time struct */
	memcpy(boot_time, &g_boot_time, sizeof(*boot_time));

	args->response_size = sizeof(*boot_time);

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_GET_BOOT_TIME, host_command_get_boot_time,
		     EC_VER_MASK(0));
