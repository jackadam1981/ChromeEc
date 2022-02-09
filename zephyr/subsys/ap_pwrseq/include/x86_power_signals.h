/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Define power signals from device tree */

#ifndef __X86_POWER_SIGNALS_H__
#define __X86_POWER_SIGNALS_H__

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
 * the device tree
 */
BUILD_ASSERT(POWER_SIGNAL_COUNT ==
	DT_PROP(POWER_SIGNALS_LIST_NODE, pwrseq_signals_required));

#endif /* (DT_NODE_EXISTS(POWER_SIGNALS_LIST_NODE)) */

/* Power signal flags */
#define POWER_SIGNAL_ACTIVE_STATE BIT(0)
/* Indicates GPIO interrupt disabled on boot or not */
#define POWER_SIGNAL_DISABLE_INT_ON_BOOT BIT(1)
#define POWER_SIGNAL_ACTIVE_LOW   0
#define POWER_SIGNAL_ACTIVE_HIGH  BIT(0)

/* Convert enum power_signal to a mask for signal functions */
#define POWER_SIGNAL_MASK(signal) (1 << (signal))

#if defined(CONFIG_AP_X86_INTEL_ADL)

/* Input state flags */
#define IN_PCH_SLP_S0_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S0_DEASSERTED)
#define IN_PCH_SLP_S3_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S3_DEASSERTED)
#define IN_PCH_SLP_S4_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S4_DEASSERTED)
#define IN_PCH_SLP_S5_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S5_DEASSERTED)
#define IN_PCH_SLP_SUS_DEASSERTED POWER_SIGNAL_MASK(X86_SLP_SUS_DEASSERTED)
#define IN_ALL_PM_SLP_DEASSERTED (IN_PCH_SLP_S3 | \
				  IN_PCH_SLP_S4 | \
				  IN_PCH_SLP_SUS)
#define IN_PGOOD_ALL_CORE POWER_SIGNAL_MASK(X86_DSW_PWROK)
#define IN_ALL_S0 (IN_PGOOD_ALL_CORE | IN_ALL_PM_SLP_DEASSERTED)
#define CHIPSET_G3S5_POWERUP_SIGNAL IN_PCH_SLP_SUS_DEASSERTED

#else
#warning("Input power signals state flags not defined");
#endif

#endif /* __X86_POWER_SIGNALS_H__ */
