/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host commands for USB-PD module.
 */

#include <string.h>

#include "battery.h"
#include "charge_manager.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_host_cmd.h"
#include "usb_pd_tcpm.h"

#ifdef CONFIG_COMMON_RUNTIME
struct ec_params_usb_pd_rw_hash_entry rw_hash_table[RW_HASH_ENTRIES];

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else /* CONFIG_COMMON_RUNTIME */
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif /* CONFIG_COMMON_RUNTIME */

/*
 * If we are trying to upgrade the TCPC port that is supplying power, then we
 * need to ensure that the battery has enough charge for the upgrade. 100mAh
 * is about 5% of most batteries, and it should be enough charge to get us
 * through the EC jump to RW and PD upgrade.
 */
#define MIN_BATTERY_FOR_TCPC_UPGRADE_MAH 100 /* mAH */

#ifdef HAS_TASK_HOSTCMD

static enum ec_status hc_pd_ports(struct host_cmd_handler_args *args)
{
	struct ec_response_usb_pd_ports *r = args->response;

	r->num_ports = board_get_usb_pd_port_count();
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_PORTS,
		     hc_pd_ports,
		     EC_VER_MASK(0));

#ifdef CONFIG_HOSTCMD_RWHASHPD
static enum ec_status
hc_remote_rw_hash_entry(struct host_cmd_handler_args *args)
{
	int i, idx = 0, found = 0;
	const struct ec_params_usb_pd_rw_hash_entry *p = args->params;
	static int rw_hash_next_idx;

	if (!p->dev_id)
		return EC_RES_INVALID_PARAM;

	for (i = 0; i < RW_HASH_ENTRIES; i++) {
		if (p->dev_id == rw_hash_table[i].dev_id) {
			idx = i;
			found = 1;
			break;
		}
	}

	if (!found) {
		idx = rw_hash_next_idx;
		rw_hash_next_idx = rw_hash_next_idx + 1;
		if (rw_hash_next_idx == RW_HASH_ENTRIES)
			rw_hash_next_idx = 0;
	}
	memcpy(&rw_hash_table[idx], p, sizeof(*p));

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_RW_HASH_ENTRY,
		     hc_remote_rw_hash_entry,
		     EC_VER_MASK(0));
#endif /* CONFIG_HOSTCMD_RWHASHPD */

