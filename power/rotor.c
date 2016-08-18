/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rotor power sequencing module for Chrome EC. */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "host_command.h"
#include "ipc.h"
#include "power.h"
#include "power_button.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define IN_S0 0
#define IN_S3 1
#define IN_S5 2
static uint8_t pwr_state_msg_map;

static void send_system_power_status(void);

/**
 * Get current system power status.
 *
 * This function will report what kind of boot is possible with the current
 * given system power.
 *
 * @return What kind of boot is possible.
 */
static enum sys_power_status get_system_power_status(void)
{
	/*
	 * TOOD(aaboagye): Might be a good idea to create macros for the power
	 * requirements.
	 */
	int charger_pwr;
	enum sys_power_status power_status;

	if (charge_get_percent() > BATTERY_LEVEL_SHUTDOWN) {
		/* We're okay if we have enough battery charge. */
		power_status = SYS_PWR_FULL;
	} else if (extpower_is_present()) {
		/*
		 * If a charger is present, see what kind of power it can
		 * provide.
		 */
		charger_pwr = charge_manager_get_power_limit_uw();
		if (charger_pwr < 1000000) {
			power_status = SYS_PWR_INSUFFICIENT;
		} else if (charger_pwr >= 1000000 && charger_pwr < 3500000) {
			/* Restricted boot requires 1W+. */
			power_status = SYS_PWR_RESTRICTED;
		} else if (charger_pwr >= 3500000) {
			/* Full boot requires 3.5W+. */
			power_status = SYS_PWR_FULL;
		}
	} else {
		/*
		 * If AC isn't present and we have such a low charge, it's
		 * insufficient.
		 */
		power_status = SYS_PWR_INSUFFICIENT;
	}

	switch (power_status) {
	case SYS_PWR_FULL:
		CPRINTS("PS: FULL");
		break;

	case SYS_PWR_INSUFFICIENT:
		CPRINTS("PS: INSUFFICIENT");
		break;

	case SYS_PWR_RESTRICTED:
		CPRINTS("PS: RESTRICTED");
		break;

	default:
		break;
	}
	return power_status;
}

/**
 * Send system power status to SP.
 */
static void send_system_power_status(void)
{
	int csum, i;
	uint32_t *buf;
	struct ec_host_request *r;
	uint8_t *p, *out;
	struct ec_params_sys_power_status params;

	params.power_status = (uint8_t)get_system_power_status();

	/*
	 * The SP will be expecting this message as a host command.  So prepare
	 * one to be sent.
	 */
	buf = get_ipc_buffer(ROTOR_MCU_SP_IPC);
	r = (struct ec_host_request *)buf;
	memset(buf, 0, ROTOR_MCU_IPC_BUF_LEN);

	/* Fill out the "host command request". */
	r->struct_version = 3;
	r->checksum = 0;
	r->command = EC_CMD_SYS_POWER_STATUS;
	r->command_version = 0;
	r->data_len = sizeof(struct ec_params_sys_power_status);

	/* Tack on the power status. */
	p = (uint8_t *)buf + sizeof(struct ec_host_request);
	memcpy(p, &params, sizeof(params));

	/* Compute checksum */
	out = (uint8_t *)buf;
	for (i = sizeof(struct ec_host_request) +
		     sizeof(struct ec_params_sys_power_status);
	     i > 0;
	     i--)
		csum += *out++;

	/* Fill in the checksum now. */
	r->checksum = (uint8_t)(-csum);

	/* Now that the message is ready, send it to the SP. */
	send_message(ROTOR_MCU_SP_IPC, buf, 0);
}
DECLARE_DEFERRED(send_system_power_status);

/**
 * Send a message to the APMU that we wish to directly transition to S5 SOF.
 */
void send_sys_power_down_req(void)
{
	int csum, i;
	uint32_t *buf;
	struct ec_host_request *r;
	uint8_t *out;

	/*
	 * The APMU will be expecting this message as a host command.  So
	 * prepare one to be sent.
	 */
	buf = get_ipc_buffer(ROTOR_MCU_APMU_IPC);
	r = (struct ec_host_request *)buf;
	memset(buf, 0, ROTOR_MCU_IPC_BUF_LEN);

	/* Fill out the "host command request". */
	r->struct_version = 3;
	r->checksum = 0;
	r->command = EC_CMD_SYS_POWER_DOWN_REQUEST;
	r->command_version = 0;
	r->data_len = 0;

	/* Compute checksum */
	out = (uint8_t *)buf;
	for (i = sizeof(struct ec_host_request); i > 0; i--)
		csum += *out++;

	/* Fill in the checksum now. */
	r->checksum = (uint8_t)(-csum);

	/* Now that the message is ready, send it to the SP. */
	send_message(ROTOR_MCU_APMU_IPC, buf, 0);
}
DECLARE_DEFERRED(send_sys_power_down_req);

