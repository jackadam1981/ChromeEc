/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Define power signals from device tree */

#ifndef __X86_POWER_SIGNALS_H__
#define __X86_POWER_SIGNALS_H__

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_signal_list)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(intel_ap_pwrseq_signal_list) == 1,
	"Only one node for intel_ap_pwrseq_signal_list is allowed");
#endif

#define POWER_SIGNALS_LIST_NODE                                \
	DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq_signal_list)

#define GEN_POWER_SIGNAL_ENUM(id)                              \
	DT_STRING_UPPER_TOKEN(id, pwrseq_signal_enum)

#define GEN_GPIO_CONFIG(id)                                    \
{                                                              \
	.spec = GPIO_DT_SPEC_GET(id, pwrseq_int_gpios),        \
	.intr_flags = DT_PROP(id, pwrseq_int_flags),           \
}

#define GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA(id)                  \
{                                                              \
	.power_sig = GEN_POWER_SIGNAL_ENUM(id),                \
	.name = DT_PROP(id, dbg_label),                        \
	.source = DT_PROP(id, source),                         \
	.flags = DT_PROP(id, flags),                           \
	.gpio_config = GEN_GPIO_CONFIG(id),                      \
},

#define GEN_VW_POWER_SIGNAL_ENTRY_VW_ENUM(id)                  \
	DT_STRING_UPPER_TOKEN(id, pwrseq_vw_enum)

#define GEN_VW_POWER_SIGNAL_ENTRY_COMMA(id)                    \
{                                                              \
	.vw_signal = GEN_VW_POWER_SIGNAL_ENTRY_VW_ENUM(id),    \
	.power_sig = GEN_POWER_SIGNAL_ENUM(id),                \
	.source = DT_PROP(id, source),                         \
	.flags = DT_PROP(id, flags),                           \
	.name = DT_PROP(id, dbg_label),                        \
},

#define GEN_POWER_SIGNAL_ENTRY_COMMA(id)                       \
{                                                              \
	.power_sig = GEN_POWER_SIGNAL_ENUM(id),                \
	.source = DT_PROP(id, source),                         \
	.flags = DT_PROP(id, flags),                           \
	.name = DT_PROP(id, dbg_label),                        \
},


#define GEN_VW_POWER_SIGNAL_ENTRY(id)                          \
	COND_CODE_1(DT_PROP(id, source),      \
		(GEN_VW_POWER_SIGNAL_ENTRY_COMMA(id)),\
		(GEN_POWER_SIGNAL_ENTRY_COMMA(id)))

#define GEN_POWER_SIGNAL_INFO_ENTRY(id) \
	COND_CODE_0(DT_PROP(id, source),    \
	(GEN_GPIO_POWER_SIGNAL_ENTRY_COMMA(id)),\
	(GEN_VW_POWER_SIGNAL_ENTRY(id)))


#define GEN_POWER_SIGNAL_ENUM_COMMA(id)                        \
	GEN_POWER_SIGNAL_ENUM(id),

enum power_signal {
#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_signal_list)
	DT_FOREACH_CHILD(
		POWER_SIGNALS_LIST_NODE,
		GEN_POWER_SIGNAL_ENUM_COMMA)
#endif
	POWER_SIGNAL_COUNT
};

enum power_source {
	SOURCE_GPIO,
	SOURCE_VW,
	SOURCE_OTHERS,
};

/*
 * Verify the number of required power signals are specified in
 * the device tree
 */
BUILD_ASSERT(POWER_SIGNAL_COUNT ==
	DT_PROP(POWER_SIGNALS_LIST_NODE, pwrseq_signals_required));

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
