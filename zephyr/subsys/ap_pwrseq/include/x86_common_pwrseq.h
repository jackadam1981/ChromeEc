/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_COMMON_PWRSEQ_H__
#define __X86_COMMON_PWRSEQ_H__

#include <drivers/espi.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <x86_power_signals.h>

/**
 * @brief System power states for Non Deep Sleep Well
 * EC is an always on device in a Non Deep Sx system except when EC
 * is hibernated or all the VRs are turned off.
 */
enum power_states_ndsx {
	/*
	 * Actual power states
	 */
	/* AP is off & EC is on */
	SYS_POWER_STATE_G3,
	/* AP is in soft off state */
	SYS_POWER_STATE_S5,
	/* AP is suspended to Non-volatile disk */
	SYS_POWER_STATE_S4,
	/* AP is suspended to RAM */
	SYS_POWER_STATE_S3,
	/* AP is in active state */
	SYS_POWER_STATE_S0,

	/*
	 * Intermediate power up states
	 */
	/* Determine if the AP's power rails are turned on */
	SYS_POWER_STATE_G3S5,
	/* Determine if AP is suspended from sleep */
	SYS_POWER_STATE_S5S4,
	/* Determine if Suspend to Disk is de-asserted */
	SYS_POWER_STATE_S4S3,
	/* Determine if Suspend to RAM is de-asserted */
	SYS_POWER_STATE_S3S0,

	/*
	 * Intermediate power down states
	 */
	/* Determine if the AP's power rails are turned off */
	SYS_POWER_STATE_S5G3,
	/* Determine if AP is suspended to sleep */
	SYS_POWER_STATE_S4S5,
	/* Determine if Suspend to Disk is asserted */
	SYS_POWER_STATE_S3S4,
	/* Determine if Suspend to RAM is asserted */
	SYS_POWER_STATE_S0S3,
};

/*
 * AP hard shutdowns are logged on the same path as resets.
 */
enum pwrseq_chipset_shutdown_reason {
	PWRSEQ_CHIPSET_SHUTDOWN_POWERFAIL,
	/* Forcing a shutdown as part of EC initialization */
	PWRSEQ_CHIPSET_SHUTDOWN_INIT,
	/* Forcing shutdown with command */
	PWRSEQ_CHIPSET_SHUTDOWN_CONSOLE_CMD,
	/* Forcing a shutdown to effect entry to G3. */
	PWRSEQ_CHIPSET_SHUTDOWN_G3,
	/* Force a chipset shutdown from the power button through EC */
	PWRSEQ_CHIPSET_SHUTDOWN_BUTTON,

	PWRSEQ_CHIPSET_SHUTDOWN_COUNT,
};

/* This encapsulates the attributes of the state machine */
struct pwrseq_context {
	/* On power-on start boot up sequence */
	enum power_states_ndsx power_state;

	/*
	 * Current input power signal states. Each bit represents an input
	 * power signal that is defined by enum power_signal in same order.
	 * 1 - signal state is asserted.
	 * 0 - signal state is de-asserted.
	 */
	uint32_t in_signals;
	/* Input signal state we're waiting for */
	uint32_t in_want;
	/* Signal values which print debug output */
	uint32_t in_debug;
	/* Indicate should exit G3 power state or not */
	int want_g3_exit;
};

struct pwrseq_gpio_int_config {
	const struct device *port;     /* GPIO device */
	const char *port_name;
	gpio_pin_t pin;                /* GPIO pin */
	struct gpio_callback intr_cb;  /* GPIO callback */
	const gpio_flags_t intr_flags; /* GPIO interrupt flags */
};

/* Information of a GPIO power signal */
struct power_signal_gpio_info {
	enum power_signal power_sig;        /* Power signal*/
	struct pwrseq_gpio_int_config int_config;
	uint32_t flags;        /* See POWER_SIGNAL_* macros */
	const char *name;
};

/* Information of a virtual wire power signal */
struct power_signal_vw_info {
	enum espi_vwire_signal vw_signal; /* ESPI VW signal */
	enum power_signal power_sig;        /* Power signal */
	uint32_t flags;	       /* See POWER_SIGNAL_* macros */
	const char *name;
};

/*
 * This is from hooks.h to use the shimmed hooks from legacy EC
 * Should be removed once the native implementation is up
 */
enum hook_type {
	/*
	 * System initialization.
	 *
	 * Hook routines are called from main(), after all hard-coded inits,
	 * before task scheduling is enabled.
	 */
	HOOK_INIT = 0,

