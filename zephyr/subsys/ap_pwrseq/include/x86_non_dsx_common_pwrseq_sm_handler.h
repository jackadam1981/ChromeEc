/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Chrome OS-specific x86 common Soc power sequencing
 */

#ifndef __X86_NON_DSX_H__
#define __X86_NON_DSX_H__

#include <drivers/gpio.h>

/*
 * @brief GPIO configuration structure
 */
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

/* Power sequencing GPIOs */

#define PCH_EC_SLP_SUS_L	slpsus
#define PCH_EC_SLP_S0_L		slps0

/* DSW_PWROK is indication to PCH that 3P3V is stable */
#define VR_EC_DSW_PWROK		dswpwrokin
#define EC_PCH_DSW_PWROK	dswpwrokout

/* RSMRST is used for resetting primary power plane logic
 * When de-asserted, this signal is an indication that the
 * power wells are stable.
 */
#define VR_PG_EC_RSMRST_ODL	rsmrstin
#define EC_PCH_RSMRST_L		rsmrstout

/* Signal represents the power good for all the rest
 * of platform voltage rails.
 */
#define VR_EC_ALL_SYS_PWRGD	allsyspwrgd

#define PCH_PWROK		pchpwrok
/* SYS_PWROK is a generic power good input to the PCH is driven
 * and utilized in platform-specific manner
 */
#define EC_PCH_SYS_PWROK	syspwrok

#define EC_VR_PPVAR_VCCIN	vccin

/* EC to PCH indication system request to sleep / wake */
#define EC_PCH_PWR_BTN_ODL	pchpwrbtn

/* Enable 5V rails */
#define EC_VR_EN_PP5000_A	enpp5p0
/* Enable 3.3V rails */
#define EC_VR_EN_PP3300_A	enpp3p3

#define IMVP9_VRRDY_OD		imvp9vrrdy

/* TODO: Move to chipset */
#define VCCST_PWRGD_OD		vccstpwrgd

/* GPIO net name as in schematics */
#define GPIO_NET_NAME(node)	DT_LABEL(DT_NODELABEL(node))

/* GPIO structure assignment */
#define POWER_SEQ_GPIO(node)	\
	.net_name = GPIO_NET_NAME(node), \
	.port_name = DT_GPIO_LABEL(DT_NODELABEL(node), gpios), \
	.pin = DT_GPIO_PIN(DT_NODELABEL(node), gpios), \
	.flags = DT_GPIO_FLAGS(DT_NODELABEL(node), gpios), \
	.port = DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(node), gpios, 0))

/* Check if the GPIO is present */
#define POWER_SEQ_GPIO_PRESENT(node) \
	DT_NODE_HAS_STATUS(DT_NODELABEL(node), okay)


/**
 * @brief System power states for Non Deep Sleep Well
 * EC is an always on device in a Non Deep Sx system except when the EC
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
	/*  Determine if the AP's power rails are turned on */
	SYS_POWER_STATE_G3S5,
	/*  Determine if AP is suspended from sleep */
	SYS_POWER_STATE_S5S4,
	/* Determine if Suspend to Disk is de-asserted*/
	SYS_POWER_STATE_S4S3,
	/* Determine if Suspend to RAM is de-asserted*/
	SYS_POWER_STATE_S3S0,

	/*
	 * Intermediate power down states
	 */
	/*  Determine if the AP's power rails are turned off */
	SYS_POWER_STATE_S5G3,
	/*  Determine if AP is suspended to sleep */
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

extern const int power_seq_gpios_count;

/* Delay in ms for pass through signals */
#define POWER_EC_PCH_DSW_PWROK_DELAY_MS	100
#define POWER_EC_PCH_RSMRST_DELAY_MS	10
#define POWER_EC_PCH_SYS_PWROK_DELAY_MS	50
#define POWER_EC_VR_EN_VCCIN_DELAY_MS	5
#define POWER_EC_PCH_PM_PWRBTN_DELAY_MS	200

void espi_bus_reset(void);

/*
 * @brief Create power sequencing thread
 *
 * @param p1 Thread sleep time in ms
 * TODO: Add details about inputs
 */
void pwrseq_thread(void *p1, void *p2, void *p3);

extern struct gpio_config power_seq_gpios[];
extern enum power_states_ndsx chipset_pwr_sm_run(
				enum power_states_ndsx curr_state);
extern void chipset_force_shutdown(enum chipset_shutdown_reason reason);
extern void all_sig_pass_thru_handler(void);
extern void common_rsmrst_pass_thru_handler(void);
extern void pwr_sm_set_state(enum power_states_ndsx new_state);
#endif /* __X86_NON_DSX_H__ */
