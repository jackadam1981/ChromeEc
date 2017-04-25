/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "host_command.h"
#include "console.h"
#include "util.h"

/* Return 1 if in tablet mode, 0 otherwise */
static int tablet_mode = 1;

int tablet_get_mode(void)
{
	return tablet_mode;
}

void tablet_set_mode(int mode)
{
	if (tablet_mode != mode) {
		tablet_mode = mode;
		hook_notify(HOOK_TABLET_MODE_CHANGE);
	}
}

static int command_tablet_mode(int argc, char **argv)
{
	int mode = 1;

	if (argc > 1 && !strcasecmp(argv[1], "on"))
		mode = 1;
	else if (argc > 1 && !strcasecmp(argv[1], "off"))
		mode = 0;
	host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
	tablet_set_mode(mode);
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(tablet_mode, command_tablet_mode,
			"[on | off]",
			"Simulate tablet mode");

