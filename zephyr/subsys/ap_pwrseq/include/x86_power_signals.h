/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Define power signals from device tree */

#ifndef __X86_POWER_SIGNALS_H__
#define __X86_POWER_SIGNALS_H__

#include <drivers/espi.h>
#include <drivers/gpio.h>

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_signal_list)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(intel_ap_pwrseq_signal_list) == 1,
	"Only one node for intel_ap_pwrseq_signal_list is allowed");
#endif

#define POWER_SIGNALS_LIST_NODE                                \
	DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq_signal_list)

#define GEN_POWER_SIGNAL_ENUM(id)                              \
	DT_STRING_UPPER_TOKEN(id, pwrseq_signal_enum)

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

#define POWER_SIGNAL_GPIO_COUNT                                \
	DT_PROP(POWER_SIGNALS_LIST_NODE, pwrseq_signals_gpio_count)

enum power_source {
	SOURCE_GPIO,
	SOURCE_VW,
	SOURCE_OTHER,
};

/*
 * Verify the number of required power signals are specified in
 * the device tree
 */
BUILD_ASSERT(POWER_SIGNAL_COUNT ==
	DT_PROP(POWER_SIGNALS_LIST_NODE, pwrseq_signals_required));

/* Information of a GPIO power signal */
struct power_signal_gpio_config {
	const struct gpio_dt_spec spec;
	gpio_flags_t intr_flags; /* GPIO interrupt flags */
	bool enable_on_boot;     /* Enable interrupt at boot up */
};

struct power_signal_info {
	enum power_signal power_sig;
	char *name;
	int source;
	uint32_t flags;
	union {
		struct power_signal_gpio_config gpio_config;
		enum espi_vwire_signal vw_signal; /* ESPI VW signal */
	};
};

/* Power signal flags */
#define POWER_SIGNAL_ACTIVE_STATE BIT(0)
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
