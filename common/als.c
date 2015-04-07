/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This provides the interface for any Ambient Light Sensors that are connected
 * to the EC instead of the AP.
 */

#include "als.h"
#include "driver/als_isl29035.h"
#include "common.h"
#include "console.h"
#include "host_command.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#ifdef CONFIG_ALS_INTERRUPTS
static int als_int;
#endif

int als_read(enum als_id id, int *lux)
{
	int af = als[id].attenuation_factor;
	return als[id].read(lux, af);
}

void als_task(void)
{
	int i, val;
	uint16_t *mapped = (uint16_t *)host_get_memmap(EC_MEMMAP_ALS);
	uint16_t als_data;

	while (1) {
		for (i = 0; i < EC_ALS_ENTRIES && i < ALS_COUNT; i++) {
#ifdef CONFIG_ALS_INTERRUPTS
			if (EC_SUCCESS == isl29035_interrupt_handler())
				ccprintf("Interrupt occured on ALS %d\n", i);
#endif
			als_data = als_read(i, &val) == EC_SUCCESS ? val : 0;
			host_lock_memmap();
			mapped[i] = als_data;
			host_unlock_memmap();
		}

#ifdef CONFIG_ALS_INTERRUPTS
		if (als_int == 1)
			task_wait_event(-1);
		else
			task_wait_event(SECOND);
#else
		task_wait_event(SECOND);
#endif
	}
}

/*****************************************************************************/
/* Console commands */

static int command_als(int argc, char **argv)
{
	int i, rv, val;

	for (i = 0; i < ALS_COUNT; i++) {
		ccprintf("%s: ", als[i].name);
		rv = als_read(i, &val);
		switch (rv) {
		case EC_SUCCESS:
			ccprintf("%d lux\n", val);
			break;
		default:
			ccprintf("Error %d\n", rv);
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(als, command_als,
			NULL,
			"Print ALS values",
			NULL);

#ifdef CONFIG_ALS_INTERRUPTS
void als_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_ALS);
}

static int command_als_interrupt(int argc, char **argv)
{
	char *e;
	int id;
	int lower_thresh;
	int upper_thresh;
	int status;

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is id. */
	id = strtoi(argv[1], &e, 0);
	if (*e || id < 0 || id >= ALS_COUNT)
		return EC_ERROR_PARAM1;

	/* Second argument is lower interrupt threshold. */
	lower_thresh = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	/* Second argument is upper interrupt threshold. */
	upper_thresh = strtoi(argv[3], &e, 0);
	if (*e || upper_thresh <= lower_thresh)
		return EC_ERROR_PARAM2;

	status = isl29035_set_interrupt(lower_thresh, upper_thresh,
			als[id].attenuation_factor);
	if (status == EC_SUCCESS)
		als_int = 1;

	return status;
}
DECLARE_CONSOLE_COMMAND(alsint, command_als_interrupt,
			"als_id lower_threshold upper_threshold",
			"Write interrupt threshold",
			NULL);
#endif
