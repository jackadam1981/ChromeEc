/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This provides the interface for any Ambient Light Sensors that are connected
 * to the EC instead of the AP.
 */

#include "als.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define ALS_POLL_PERIOD SECOND

static int task_timeout = -1;
static struct motion_sensor_t als_sns;

void als_task(void)
{
	uint16_t *mapped = (uint16_t *)host_get_memmap(EC_MEMMAP_ALS);

	als_sns.port = als.drv->port;
	als_sns.addr = als.drv->addr;
	als_sns.drv_data = (void *) &als.attenuation_factor;

	while (1) {
		task_wait_event(task_timeout);

		/* If task was disabled while waiting do not read from ALS */
		if (task_timeout < 0)
			continue;

		mapped[0] = als.drv->read(&als_sns, als_sns.raw_xyz) ?
				0 : als_sns.raw_xyz[0];
	}
}

static void als_task_enable(void)
{
	int err;

	err = als.drv->init(&als_sns);

	/* If ALS failed to initialize, disable the ALS task. */
	if (err) {
		ccprintf("%s ALS sensor failed to initialize, err=%d\n",
				als.drv->name, err);
		task_timeout = -1;
	} else
		task_timeout = ALS_POLL_PERIOD;

	task_wake(TASK_ID_ALS);
}

static void als_task_disable(void)
{
	task_timeout = -1;
}

static void als_task_init(void)
{
	/*
	 * Enable ALS task in S0 only and may need to re-enable
	 * when sysjumped.
	 */
	if (system_jumped_to_this_image() &&
		chipset_in_state(CHIPSET_STATE_ON))
		als_task_enable();
}

DECLARE_HOOK(HOOK_CHIPSET_RESUME, als_task_enable, HOOK_PRIO_ALS_INIT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, als_task_disable, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, als_task_init, HOOK_PRIO_ALS_INIT);

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CMD_ALS
static int command_als(int argc, char **argv)
{
	int rv;

	ccprintf("%s: ", als.drv->name);
	rv = als.drv->read(&als_sns, als_sns.raw_xyz);
	if (rv)
		ccprintf("Error %d\n", rv);
	else
		ccprintf("%d lux\n", als_sns.raw_xyz[0]);

	return rv;
}
DECLARE_CONSOLE_COMMAND(als, command_als,
			NULL,
			"Print ALS values");
#endif
