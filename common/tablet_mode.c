/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "acpi.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_angle.h"
#include "stdbool.h"
#include "tablet_mode.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_LID, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_LID, format, ## args)

/*
 * Other code modules assume that notebook mode (i.e. tablet_mode = false) at
 * startup
 */
static bool tablet_mode;

/*
 * Console command can force the value of tablet_mode. If tablet_mode_force is
 * true, the all external set call for tablet_mode are ignored.
 */
static bool tablet_mode_forced;

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
=======
/* True if GMR sensor is reporting 360 degrees. */
static bool gmr_sensor_at_360;

>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
/*
 * True: all calls to tablet_set_mode are ignored and tablet_mode if forced to 0
 * False: all calls to tablet_set_mode are honored
 */
static bool disabled;

int tablet_get_mode(void)
{
	return tablet_mode;
}

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
static void notify_tablet_mode_change(void)
{
	CPRINTS("tablet mode %sabled", tablet_mode ? "en" : "dis");
=======
static inline void print_tablet_mode(void)
{
	CPRINTS("tablet mode %sabled", tablet_mode ? "en" : "dis");
}

static void notify_tablet_mode_change(void)
{
	print_tablet_mode();
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
	hook_notify(HOOK_TABLET_MODE_CHANGE);

	/*
	 * When tablet mode changes, send an event to ACPI to retrieve
	 * tablet mode value and send an event to the kernel.
	 */
	if (IS_ENABLED(CONFIG_HOSTCMD_EVENTS))
		host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);

}

void tablet_set_mode(int mode)
{
	/* If tablet_mode is forced via a console command, ignore set. */
	if (tablet_mode_forced)
		return;

	if (tablet_mode == !!mode)
		return;

	if (disabled) {
		CPRINTS("Tablet mode set while disabled (ignoring)!");
		return;
	}

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
	tablet_mode = !!mode;

	hook_notify(HOOK_TABLET_MODE_CHANGE);
=======
	if (gmr_sensor_at_360 && !mode) {
		CPRINTS("Ignoring tablet mode exit while gmr sensor "
			"reports 360-degree tablet mode.");
		return;
	}

	tablet_mode = !!mode;

	notify_tablet_mode_change();
}

void tablet_disable(void)
{
	tablet_mode = false;
	disabled = true;
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
}

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
void tablet_disable(void)
=======
/* This ifdef can be removed once we clean up past projects which do own init */
#ifdef CONFIG_GMR_TABLET_MODE
#ifndef GMR_TABLET_MODE_GPIO_L
#error  GMR_TABLET_MODE_GPIO_L must be defined
#endif
#ifdef CONFIG_DPTF_MOTION_LID_NO_GMR_SENSOR
#error The board has GMR sensor
#endif
static void gmr_tablet_switch_interrupt_debounce(void)
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
{
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
	tablet_mode = false;
	disabled = true;
}

