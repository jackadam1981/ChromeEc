/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>
#include <string.h>

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "power_button.h"
#include "util.h"

#define CPRINTS(format, args...) \
	cprints(CC_USBCHARGE, "chargesplash: " format, ##args)

/*
 * Was this power on initiated to show a charge splash?
 *
 * - Set when powering on for an AC connect.
 * - Unset when power button is pushed, or the chargesplash request is
 *   cancelled due to AC disconnection.
 */
static bool power_on_for_chargesplash;

/*
 * We maintain a counter of the number of times we have failed to boot
 * to the chargesplash before the AC was disconnected.  When this
 * counter increments too high, it could indicate adapter or cable
 * damage (disconnecting frequently).  If we get many repeated we
 * lockout the chargesplash feature to prevent power waste.  A
 * successful boot after a lockout will clear the condition.
 */
static int chargesplash_boot_tries;

/* True once the display has come up */
static bool display_initialized;

/* Manually reset state (via host or UART cmd) */
static void chargesplash_reset_state(void)
{
	power_on_for_chargesplash = false;
	chargesplash_boot_tries = 0;
	display_initialized = false;
}

/* Manually trigger a lockout (via host or UART cmd) */
static void chargesplash_lockout(void)
{
	chargesplash_boot_tries = CONFIG_CHARGESPLASH_BOOT_MAX_TRIES;
}

static void boot_for_chargesplash(void)
{
	if (chargesplash_boot_tries >= CONFIG_CHARGESPLASH_BOOT_MAX_TRIES) {
		CPRINTS("Lockout (max tries exceeded)");
		return;
	}

	CPRINTS("Power on for charge display");
	power_on_for_chargesplash = true;
	display_initialized = false;
	chipset_power_on();
}

static void init_display(void)
{
	/*
	 * TODO(b/228370390): Consider asserting PROCHOT (on
	 * some platforms) to slow down background boot.
	 */

	CPRINTS("Display initialized");
	display_initialized = true;
}

static void handle_ac_change(void)
{
	if (extpower_is_present()) {
		/* AC connect event */
		if (!lid_is_open()) {
			CPRINTS("Ignore AC connect as lid is closed");
			return;
		}

		if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
			boot_for_chargesplash();
		}
	} else {
		/* AC disconnect event */
		if (power_on_for_chargesplash && !display_initialized) {
			CPRINTS("Shutdown due to disconnected charger");
			chipset_force_shutdown(
				CHIPSET_SHUTDOWN_CHARGESPLASH_CANCEL);
			power_on_for_chargesplash = false;
			chargesplash_boot_tries++;
		}
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, handle_ac_change, HOOK_PRIO_LAST - 1);

static void handle_power_button_change(void)
{
	if (power_button_is_pressed() && power_on_for_chargesplash) {
		CPRINTS("Cancel due to power button press");
		power_on_for_chargesplash = false;
		display_initialized = false;
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, handle_power_button_change,
	     HOOK_PRIO_FIRST);

static void handle_chipset_shutdown(void)
{
	power_on_for_chargesplash = false;
	display_initialized = false;
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
		ccprints("display_initialized = %d", display_initialized);
		ccprints("chargesplash_boot_tries = %d",
			 chargesplash_boot_tries);
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "reset")) {
		chargesplash_reset_state();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "lockout")) {
		chargesplash_lockout();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "boot")) {
		boot_for_chargesplash();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "cancel")) {
		power_on_for_chargesplash = false;
		return EC_SUCCESS;
	}

	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND(chargesplash, command_chargesplash,
			"[state|reset|lockout|boot|cancel]",
			"Charge splash controls");

static enum ec_status chargesplash_host_cmd(struct host_cmd_handler_args *args)
{
	const struct ec_params_chargesplash *params = args->params;
	struct ec_response_chargesplash *response = args->response;

	if (args->params_size < sizeof(*params)) {
		return EC_RES_INVALID_PARAM;
	}

	if (args->response_max < sizeof(*response)) {
		return EC_RES_INVALID_RESPONSE;
	}

	switch (params->cmd) {
	case EC_CHARGESPLASH_GET_STATE:
		/* No action to do */
		break;
	case EC_CHARGESPLASH_INIT_DISPLAY:
		/* Successful boot clears a lockout */
		chargesplash_boot_tries = 0;

		if (power_on_for_chargesplash) {
			init_display();
		}
		break;
	case EC_CHARGESPLASH_REQUEST:
		power_on_for_chargesplash = true;
		break;
	case EC_CHARGESPLASH_RESET:
		chargesplash_reset_state();
		break;
	case EC_CHARGESPLASH_LOCKOUT:
		chargesplash_lockout();
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	/* All commands return the (possibly updated) state */
	response->requested = power_on_for_chargesplash;
	response->display_initialized = display_initialized;
	response->tries = chargesplash_boot_tries;
	response->max_tries = CONFIG_CHARGESPLASH_BOOT_MAX_TRIES;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CHARGESPLASH, chargesplash_host_cmd,
		     EC_VER_MASK(0));
