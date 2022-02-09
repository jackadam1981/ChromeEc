/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_COMMON_PWRSEQ_H__
#define __X86_COMMON_PWRSEQ_H__

#include <drivers/espi.h>
#include <drivers/gpio.h>
#include <logging/log.h>

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
	PWRSEQ_CHIPSET_SHUTDOWN_BEGIN = BIT(15),
	PWRSEQ_CHIPSET_SHUTDOWN_POWERFAIL = PWRSEQ_CHIPSET_SHUTDOWN_BEGIN,
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
};

struct pwrseq_gpio_int_config {
	const struct device *port;     /* GPIO device */
	const char *port_name;
	gpio_pin_t pin;                /* GPIO pin */
	struct gpio_callback intr_cb;  /* GPIO callback */
	const gpio_flags_t intr_flags; /* GPIO interrupt flags */
};

/* Extract power signals information from devicetree */
#define POWER_SIGNALS_LIST_NODE                                \
	DT_NODELABEL(pwrseq_signals_list)

#if (DT_NODE_EXISTS(POWER_SIGNALS_LIST_NODE))
#define GEN_POWER_SIGNAL_ENUM(id)                              \
	DT_STRING_UPPER_TOKEN(id, pwrseq_signal_enum)

/*
 * Pass id of gpio-interrupts child node to GPIO_INT_CREATE
 * For example, the node int_slp_s0.
 * int_slp_s0: slp_s0 {
 *     irq-pin = <&gpio_slp_s0_l>;
 *     flags = <GPIO_INT_EDGE_BOTH>;
 *     handler = "power_signal_interrupt";
 * };
 */

#define GPIO_INT_CREATE(id, irq_pin)                           \
{                                                              \
	.intr_flags = DT_PROP(id, flags),                      \
	.port = DEVICE_DT_GET(DT_GPIO_CTLR(irq_pin, gpios)),   \
	.port_name = DT_GPIO_LABEL(irq_pin, gpios),            \
	.pin = DT_GPIO_PIN(irq_pin, gpios),                    \
}

#define CONFIG_FROM_GPIO_INT(id)\
	GPIO_INT_CREATE(id, DT_PHANDLE(id, irq_pin))

#define GEN_POWER_INT_CONFIG(id) \
	CONFIG_FROM_GPIO_INT(DT_PROP(id, pwrseq_gpio_int_node))

#define GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA(id)                  \
{                                                              \
	.int_config = GEN_POWER_INT_CONFIG(id),                \
	.power_sig = GEN_POWER_SIGNAL_ENUM(id),                \
	.flags = DT_PROP(id, flags),                           \
	.name = DT_PROP(id, dbg_label),                        \
},

#define GEN_GPIO_POWER_SIGNAL_ENTRY(id)                        \
	COND_CODE_1(DT_NODE_HAS_PROP(id, pwrseq_gpio_int_node),\
		(GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA(id)), ())

#define GEN_VW_POWER_SIGNAL_ENTRY_VW_ENUM(id)                  \
	DT_STRING_UPPER_TOKEN(id, pwrseq_vw_enum)

#define GEN_VW_POWER_SIGNAL_ENTRY_COMMA(id)                    \
{                                                              \
	.vw_signal = GEN_VW_POWER_SIGNAL_ENTRY_VW_ENUM(id),    \
	.power_sig = GEN_POWER_SIGNAL_ENUM(id),                \
	.flags = DT_PROP(id, flags),                           \
	.name = DT_PROP(id, dbg_label),                        \
},

#define GEN_VW_POWER_SIGNAL_ENTRY(id)                          \
	COND_CODE_1(DT_NODE_HAS_PROP(id, pwrseq_vw_enum),      \
		(GEN_VW_POWER_SIGNAL_ENTRY_COMMA(id)), ())

#define GEN_POWER_SIGNAL_ENUM_COMMA(id)                        \
	GEN_POWER_SIGNAL_ENUM(id),

enum power_signal {
	DT_FOREACH_CHILD(
		POWER_SIGNALS_LIST_NODE,
		GEN_POWER_SIGNAL_ENUM_COMMA)
	POWER_SIGNAL_COUNT
};
/*
 * Verify the number of required power signals are specified in
 * the devicetree
 */
BUILD_ASSERT(POWER_SIGNAL_COUNT ==
	DT_PROP(POWER_SIGNALS_LIST_NODE, pwrseq_signals_required));
#endif /* (DT_NODE_EXISTS(POWER_SIGNALS_LIST_NODE)) */

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

#endif /* __X86_COMMON_PWRSEQ_H__ */
