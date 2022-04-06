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
#include "lid_switch.h"
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
 * counter increments too high, it could indicate adapter or cable
 * damage (disconnecting frequently).  If we get many repeated we
 * lockout the chargesplash feature to prevent power waste.  The
 * only way to re-enable the chargesplash feature after a lockout is
 * an EC reset.
 */
static int chargesplash_boot_tries;

void chargesplash_reset(void)
{
	power_on_for_chargesplash = false;
	chargesplash_boot_tries = 0;
}

static void boot_for_chargesplash_fail(void)
{
	CPRINTS("Cancel chargesplash request");
	chipset_force_shutdown(CHIPSET_SHUTDOWN_CHARGESPLASH_CANCEL);
	power_on_for_chargesplash = false;
	chargesplash_boot_tries++;
}

static void boot_for_chargesplash(void)
{
	if (chargesplash_boot_tries >= CONFIG_CHARGESPLASH_BOOT_MAX_TRIES) {
		CPRINTS("Lockout chargesplash (max tries exceeded)");
		return;
	}

	CPRINTS("Power on for chargesplash");
	power_on_for_chargesplash = true;
	chipset_power_on();
}

static void handle_ac_change(void)
{
	CPRINTS(__func__);
	if (extpower_is_present()) {
		/* AC connect event */
		CPRINTS("CS: ac connect");

		if (!lid_is_open()) {
			CPRINTS("Ignore chargesplash request as lid is closed");
			return;
		}

		if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
			CPRINTS("CS: chipset is off");
			boot_for_chargesplash();
		}

		/* TODO: Handle wake condition when we're in suspend */
	} else {
		/* AC disconnect event */
		CPRINTS("CS: ac disconnect");

		if (power_on_for_chargesplash) {
			boot_for_chargesplash_fail();
		}
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, handle_ac_change, HOOK_PRIO_LAST - 1);

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

	if (!strcasecmp(argv[1], "reset")) {
		chargesplash_reset();
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
			"[state|reset|boot|cancel]", "Debug charging splash");
