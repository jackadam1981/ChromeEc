/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_COMMON_H__
#define __X86_COMMON_H__

#include <drivers/espi.h>
#include <drivers/gpio.h>

/* Power signal GPIO configuration */
struct gpio_config {
	/* GPIO net name */
	const char *net_name;
	/* GPIO pin port name */
	const char *port_name;
	/* GPIO pin index */
	const gpio_pin_t pin;
	/* GPIO configuration flags */
	const gpio_flags_t flags;
	/* Device structure for the driver instance */
	const struct device *port;
};

struct gpio_interrupt_config {
	/* GPIO net name */
	const char *net_name;
	/* GPIO configuration */
	const struct gpio_config *config;
	/* GPIO callback */
	struct gpio_callback intr_cb;
	/* GPIO interrupt flags */
	const gpio_flags_t intr_flags;
	/* Disable at boot up */
	const bool disable_at_boot;
};

/* Power signals list */
enum power_signal {
	X86_SLP_S0_DEASSERTED,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
	X86_SLP_S5_DEASSERTED,
	X86_SLP_SUS_DEASSERTED,
	X86_RSMRST_L_PGOOD,
	X86_DSW_PWROK,
	X86_ALL_SYS_PGOOD,
	/* X86 signals count, GPIO and VW */
	POWER_SIGNAL_COUNT
};

/* Information of a GPIO power signal */
struct power_signal_gpio_info {
	const char *net_name;   /* GPIO net name of signal */
	enum power_signal power_sig;        /* Power signal*/
	uint32_t flags;		/* See POWER_SIGNAL_* macros */
	const char *name;
};

/* Information of a virtual wire power signal */
struct power_signal_vw_info {
	enum espi_vwire_signal vw_signal; /* ESPI VW signal */
	enum power_signal power_sig;      /* Power signal */
	uint32_t flags;	        /* See POWER_SIGNAL_* macros */
	const char *name;
};

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
enum chipset_shutdown_reason {
	CHIPSET_SHUTDOWN_BEGIN = BIT(15),
	CHIPSET_SHUTDOWN_POWERFAIL = CHIPSET_SHUTDOWN_BEGIN,
	/* Forcing a shutdown as part of EC initialization */
	CHIPSET_SHUTDOWN_INIT,
	/* Custom reason on a per-board basis. */
	CHIPSET_SHUTDOWN_BOARD_CUSTOM,
	/* This is a reason to inhibit startup, not cause shut down. */
	CHIPSET_SHUTDOWN_BATTERY_INHIBIT,
	/* A power_wait_signal is being asserted */
	CHIPSET_SHUTDOWN_WAIT,
	/* Critical battery level. */
	CHIPSET_SHUTDOWN_BATTERY_CRIT,
	/* Because you told me to. */
	CHIPSET_SHUTDOWN_CONSOLE_CMD,
	/* Forcing a shutdown to effect entry to G3. */
	CHIPSET_SHUTDOWN_G3,
	/* Force shutdown due to over-temperature. */
	CHIPSET_SHUTDOWN_THERMAL,
	/* Force a chipset shutdown from the power button through EC */
	CHIPSET_SHUTDOWN_BUTTON,

	CHIPSET_SHUTDOWN_COUNT,
};

/* Common device tree configurable attributes */
struct common_pwrseq_config {
	int pch_dsw_pwrok_delay_ms;
	int pch_pm_pwrbtn_delay_ms;
	int pch_rsmrst_delay_ms;
	int vr_en_vccin_delay_ms;
	/* Default timeout to wait for power signal */
	int wait_signal_timeout_ms;
};

/* This encapsulates the attributes of the state machine */
struct power_seq_context {
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

	/* S5 inactive time in seconds before power state change */
	int s5_timeout_s;
	/* Indicate should exit G3 power state or not */
	int want_g3_exit;
};

#endif /* __X86_COMMON_H__ */
