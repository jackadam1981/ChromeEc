/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>
#include <string.h>

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "host_command.h"
#include "power_button.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)

/*
 * Was this power on initiated to show a charge splash?
 *
 * - Set when powering on for an AC connect.
 * - Unset when power button is pushed, or the chargesplash request is
 *   cancelled due to AC disconnection.
 */
static bool power_on_for_chargesplash;

bool chargesplash_get_boot_mode(void)
{
	return power_on_for_chargesplash;
}

/*
 * We maintain a counter of the number of times we have failed to boot
 * to the chargesplash before the AC was disconnected.  When this
 * counter increments too high, it could indicate an adapter or cable
 * issue (disconnecting frequently).  If we get more than 5 failures,
 * we lockout the chargesplash feature to prevent power waste.  The
 * only way to re-enable the chargesplash feature after a lockout is
 * an EC reset.
 */
#define CHARGESPLASH_BOOT_MAX_TRIES 5
static int chargesplash_boot_tries;

static void boot_for_chargesplash_fail(void)
{
	CPRINTS("Cancel chargesplash request");
	chipset_force_shutdown(CHIPSET_SHUTDOWN_CHARGESPLASH_CANCEL);
	power_on_for_chargesplash = false;
	chargesplash_boot_tries++;
}

static void boot_for_chargesplash(void)
{
	if (chargesplash_boot_tries >= CHARGESPLASH_BOOT_MAX_TRIES) {
		CPRINTS("Lockout chargesplash (max tries exceeded)");
		return;
	}

	CPRINTS("Power on for chargesplash");
	power_on_for_chargesplash = true;
	chipset_power_on();
}

static void handle_ac_change(void)
{
	if (extpower_is_present()) {
		/* AC connect event */
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
			boot_for_chargesplash();
		}

		/* TODO: Handle wake condition when we're in suspend */
	} else {
		/* AC disconnect event */
		if (power_on_for_chargesplash) {
			boot_for_chargesplash_fail();
		}
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, handle_ac_change, HOOK_PRIO_LAST);

static void handle_power_button_change(void)
{
	if (power_button_is_pressed() && power_on_for_chargesplash) {
		power_on_for_chargesplash = false;
		/* TODO: Send signal to UI to continue boot */
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, handle_power_button_change,
	     HOOK_PRIO_FIRST);

static void handle_chipset_shutdown(void)
{
	power_on_for_chargesplash = false;
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, handle_chipset_shutdown, HOOK_PRIO_DEFAULT);

static int command_chargesplash(int argc, char **argv)
{
	if (argc != 2) {
		return EC_ERROR_PARAM_COUNT;
	}

	if (!strcasecmp(argv[1], "state")) {
		ccprints("power_on_for_chargesplash = %d",
			 power_on_for_chargesplash);
		ccprints("chargesplash_boot_tries = %d",
			 chargesplash_boot_tries);
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "boot")) {
		boot_for_chargesplash();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "cancel")) {
		boot_for_chargesplash_fail();
		return EC_SUCCESS;
	}

	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND(chargesplash, command_chargesplash,
			"[state|boot|cancel]", "Debug charging splash");
