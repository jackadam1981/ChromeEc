/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host commands shared across multiple USB-PD implementations
 */

#include "atomic.h"
#include "battery.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dpm_sm.h"

#include <string.h>

#if !defined(CONFIG_USB_PD_TCPM_STUB)
/*
 * PD host event status for host command
 * Note: this variable must be aligned on 4-byte boundary because we pass the
 * address to atomic_ functions which use assembly to access them.
 */
static atomic_t pd_host_event_status __aligned(4);

test_mockable void pd_send_host_event(int mask)
{
	/* mask must be set */
	if (!mask)
		return;

	atomic_or(&pd_host_event_status, mask);
	/* interrupt the AP */
	host_set_single_event(EC_HOST_EVENT_PD_MCU);
}

static enum ec_status
hc_pd_host_event_status(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_status *r = args->response;

	/* Read and clear the host event status to return to AP */
	r->status = atomic_clear(&pd_host_event_status);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_HOST_EVENT_STATUS, hc_pd_host_event_status,
		     EC_VER_MASK(0));
#endif /* ! CONFIG_USB_PD_TCPM_STUB */

/*
 * If we are trying to upgrade PD firmwares (TCPC chips, retimer, etc), we
 * need to ensure the battery has enough charge for this process. Set the
 * threshold to 10%, and it should be enough charge to get us
 * through the EC jump to RW and PD upgrade.
 */
#define MIN_BATTERY_FOR_PD_UPGRADE_PERCENT 10 /* % */

bool pd_firmware_upgrade_check_power_readiness(int port)
{
	if (IS_ENABLED(HAS_TASK_CHARGER)) {
		struct batt_params batt = { 0 };
		/*
		 * Cannot rely on the EC's active charger data as the
		 * EC may just rebooted into RW and has not necessarily
		 * picked the active charger yet. Charger task may not
		 * initialized, so check battery directly.
		 * Prevent the upgrade if the battery doesn't have enough
		 * charge to finish the upgrade.
		 */
		battery_get_params(&batt);
		if (batt.flags & BATT_FLAG_BAD_STATE_OF_CHARGE ||
		    batt.state_of_charge < MIN_BATTERY_FOR_PD_UPGRADE_PERCENT) {
			cprintf(CC_USBPD,
				"C%d: Cannot suspend for upgrade, not "
				"enough battery (%d%%)!",
				port, batt.state_of_charge);
			return false;
		}
	} else {
		/* VBUS is present on the port (it is either a
		 * source or sink) to provide power, so don't allow
		 * PD firmware upgrade on the port.
		 */
		if (pd_is_vbus_present(port))
			return false;
	}

	return true;
}

#ifdef CONFIG_HOSTCMD_PD_CONTROL
static int pd_control_disabled[CONFIG_USB_PD_PORT_MAX_COUNT];

/* Only allow port re-enable in unit tests */
#ifdef TEST_BUILD
void pd_control_port_enable(int port)
{
	pd_control_disabled[port] = 0;
}
#endif /* TEST_BUILD */

static enum ec_status pd_control(struct host_cmd_handler_args *args)
{
	const struct ec_params_pd_control *cmd = args->params;
	int enable = 0;

	if (cmd->chip >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	/* Always allow disable command */
	if (cmd->subcmd == PD_CONTROL_DISABLE) {
		pd_control_disabled[cmd->chip] = 1;
		return EC_RES_SUCCESS;
	}

	if (pd_control_disabled[cmd->chip])
		return EC_RES_ACCESS_DENIED;

	if (cmd->subcmd == PD_SUSPEND) {
		if (!pd_firmware_upgrade_check_power_readiness(cmd->chip))
			return EC_RES_BUSY;
		enable = 0;
	} else if (cmd->subcmd == PD_RESUME) {
		enable = 1;
	} else if (cmd->subcmd == PD_RESET) {
		board_reset_pd_mcu();
	} else if (cmd->subcmd == PD_CHIP_ON && board_set_tcpc_power_mode) {
		board_set_tcpc_power_mode(cmd->chip, 1);
		return EC_RES_SUCCESS;
	} else {
		return EC_RES_INVALID_COMMAND;
	}

	pd_comm_enable(cmd->chip, enable);
	pd_set_suspend(cmd->chip, !enable);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_CONTROL, pd_control, EC_VER_MASK(0));
#endif /* CONFIG_HOSTCMD_PD_CONTROL */
