/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <toolchain.h>
#include <power_signals.h>

#include "signal_interface.h"
#include "signal_gpio.h"
#include "signal_vw.h"

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_BASE)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(COMPAT_BASE) == 1,
	"Only one node for intel_ap_pwrseq is allowed");
#endif

/*
 * Enum indicating type of signal.
 */
enum signal_source {
	PWR_SIG_SRC_GPIO,
	PWR_SIG_SRC_VW,
	PWR_SIG_SRC_EXT,
};

struct ps_config {
	const char *debug_name;
	uint8_t source;
	uint8_t src_index;
};

/*
 * Generate enums for the GPIOs, virtual wire signals and
 * external signals.
 * The input and output GPIOs have the same enum, but the inputs
 * go first, since interrupts are not required for the outputs.
 */

#define TAG_PWR_ENUM(tag, name) DT_CAT(tag, name)

#define PWR_ENUM(id, tag)			\
	TAG_PWR_ENUM(tag, PWR_SIGNAL_ENUM(id))

#define PWR_ENUM_COMMA(id, tag)	PWR_ENUM(id, tag),

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN) || \
	DT_HAS_COMPAT_STATUS_OKAY(COMPAT_OUT)
enum power_signal_gpios {
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_IN, PWR_ENUM_COMMA, PWR_GPIO_)
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_OUT, PWR_ENUM_COMMA, PWR_GPIO_)
};
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_VW)
enum power_signal_vw {
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_VW, PWR_ENUM_COMMA, PWR_VW_)
};
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT)
enum power_signal_ext {
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_VW, PWR_ENUM_COMMA, PWR_EXT_)
};
#endif

#define GEN_PS_ENTRY(id, src, tag)		\
{						\
	.debug_name = DT_PROP(id, dbg_label),	\
	.source = src,				\
	.src_index = PWR_ENUM(id, tag),		\
},

/*
 * Generate the power signal configuration array.
 */
static const struct ps_config sig_config[] = {
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_IN, GEN_PS_ENTRY,
			     PWR_SIG_SRC_GPIO, PWR_GPIO_)
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_OUT, GEN_PS_ENTRY,
			     PWR_SIG_SRC_GPIO, PWR_GPIO_)
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_VW, GEN_PS_ENTRY,
			     PWR_SIG_SRC_VW, PWR_VW_)
DT_FOREACH_STATUS_OKAY_VARGS(COMPAT_EXT, GEN_PS_ENTRY,
			     PWR_SIG_SRC_EXT, PWR_EXT_)
};

int power_signal_get(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -1;  /* should never happen */

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN) || \
	DT_HAS_COMPAT_STATUS_OKAY(COMPAT_OUT)
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_get(cp->src_index);
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_VW)
	case PWR_SIG_SRC_VW:
		return power_signal_vw_get(cp->src_index);
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT)
	case PWR_SIG_SRC_EXT:
		return board_power_signal_get(signal);
#endif
	}
}

int power_signal_set(enum power_signal signal, int value)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -1; /* should never happen */

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN) || \
	DT_HAS_COMPAT_STATUS_OKAY(COMPAT_OUT)
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_set(cp->src_index, value);
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_VW)
	case PWR_SIG_SRC_VW:
		/* No virtual wire output */
		return -1;
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT)
	case PWR_SIG_SRC_EXT:
		return board_power_signal_set(signal, value);
#endif
	}
}

int power_signal_enable_interrupt(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		/*
		 * Not sure if board (external) signals will
		 * need interrupt enable/disable.
		 */
		return -1;  /* should never happen */

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN)
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_enable_int(cp->src_index);
#endif
	}
}

int power_signal_disable_interrupt(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -1;  /* should never happen */

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN)
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_disable_int(cp->src_index);
#endif
	}
}

const char *power_signal_name(enum power_signal signal)
{
	return sig_config[signal].debug_name;
}

void power_signal_init(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_IN) || \
	DT_HAS_COMPAT_STATUS_OKAY(COMPAT_OUT)
	power_signal_gpio_init();
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_VW)
	power_signal_vw_init();
#endif
}

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT)

/*
 * Default functions for board external signals.
 */
__attribute__((weak)) int board_power_signal_get(enum power_signal sig)
{
	/* Maybe log this */
	return -1;
}

__attribute__((weak)) int board_power_signal_set(enum power_signal sig,
						  int value)
{
	return -1;
}
#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT) */
