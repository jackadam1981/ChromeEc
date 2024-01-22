/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI platform policy manager */

#include "stdbool.h"
#include "task.h"
#include "timer.h"

#define UCSI_PPM_EVENT_TIMEOUT (250 * MSEC)

static enum sm_local_state local_state;

static void ucsi_ppm_run(int evt, int en)
{
	switch (local_state) {
	case SM_PAUSED:
		if (!en)
			break;
		__fallthrough;
	case SM_INIT:
		dpm_init(port);
		local_state[port] = SM_RUN;
		__fallthrough;
	case SM_RUN:
		if (!en) {
			local_state[port] = SM_PAUSED;
			/*
			 * While we are paused, exit all states and wait until
			 * initialized again.
			 */
			set_state(port, &dpm[port].ctx, NULL);
			break;
		}

		/* Run state machine */
		run_state(port, &dpm[port].ctx);

		break;
	}
}

static bool ucsi_ppm_main(void)
{
	const uint32_t evt = task_wait_event(UCSI_PPM_EVENT_TIMEOUT);

	ucsi_ppm_run(evt, 1);

	return true;
}

void ucsi_ppm_task(void *u)
{
	while (1) {
		/* Initialization */

		/*
		 * As long as ucsi_ppm_main returns true, keep running the loop.
		 * ucsi_ppm_main returns false when the code needs to re-init
		 * the task.
		 */
		while (ucsi_ppm_main())
			continue;
	}
}