static int command_sys_power_down_req(int argc, char **argv)
{
	/* Issue the message if in S3. */
	if (chipset_in_state(CHIPSET_STATE_SUSPEND))
		send_sys_power_down_req();
	else
		ccprintf("Can't issue request because system is not in S3!\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pwrdwnreq, command_sys_power_down_req, NULL,
			"Send msg to APMU to power down", NULL);

/**
 * Assert/Deassert AP reset.
 *
 * @param assert	0 deasserts AP reset, non-zero asserts AP reset.
 */
static void set_ap_reset(int assert)
{
	if (assert) {
		ROTOR_MCU_RESETAP &= (~ROTOR_MCU_AP_NRESET & 0x3);
		CPRINTS("AP held in reset.");
	} else {
		/* Allow HW BIST to complete. */
		ROTOR_MCU_EFUSE_PWR_CTL &= ~(1 << 0);
		ROTOR_MCU_M4_BIST_CLKCFG |= (1 << 1);
		/* Release AP from reset. */
		ROTOR_MCU_RESETAP |= (ROTOR_MCU_AP_NRESET & 0x3);
		CPRINTS("AP reset released.");
	}
}

enum power_state power_chipset_init(void)
{
	/*
	 * Technically, I think the MCU doesn't actually start running until S5
	 * Start of Day.
	 */
	return POWER_G3S5_STOD;
}

enum power_state power_handle_state(enum power_state state)
{
	switch (state) {
	case POWER_G3:
		/* Would need to look for power on events here. */
		return POWER_G3S5_STOD;

	case POWER_G3S5_STOD: /* t0a */
	case POWER_G3S5:
		hook_call_deferred(&send_system_power_status_data, 0);
		return POWER_S5_STOD;

	case POWER_S5_RS0: /* t7 */
	case POWER_S5_STODS0: /* t1 */
		hook_notify(HOOK_CHIPSET_STARTUP);
		return POWER_S0;

	case POWER_S5_SOS5_SOF: /* t6b */
		/* Change ANA_GRP to D0 */
		/* Change MCU clock sources to 25MHz clock. */
		/* Maybe hook_notify(HOOK_FREQ_CHANGE); */
		return POWER_S5_SOF;

	case POWER_S5_SOFS5_SO: /* t6a */
		/* Change MCU clock sources to 32KHz clock */
		/* Change ANA_GRP to D2 */
		/* hook_notify(HOOK_FREQ_CHANGE) ? */
		return POWER_S5_SO;

	case POWER_S5_SOFS5_APR: /* t6c */
		/* Assert AP reset */
		set_ap_reset(1);
		/* Power on AP */
#ifdef CONFIG_BRINGUP
		CPRINTS("Would power on AP.");
#endif /* defined(CONFIG_BRINGUP) */
		return POWER_S5_APR;

	case POWER_S5_APRS5_SOF: /* t6d */
		/* Power off AP */
		/* De-assert AP reset */
#ifdef CONFIG_BRINGUP
		CPRINTS("Would power off AP.");
#endif /* defined(CONFIG_BRINGUP) */
		/* De-assert AP reset */
		set_ap_reset(0);
		return POWER_S5_SOF;

	case POWER_S5_APRS5_R: /* t6 */
		hook_call_deferred(&send_system_power_status_data, 0);
		/* Disable AP isolation */
		ROTOR_MCU_AP_ISOLATE = 1;
		/* De-assert AP reset */
		set_ap_reset(0);
		/* TODO(aaboagye): Remove this hack when issue is identified. */
		ROTOR_MCU_RESETAP |= (1 << 1);
		return POWER_S5_R;

	/* To the "G3" state */
	case POWER_S5_STODG3: /* t0b */
	case POWER_S5_SOFG3: /* t6e */
	case POWER_S5_RG3: /* t7a */
	case POWER_S5G3:
		return POWER_G3;

	case POWER_S5:
		return POWER_S5;

	case POWER_S5_R:
		if (pwr_state_msg_map == (1 << IN_S0)) {
			pwr_state_msg_map = 0;
			return POWER_S5_RS0; /* t7 */
		}
		/* Wait for a message to enter S0. */
		power_wait_signals(0);
		task_wait_event(-1);
		return POWER_S5_R;

	case POWER_S5_STOD:
		if (pwr_state_msg_map == (1 << IN_S0)) {
			pwr_state_msg_map = 0;
			return POWER_S5_STODS0;
		}
		/* Wait for a message. */
		power_wait_signals(0);
		task_wait_event(-1);
		return POWER_S5_STOD;

	case POWER_S5_APR:
		return POWER_S5_APRS5_R;

	case POWER_S5_SOF:
		if (power_button_is_pressed())
			return POWER_S5_SOFS5_APR;
		/* wait */
		power_wait_signals(0);
		task_wait_event(-1);
		return POWER_S5_SOF;

	case POWER_S5_SO:
		/*
		 * TODO(aaboagye): Determine when to transition back up to SOF.
		 */
		return POWER_S5_SO;

	case POWER_S3:
		/* Check for wake sources */
		if (pwr_state_msg_map == (1 << IN_S0)) {
			pwr_state_msg_map = 0;
			return POWER_S3S0;
		} else if (pwr_state_msg_map == (1 << IN_S5)) {
			pwr_state_msg_map = 0;
			return POWER_S3S5_SOF;
		}
		return POWER_S3;

	case POWER_S0:
		/* Check for suspend sources */
		if (pwr_state_msg_map == (1 << IN_S3)) {
			pwr_state_msg_map = 0;
			return POWER_S0S3;
		} else if (pwr_state_msg_map == (1 << IN_S5)) {
			pwr_state_msg_map = 0;
			return POWER_S0S5_SOF;
		}
		return POWER_S0;

	case POWER_S0S5_SOF: /* t4 */
	case POWER_S3S5_SOF: /* t5 */
	case POWER_S3S5:
		hook_notify(HOOK_CHIPSET_SHUTDOWN);
		/* Set AP Reset */
		set_ap_reset(1);
		/* Enable AP isolation */
		ROTOR_MCU_AP_ISOLATE = 0;
		/* Power off AP */
		/* Change unneeded devices to D2 */
		CPRINTS("Would power off AP and change uneeded devices to D2.");
		return POWER_S5_SOF;

	case POWER_S5S3:
		/* not a valid path for this chipset. */

	case POWER_S3S0:
		hook_notify(HOOK_CHIPSET_RESUME);
		return POWER_S0;

	case POWER_S0S3:
		hook_notify(HOOK_CHIPSET_SUSPEND);
		return POWER_S3;
	}


	return state;
}

static void powerbtn_rotor_changed(void)
{
	task_wake(TASK_ID_CHIPSET);
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, powerbtn_rotor_changed,
	     HOOK_PRIO_DEFAULT);

static int host_command_sys_power_state_change(struct host_cmd_handler_args
					       *args)
{
	/* incoming notification about chipset power state changes. */
	const struct ec_params_sys_power_change *p = args->params;

	switch (p->state_change) {
	case SYS_ENTERING_S0:
		CPRINTS("System entering S0 msg received.");
		pwr_state_msg_map = (1 << IN_S0);
		break;

	case SYS_ENTERING_S3:
		CPRINTS("System entering S3 msg received.");
		pwr_state_msg_map = (1 << IN_S3);
		break;

	case SYS_ENTERING_S5: /* S5-MCU SO */
		CPRINTS("System entering S5 SO msg received.");
		pwr_state_msg_map = (1 << IN_S5);
		break;

	default:
		break;
	};

	/* Wake up the chipset task to notify the change. */
	task_wake(TASK_ID_CHIPSET);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_SYS_POWER_STATE_CHANGE,
		     host_command_sys_power_state_change,
		     EC_VER_MASK(0));

void chipset_force_shutdown(void)
{
	/*
	 * TODO(aaboagye): implement this.  Probably turn off AP, try and get
	 * the PMU to turn everything else off?
	 */
}

void chipset_reset(int cold_reset)
{
}

static int host_command_shutdown(struct host_cmd_handler_args *args)
{
	hook_notify(HOOK_CHIPSET_SHUTDOWN);

	/* Disable interrupts. */
	interrupt_disable();

	/*
	 * Probably need to abort any i2c transactions and leave the lines back
	 * to idle.
	 */

	/*
	 * Clear the interrupt bit to show that we have acknowledged the
	 * shutdown request.
	 */
	ROTOR_MCU_IPC_ICR(ROTOR_MCU_SP_IPC) = (1 << 0);

	CPRINTS("Would try and shutdown EC.");

	/* Time for the long sleep... */
	while (1)
		asm("wfi");

	/* We shouldn't get here... */
	return EC_ERROR_UNKNOWN;
}
DECLARE_HOST_COMMAND(EC_CMD_SHUTDOWN, host_command_shutdown, EC_VER_MASK(0));
