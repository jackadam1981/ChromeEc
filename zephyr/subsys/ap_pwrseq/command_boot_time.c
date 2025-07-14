// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_boot_time.h"
#include "console.h"

static int command_boot_time(int argc, const char **argv)
{
	const char *state_name;
	uint64_t display_time;
	int rv;

	if (argc != 2) {
		return EC_ERROR_PARAM_COUNT;
	}

	state_name = argv[1];

	rv = ap_power_get_boot_time(state_name, &display_time);
	if (rv != 0) {
		return rv;
	}

	if (display_time == uninitialized_time) {
		ccprints("first %s: %lldms", state_name, -1ll);
	} else {
		ccprints("first %s: %lldms", state_name,
			 (int64_t)(display_time / USEC_PER_MSEC));
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(boottime, command_boot_time, "<S0|S5>",
			"Expose boot time for firmware.BootTime.");
