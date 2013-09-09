/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common chipset throttling code for Chrome EC */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "task.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)


/*****************************************************************************/
/* This enforces the virtual OR of all throttling sources. */
static struct mutex throttle_mutex;
static uint32_t throttle_request;

void chipset_throttle_cpu(int throttle, enum throttle_sources source)
{
	mutex_lock(&throttle_mutex);

	if (throttle)
		throttle_request |= source;
	else
		throttle_request &= ~source;

	chipset_throttle_cpu_implementation(throttle_request);

	mutex_unlock(&throttle_mutex);
}

/*****************************************************************************/
/* Console commands */

static int command_apthrottle(int argc, char **argv)
{
	uint32_t tmpval;
	char *e;
	if (argc > 1) {
		tmpval = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;

		mutex_lock(&throttle_mutex);
		throttle_request = tmpval;	/* force source */
		mutex_unlock(&throttle_mutex);

		chipset_throttle_cpu(0, 0);	/* make it so */
	}

	mutex_lock(&throttle_mutex);
	tmpval = throttle_request;		/* read current value */
	mutex_unlock(&throttle_mutex);

	ccprintf("AP throttling is %s (0x%08x)\n",
		 tmpval ? "on" : "off", tmpval);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(apthrottle, command_apthrottle,
			"[BITMASK]",
			"Display or set the AP throttling state",
			NULL);

