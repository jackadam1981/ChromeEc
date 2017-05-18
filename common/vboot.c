/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implementation of EC's boot verification
 */

#include "battery.h"
#include "charge_manager.h"
#include "chipset.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "rwsig.h"
#include "system.h"
#include "usb_pd.h"
#include "vboot.h"

#define CPRINTS(format, args...) cprints(CC_VBOOT, format, ## args)

static int has_matrix_keyboard(void)
{
	return 0;
}

static int is_vboot_ec_supported(void)
{
	return 0;
}

static int is_low_power_ap_boot_supported(void)
{
	return 0;
}

static int verify_slot(int slot)
{
	/* TODO: Handle slot A and B */
	CPRINTS("Verifying S%d", slot);
	return rwsig_check_signature();
}

static int verify_rw(void)
{
	/* 1. Read BBRAM to decide which slot to verify */
	/* 2. Verify the slot */
	return verify_slot(0);
}

/* Request more power: charging battery or more powerful AC adapter */
static void request_power(void)
{
	/* TODO: Blink LED */
	chipset_force_shutdown();
}

static void request_recovery(void)
{
	/* TODO: Blink LED */
	while (1)
		;
}

static int is_manual_recovery(void)
{
	return host_get_events() & EC_HOST_EVENT_KEYBOARD_RECOVERY;
}

void vboot_ec(void)
{
	int port;

	if (system_is_powered_to_boot_ap())
		/* AC or battery or both are good. Proceed to boot. */
		return;

	CPRINTS("Not enough power to boot AP: %dmW",
		charge_manager_get_power_limit_uw() / 1000);

	/* Charge suppliers must be initialized already. TODO: Verify.
	 * Alternatively, we can detect power supplier directly. */
	port = charge_manager_get_active_charge_port();
	if (port >= CONFIG_USB_PD_PORT_COUNT)
		/* AC is not type-c. No chance to boot. */
		request_power();

	if (pd_comm_is_enabled_at_start())
		/* Normal RW boot or unlocked RO boot.
		 * Hoping enough power will be supplied after PD negotiation.
		 * There will be another call to system_is_powered_to_boot_ap
		 * when turning on AP. */
		return;

	/* PD communication is disabled. Probably this is RO image */
	CPRINTS("PD comm disabled");

	if (is_manual_recovery()) {
		if (battery_is_present() || has_matrix_keyboard())
			/* Shutdown for charging battery */
			request_power(); /* No return */
		CPRINTS("Enable C%d PD communication", port);
		/* TODO: Ensure this won't be overwritten by pd_init_tasks */
		pd_comm_enable(port, 1);
		return;
	}

	if (!is_vboot_ec_supported() && !is_low_power_ap_boot_supported())
		request_power();

	if (verify_rw())
		/* Jump (and reboot) */
		rwsig_jump_now();

	/* Failed to verify RW. Need recovery. */
	request_recovery();	/* No return */
}

/*
 * This makes vboot run after chargers are initialized but before chipset task
 * starts.
 *
 * TODO: Evaluate the boot speed impact.
 *
 * The followings have to work before we can vboot_ec()
 *
 * 1. ADC
 * ADC is used to read the voltage of power supply. ADC init is called in
 * HOOK_INIT. adc_read_channel waits for interrupt.
 *
 * 2. charge_manager
 * charge_manager knows how much power is supplied. It updates current and
 * voltage using deferred calls. This is for allowing PD and USB tasks to
 * asynchronously update new available sources.
 *
 * 3. flash
 * PD communication is locked or unlocked depending on the status register
 * of the flash chip.
 */
DECLARE_HOOK(HOOK_INIT, vboot_ec, HOOK_PRIO_LAST);