	/*
	 * System clock changed frequency.
	 *
	 * The "pre" frequency hook is called before we change the frequency.
	 * There is no way to cancel.  Hook routines are always called from
	 * a task, so it's OK to lock a mutex here.  However, they may be called
	 * from a deferred task on some platforms so callbacks must make sure
	 * not to do anything that would require some other deferred task to
	 * run.
	 */
	HOOK_PRE_FREQ_CHANGE,
	HOOK_FREQ_CHANGE,

	/*
	 * About to jump to another image.  Modules which need to preserve data
	 * across such a jump should save it here and restore it in HOOK_INIT.
	 *
	 * Hook routines are called from the context which initiates the jump,
	 * WITH INTERRUPTS DISABLED.
	 */
	HOOK_SYSJUMP,

	/*
	 * Initialization for components such as PMU to be done before host
	 * chipset/AP starts up.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_PRE_INIT,

	/* System is starting up.  All suspend rails are now on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_STARTUP,

	/*
	 * System is resuming from suspend, or booting and has reached the
	 * point where all voltage rails are on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_RESUME,

	/*
	 * System is suspending, or shutting down; all voltage rails are still
	 * on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_SUSPEND,

#ifdef CONFIG_CHIPSET_RESUME_INIT_HOOK
	/*
	 * Initialization before the system resumes, like enabling the SPI
	 * driver such that it can receive a host resume event.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_RESUME_INIT,

	/*
	 * System has suspended. It is paired with CHIPSET_RESUME_INIT hook,
	 * like reverting the initialization of the SPI driver.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_SUSPEND_COMPLETE,
#endif

	/*
	 * System is shutting down.  All suspend rails are still on.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_SHUTDOWN,

	/*
	 * System has already shut down. All the suspend rails are already off.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_SHUTDOWN_COMPLETE,

	/*
	 * System is in G3.  All power rails are now turned off.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_HARD_OFF,

	/*
	 * System reset in S0.  All rails are still up.
	 *
	 * Hook routines are called from the chipset task.
	 */
	HOOK_CHIPSET_RESET,

	/*
	 * AC power plugged in or removed.
	 *
	 * Hook routines are called from the TICK task.
	 */
	HOOK_AC_CHANGE,

	/*
	 * Lid opened or closed.  Based on debounced lid state, not raw lid
	 * GPIO input.
	 *
	 * Hook routines are called from the TICK task.
	 */
	HOOK_LID_CHANGE,

	/*
	 * Device in tablet mode (base behind lid).
	 *
	 * Hook routines are called from the TICK task.
	 */
	HOOK_TABLET_MODE_CHANGE,

	/*
	 * Detachable device connected to a base.
	 *
	 * Hook routines are called from the TICK task.
	 */
	HOOK_BASE_ATTACHED_CHANGE,

	/*
	 * Power button pressed or released.  Based on debounced power button
	 * state, not raw GPIO input.
	 *
	 * Hook routines are called from the TICK task.
	 */
	HOOK_POWER_BUTTON_CHANGE,

	/*
	 * Battery state of charge changed
	 *
	 * Hook routines are called from the charger task.
	 */
	HOOK_BATTERY_SOC_CHANGE,

#ifdef CONFIG_USB_SUSPEND
	/*
	 * Called when there is a change in USB power management status
	 * (suspended or resumed).
	 *
	 * Hook routines are called from HOOKS task.
	 */
	HOOK_USB_PM_CHANGE,
#endif

	/*
	 * Periodic tick, every HOOK_TICK_INTERVAL.
	 *
	 * Hook routines will be called from the TICK task.
	 */
	HOOK_TICK,

	/*
	 * Periodic tick, every second.
	 *
	 * Hook routines will be called from the TICK task.
	 */
	HOOK_SECOND,

	/*
	 * USB PD cc disconnect event.
	 */
	HOOK_USB_PD_DISCONNECT,

	/*
	 * USB PD cc connection event.
	 */
	HOOK_USB_PD_CONNECT,

#ifdef TEST_BUILD
	/*
	 * Special hook types to be used by unit tests of the hooks
	 * implementation only.
	 */
	HOOK_TEST_1,
	HOOK_TEST_2,
	HOOK_TEST_3,
#endif  /* TEST_BUILD */

	/*
	 * Not a hook type (instead the number of hooks). This should
	 * always be placed at the end of this enumeration.
	 */
	HOOK_TYPE_COUNT,
};

extern void hook_notify(enum hook_type type);

#endif /* __X86_COMMON_PWRSEQ_H__ */