#ifndef CONFIG_USB_PD_TCPC
#ifdef CONFIG_EC_CMD_PD_CHIP_INFO
static enum ec_status hc_remote_pd_chip_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_pd_chip_info *p = args->params;
	struct ec_response_pd_chip_info_v1 *info;

	if (p->port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	if (tcpm_get_chip_info(p->port, p->live, &info))
		return EC_RES_ERROR;

	/*
	 * Take advantage of the fact that v0 and v1 structs have the
	 * same layout for v0 data. (v1 just appends data)
	 */
	args->response_size =
		args->version ? sizeof(struct ec_response_pd_chip_info_v1)
			      : sizeof(struct ec_response_pd_chip_info);

	memcpy(args->response, info, args->response_size);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_CHIP_INFO,
		     hc_remote_pd_chip_info,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
#endif /* CONFIG_EC_CMD_PD_CHIP_INFO */
#endif /* CONFIG_USB_PD_TCPC */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static enum ec_status hc_remote_pd_set_amode(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_pd_set_mode_request *p = args->params;

	if ((p->port >= board_get_usb_pd_port_count()) ||
	    (!p->svid) || (!p->opos))
		return EC_RES_INVALID_PARAM;

	switch (p->cmd) {
	case PD_EXIT_MODE:
		if (pd_dfp_exit_mode(p->port, p->svid, p->opos))
			pd_send_vdm(p->port, p->svid,
				    CMD_EXIT_MODE | VDO_OPOS(p->opos), NULL, 0);
		else {
			CPRINTF("Failed exit mode\n");
			return EC_RES_ERROR;
		}
		break;
	case PD_ENTER_MODE:
		if (pd_dfp_enter_mode(p->port, p->svid, p->opos))
			pd_send_vdm(p->port, p->svid, CMD_ENTER_MODE |
				    VDO_OPOS(p->opos), NULL, 0);
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_SET_AMODE,
		     hc_remote_pd_set_amode,
		     EC_VER_MASK(0));
#endif /* CONFIG_USB_PD_ALT_MODE_DFP */

#ifdef CONFIG_COMMON_RUNTIME
static enum ec_status hc_remote_pd_dev_info(struct host_cmd_handler_args *args)
{
	const uint8_t *port = args->params;
	struct ec_params_usb_pd_rw_hash_entry *r = args->response;
	uint16_t dev_id;
	uint32_t current_image;

	if (*port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	pd_dev_get_rw_hash(*port, &dev_id, r->dev_rw_hash, &current_image);

	r->dev_id = dev_id;
	r->current_image = current_image;

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_DEV_INFO,
		     hc_remote_pd_dev_info,
		     EC_VER_MASK(0));

#ifdef CONFIG_CMD_PD_CONTROL
static enum ec_status pd_control(struct host_cmd_handler_args *args)
{
	static int pd_control_disabled[CONFIG_USB_PD_PORT_MAX_COUNT];
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
		/*
		 * The AP is requesting to suspend PD traffic on the EC so it
		 * can perform a firmware upgrade. If Vbus is present on the
		 * connector (it is either a source or sink), then we will
		 * prevent the upgrade if there is not enough battery to finish
		 * the upgrade. We cannot rely on the EC's active charger data
		 * as the EC just rebooted into RW and has not necessarily
		 * picked the active charger yet.
		 */
		if (IS_ENABLED(HAS_TASK_CHARGER) &&
			pd_is_vbus_present(cmd->chip)) {
			struct batt_params batt = { 0 };
			/*
			 * The charger task has not re-initialized, so we need
			 * to ask the battery directly.
			 */
			battery_get_params(&batt);
			if (batt.remaining_capacity <
				    MIN_BATTERY_FOR_TCPC_UPGRADE_MAH ||
			    batt.flags & BATT_FLAG_BAD_REMAINING_CAPACITY) {
				CPRINTS("C%d: Cannot suspend for upgrade, not "
					"enough battery (%dmAh)!",
					cmd->chip, batt.remaining_capacity);
				return EC_RES_BUSY;
			}
		}

		if (!IS_ENABLED(HAS_TASK_CHARGER) &&
			pd_is_vbus_present(cmd->chip)) {
			CPRINTS("C%d: Cannot suspend for upgrade, Vbus "
				"present!", cmd->chip);
			return EC_RES_BUSY;
		}
		enable = 0;
	} else if (cmd->subcmd == PD_RESUME) {
		enable = 1;
	} else if (cmd->subcmd == PD_RESET) {
		if (IS_ENABLED(HAS_TASK_PDCMD))
			board_reset_pd_mcu();
		else
			return EC_RES_INVALID_COMMAND;
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
#endif /* CONFIG_CMD_PD_CONTROL */

#ifdef CONFIG_HOSTCMD_FLASHPD
/* TCPMv2 does not have VDM states hence return VDM_STATE_DONE */
__overridable enum vdm_states pd_get_vdm_state(int port)
{
	return VDM_STATE_DONE;
}

static enum ec_status hc_remote_flash(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_pd_fw_update *p = args->params;
	int port = p->port;
	const uint32_t *data = &(p->size) + 1;
	int i, size, rv = EC_RES_SUCCESS;
	timestamp_t timeout;

	if (port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	if (p->size + sizeof(*p) > args->params_size)
		return EC_RES_INVALID_PARAM;

	/*
	 * Do not allow PD firmware update if no battery and this port
	 * is sinking power, because we will lose power.
	 */
#if defined(CONFIG_BATTERY) && \
	(defined(CONFIG_BATTERY_PRESENT_CUSTOM) ||   \
	 defined(CONFIG_BATTERY_PRESENT_GPIO))
	if (battery_is_present() != BP_YES &&
	    charge_manager_get_active_charge_port() == port)
		return EC_RES_UNAVAILABLE;
#endif

	/*
	 * Busy still with a VDM that host likely generated.  1 deep VDM queue
	 * so just return for retry logic on host side to deal with.
	 */
	if (pd_get_vdm_state(port) > VDM_STATE_DONE)
		return EC_RES_BUSY;

	switch (p->cmd) {
	case USB_PD_FW_REBOOT:
		pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_REBOOT, NULL, 0);

		/*
		 * Return immediately to free pending i2c bus.	Host needs to
		 * manage this delay.
		 */
		return EC_RES_SUCCESS;

	case USB_PD_FW_FLASH_ERASE:
		pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_FLASH_ERASE, NULL, 0);

		/*
		 * Return immediately.	Host needs to manage delays here which
		 * can be as long as 1.2 seconds on 64KB RW flash.
		 */
		return EC_RES_SUCCESS;

	case USB_PD_FW_ERASE_SIG:
		pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_ERASE_SIG, NULL, 0);
		timeout.val = get_time().val + 500*MSEC;
		break;

	case USB_PD_FW_FLASH_WRITE:
		/* Data size must be a multiple of 4 */
		if (!p->size || p->size % 4)
			return EC_RES_INVALID_PARAM;

		size = p->size / 4;
		for (i = 0; i < size; i += VDO_MAX_SIZE - 1) {
			pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_FLASH_WRITE,
				    data + i, MIN(size - i, VDO_MAX_SIZE - 1));
			timeout.val = get_time().val + 500*MSEC;

			/* Wait until VDM is done */
			while ((pd_get_vdm_state(port) > VDM_STATE_DONE) &&
			       (get_time().val < timeout.val))
				task_wait_event(10*MSEC);

			if (pd_get_vdm_state(port) > VDM_STATE_DONE)
				return EC_RES_TIMEOUT;
		}
		return EC_RES_SUCCESS;

	default:
		return EC_RES_INVALID_PARAM;
	}

	/* Wait until VDM is done or timeout */
	while (pd_get_vdm_state(port) > VDM_STATE_DONE &&
		get_time().val < timeout.val)
		task_wait_event(50*MSEC);

	if (pd_get_vdm_state(port) > VDM_STATE_DONE ||
	    pd_get_vdm_state(port) == VDM_STATE_ERR_TMOUT)
		rv = EC_RES_TIMEOUT;
	else if (pd_get_vdm_state(port) < VDM_STATE_DONE)
		rv = EC_RES_ERROR;

	return rv;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_FW_UPDATE,
		     hc_remote_flash,
		     EC_VER_MASK(0));
#endif /* CONFIG_HOSTCMD_FLASHPD */
#endif /* CONFIG_COMMON_RUNTIME */

#endif /* HAS_TASK_HOSTCMD */
