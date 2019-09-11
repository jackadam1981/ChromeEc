/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_angle.h"
#include "tablet_mode.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_LID, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_LID, format, ## args)

<<<<<<< HEAD   (40c69e usb_pd_protocol: Add a 3ms delay between polling ALERT#.)
/* 1: in tablet mode. 0: otherwise */
static int tablet_mode = 1;
=======
/* 1: in tablet mode; 0: notebook mode; -1: uninitialized  */
static int tablet_mode = -1;
static int forced_tablet_mode = -1;

/* 1: hall sensor is reporting 360 degrees. */
static int hall_sensor_at_360;

/*
 * 1: all calls to tablet_set_mode are ignored and tablet_mode if forced to 0
 * 0: all calls to tablet_set_mode are honored
 */
static int disabled;
>>>>>>> CHANGE (87502c tablet_mode: expose console command.)

int tablet_get_mode(void)
{
<<<<<<< HEAD   (40c69e usb_pd_protocol: Add a 3ms delay between polling ALERT#.)
	return tablet_mode;
=======
	if (forced_tablet_mode != -1)
		return !!forced_tablet_mode;
	return !!tablet_mode;
>>>>>>> CHANGE (87502c tablet_mode: expose console command.)
}

void tablet_set_mode(int mode)
{
	if (tablet_mode == mode)
		return;

	tablet_mode = mode;

	if (forced_tablet_mode != -1)
		return;

	CPRINTS("tablet mode %sabled", mode ? "en" : "dis");
	hook_notify(HOOK_TABLET_MODE_CHANGE);
<<<<<<< HEAD   (40c69e usb_pd_protocol: Add a 3ms delay between polling ALERT#.)
=======

#ifdef CONFIG_HOSTCMD_EVENTS
	/*
	 * When tablet mode changes, send an event to ACPI to retrieve
	 * tablet mode value and send an event to the kernel.
	 */
	host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
#endif
}

static void tabletmode_force_state(int mode)
{
	if (forced_tablet_mode == mode)
		return;

	forced_tablet_mode = mode;

	hook_notify(HOOK_TABLET_MODE_CHANGE);
	if (IS_ENABLED(CONFIG_HOSTCMD_EVENTS))
		host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
}

void tablet_disable(void)
{
	tablet_mode = 0;
	disabled = 1;
>>>>>>> CHANGE (87502c tablet_mode: expose console command.)
}

/* This ifdef can be removed once we clean up past projects which do own init */
#ifdef CONFIG_TABLET_SWITCH
#ifndef TABLET_MODE_GPIO_L
#error  TABLET_MODE_GPIO_L must be defined
#endif
static void tablet_mode_debounce(void)
{
	/* We won't reach here on boards without a dedicated tablet switch */
	tablet_set_mode(!gpio_get_level(TABLET_MODE_GPIO_L));

#ifdef CONFIG_LID_ANGLE_UPDATE
	/* Then, we disable peripherals only when the lid reaches 360 position.
	 * (It's probably already disabled by motion_sense_task.)
	 * We deliberately do not enable peripherals when the lid is leaving
	 * 360 position. Instead, we let motion_sense_task enable it once it
	 * reaches laptop zone (180 or less). */
	if (tablet_mode)
		lid_angle_peripheral_enable(0);
#endif /* CONFIG_LID_ANGLE_UPDATE */
}
DECLARE_DEFERRED(tablet_mode_debounce);

#define TABLET_DEBOUNCE_US    (30 * MSEC)  /* Debounce time for tablet switch */

void tablet_mode_isr(enum gpio_signal signal)
{
	hook_call_deferred(&tablet_mode_debounce_data, TABLET_DEBOUNCE_US);
}

static void tablet_mode_init(void)
{
	gpio_enable_interrupt(TABLET_MODE_GPIO_L);
	/* Ensure tablet mode is initialized according to the hardware state
	 * so that the cached state reflects reality. */
	tablet_mode_debounce();
}
DECLARE_HOOK(HOOK_INIT, tablet_mode_init, HOOK_PRIO_DEFAULT);
#endif

static int command_settabletmode(int argc, char **argv)
{
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;
	if (argv[1][0] == 'o' && argv[1][1] == 'n')
		tabletmode_force_state(1);
	else if (argv[1][0] == 'o' && argv[1][1] == 'f')
		tabletmode_force_state(0);
	else if (argv[1][0] == 'r')
		tabletmode_force_state(-1);
	else
		return EC_ERROR_PARAM1;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tabletmode, command_settabletmode,
	"[on | off | reset]",
	"Manually force tablet mode to on, off or reset.");