/* This ifdef can be removed once we clean up past projects which do own init */
#ifdef CONFIG_HALL_SENSOR
#ifndef HALL_SENSOR_GPIO_L
#error  HALL_SENSOR_GPIO_L must be defined
#endif
static void hall_sensor_interrupt_debounce(void)
{
	int flipped_360_mode = !gpio_get_level(HALL_SENSOR_GPIO_L);

	/*
	 * 1. Peripherals are disabled only when lid reaches 360 position (It's
	 * probably already disabled by motion_sense task). We deliberately do
	 * not enable peripherals when the lid is leaving 360 position. Instead,
	 * we let motion sense task enable it once it is reaches laptop zone
	 * (180 or less).
	 * 2. Similarly, tablet mode is set here when lid reaches 360
	 * position. It should already be set by motion lid driver. We
	 * deliberately do not clear tablet mode when lid is leaving 360
	 * position(if motion lid driver is used). Instead, we let motion lid
	 * driver to clear it when lid goes into laptop zone.
	 */

#ifdef CONFIG_LID_ANGLE
	if (flipped_360_mode)
#endif /* CONFIG_LID_ANGLE */
		tablet_set_mode(flipped_360_mode);
=======
	gmr_sensor_at_360 = IS_ENABLED(CONFIG_GMR_TABLET_MODE_CUSTOM)
				     ? board_sensor_at_360()
				     : !gpio_get_level(GMR_TABLET_MODE_GPIO_L);
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
#ifdef CONFIG_LID_ANGLE_UPDATE
	if (flipped_360_mode)
=======
	/*
	 * DPTF table is updated only when the board enters/exits completely
	 * flipped tablet mode. If the board has no GMR sensor, we determine
	 * if the board is in completely-flipped tablet mode by lid angle
	 * calculation and update DPTF table when lid angle > 300 degrees.
	 */
	if (IS_ENABLED(CONFIG_HOSTCMD_X86) && IS_ENABLED(CONFIG_DPTF)) {
		acpi_dptf_set_profile_num(gmr_sensor_at_360 ?
					  DPTF_PROFILE_FLIPPED_360_MODE :
					  DPTF_PROFILE_CLAMSHELL);
	}
	/*
	 * 1. Peripherals are disabled only when lid reaches 360 position (It's
	 * probably already disabled by motion_sense task). We deliberately do
	 * not enable peripherals when the lid is leaving 360 position. Instead,
	 * we let motion sense task enable it once it is reaches laptop zone
	 * (180 or less).
	 * 2. Similarly, tablet mode is set here when lid reaches 360
	 * position. It should already be set by motion lid driver. We
	 * deliberately do not clear tablet mode when lid is leaving 360
	 * position(if motion lid driver is used). Instead, we let motion lid
	 * driver to clear it when lid goes into laptop zone.
	 */

	if (!IS_ENABLED(CONFIG_LID_ANGLE) || gmr_sensor_at_360)
		tablet_set_mode(gmr_sensor_at_360);

	if (IS_ENABLED(CONFIG_LID_ANGLE_UPDATE) && gmr_sensor_at_360)
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
		lid_angle_peripheral_enable(0);
}
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
DECLARE_DEFERRED(hall_sensor_interrupt_debounce);
=======
DECLARE_DEFERRED(gmr_tablet_switch_interrupt_debounce);
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
/* Debounce time for hall sensor interrupt */
#define HALL_SENSOR_DEBOUNCE_US    (30 * MSEC)
=======
/* Debounce time for gmr sensor tablet mode interrupt */
#define GMR_SENSOR_DEBOUNCE_US    (30 * MSEC)
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
void hall_sensor_isr(enum gpio_signal signal)
=======
void gmr_tablet_switch_isr(enum gpio_signal signal)
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
{
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
	hook_call_deferred(&hall_sensor_interrupt_debounce_data,
				HALL_SENSOR_DEBOUNCE_US);
=======
	hook_call_deferred(&gmr_tablet_switch_interrupt_debounce_data,
			   GMR_SENSOR_DEBOUNCE_US);
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
}

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
static void hall_sensor_init(void)
=======
static void gmr_tablet_switch_init(void)
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
{
	/* If this sub-system was disabled before initializing, honor that. */
	if (disabled)
		return;

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
	gpio_enable_interrupt(HALL_SENSOR_GPIO_L);
	/* Ensure tablet mode is initialized according to the hardware state
	 * so that the cached state reflects reality. */
	hall_sensor_interrupt_debounce();
=======
	gpio_enable_interrupt(GMR_TABLET_MODE_GPIO_L);
	/*
	 * Ensure tablet mode is initialized according to the hardware state
	 * so that the cached state reflects reality.
	 */
	gmr_tablet_switch_interrupt_debounce();
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
}
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
DECLARE_HOOK(HOOK_INIT, hall_sensor_init, HOOK_PRIO_DEFAULT);
=======
DECLARE_HOOK(HOOK_INIT, gmr_tablet_switch_init, HOOK_PRIO_DEFAULT);
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
void hall_sensor_disable(void)
=======
void gmr_tablet_switch_disable(void)
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
{
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
	gpio_disable_interrupt(HALL_SENSOR_GPIO_L);
=======
	gpio_disable_interrupt(GMR_TABLET_MODE_GPIO_L);
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
	/* Cancel any pending debounce calls */
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
	hook_call_deferred(&hall_sensor_interrupt_debounce_data, -1);
=======
	hook_call_deferred(&gmr_tablet_switch_interrupt_debounce_data, -1);
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
	tablet_disable();
}
#endif

static int command_settabletmode(int argc, char **argv)
{
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
=======
	if (argc == 1) {
		print_tablet_mode();
		return EC_SUCCESS;
	}

>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	if (argv[1][0] == 'o' && argv[1][1] == 'n') {
		tablet_mode = true;
		tablet_mode_forced = true;
	} else if (argv[1][0] == 'o' && argv[1][1] == 'f') {
		tablet_mode = false;
		tablet_mode_forced = true;
	} else if (argv[1][0] == 'r') {
		tablet_mode_forced = false;
	} else {
		return EC_ERROR_PARAM1;
	}

	notify_tablet_mode_change();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tabletmode, command_settabletmode,
	"[on | off | reset]",
	"Manually force tablet mode to on, off or reset.");
