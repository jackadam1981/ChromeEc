/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <toolchain.h>
#include <logging/log.h>

#include <power_signals.h>

#include "signal_gpio.h"
#include "signal_interrupt.h"
#include "signal_named_gpio.h"
#include "signal_vw.h"

LOG_MODULE_DECLARE(ap_pwrseq, 4);

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(intel_ap_pwrseq) == 1,
	"Only one node for intel_ap_pwrseq is allowed");
#endif

/*
 * Enum indicating type (source) of signal.
 */
enum signal_source {
	PWR_SIG_SRC_GPIO,
	PWR_SIG_SRC_VW,
	PWR_SIG_SRC_EXT,
	PWR_SIG_SRC_INTERRUPT,
	PWR_SIG_SRC_NAMED_GPIO,
};

struct ps_config {
	const char *debug_name;
	uint8_t source;
	uint8_t src_index;
};

/*
 * Generate enums for the GPIOs, virtual wire signals and
 * external signals. These enums are only used internally
 * to assign an index to each signal that is specific
 * to the source.
 */

#define TAG_PWR_ENUM(tag, name) DT_CAT(tag, name)

#define PWR_ENUM(id, tag)			\
	TAG_PWR_ENUM(tag, PWR_SIGNAL_ENUM(id))

#define PWR_ENUM_COMMA(id, tag)	PWR_ENUM(id, tag),

#if HAS_GPIO_SIGNALS
enum {
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_gpio, PWR_ENUM_COMMA, PWR_GPIO_)
};
#endif

#if HAS_VW_SIGNALS
enum {
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_vw, PWR_ENUM_COMMA, PWR_VW_)
};
#endif

#if HAS_EXT_SIGNALS
enum {
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_external, PWR_ENUM_COMMA, PWR_EXT_)
};
#endif

#if HAS_INTERRUPT_SIGNALS
enum {
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_interrupt,
			     PWR_ENUM_COMMA, PWR_ISR_)
};
#endif

#if HAS_NAMED_GPIO_SIGNALS
enum {
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_named_gpio,
			     PWR_ENUM_COMMA, PWR_NG_)
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
 * This has to be in the same order as the
 * enum generation in include/power_signals.h
 */
static const struct ps_config sig_config[] = {
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_gpio, GEN_PS_ENTRY,
			     PWR_SIG_SRC_GPIO, PWR_GPIO_)
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_vw, GEN_PS_ENTRY,
			     PWR_SIG_SRC_VW, PWR_VW_)
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_external, GEN_PS_ENTRY,
			     PWR_SIG_SRC_EXT, PWR_EXT_)
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_interrupt, GEN_PS_ENTRY,
			     PWR_SIG_SRC_INTERRUPT, PWR_ISR_)
DT_FOREACH_STATUS_OKAY_VARGS(intel_ap_pwrseq_named_gpio, GEN_PS_ENTRY,
			     PWR_SIG_SRC_NAMED_GPIO, PWR_NG_)
};

static power_signal_mask_t power_signals;
static power_signal_mask_t debug_signals;

void power_update_signals(void)
{
	power_signal_mask_t n = 0;

	for (int i = 0; i < POWER_SIGNAL_COUNT; i++) {
		if (power_signal_get(i)) {
			n |= BIT(i);
		}
	}
	/* Check if any signals flagged for debug have changed. */
	if ((n ^ power_signals) & debug_signals) {
		LOG_INF("power update (0x%04x -> 0x%04x, 0x%04x changed)",
			power_signals, n, n ^ power_signals);
	}
	power_signals = n;
}

void power_set_debug(power_signal_mask_t debug)
{
	debug_signals = debug;
}

power_signal_mask_t power_get_debug(void)
{
	return debug_signals;
}

void power_signal_interrupt(void)
{
	power_update_signals();
}

int power_wait_mask_signals_timeout(power_signal_mask_t want,
				    power_signal_mask_t mask,
				    int timeout)
{
	if (mask == 0) {
		return 0;
	}
	want &= mask;
	while (timeout-- > 0) {
		if ((power_signals & mask) == want) {
			return 0;
		}
	}
	power_update_signals();
	return -ETIMEDOUT;
}

int power_signal_get(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -EINVAL;  /* should never happen */

#if HAS_GPIO_SIGNALS
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_get(cp->src_index);
#endif

#if HAS_VW_SIGNALS
	case PWR_SIG_SRC_VW:
		return power_signal_vw_get(cp->src_index);
#endif

#if HAS_EXT_SIGNALS
	case PWR_SIG_SRC_EXT:
		return board_power_signal_get(signal);
#endif

#if HAS_INTERRUPT_SIGNALS
	case PWR_SIG_SRC_INTERRUPT:
		return power_signal_interrupt_get(cp->src_index);
#endif

#if HAS_NAMED_GPIO_SIGNALS
	case PWR_SIG_SRC_NAMED_GPIO:
		return power_signal_named_gpio_get(cp->src_index);
#endif
	}
}

/*
 * Virtual wire and named interrupts are not able to be set.
 */
int power_signal_set(enum power_signal signal, int value)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -EINVAL;

#if HAS_GPIO_SIGNALS
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_set(cp->src_index, value);
#endif

#if HAS_EXT_SIGNALS
	case PWR_SIG_SRC_EXT:
		return board_power_signal_set(signal, value);
#endif

#if HAS_NAMED_GPIO_SIGNALS
	case PWR_SIG_SRC_NAMED_GPIO:
		return power_signal_named_gpio_set(cp->src_index, value);
#endif
	}
}

/*
 * Only GPIOs and named interrupts can enable/disable interrupts.
 */
int power_signal_enable_interrupt(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		/*
		 * Not sure if board (external) signals will
		 * need interrupt enable/disable.
		 */
		return -EINVAL;

#if HAS_GPIO_SIGNALS
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_enable_int(cp->src_index);
#endif

#if HAS_INTERRUPT_SIGNALS
	case PWR_SIG_SRC_INTERRUPT:
		return power_signal_interrupt_enable_int(cp->src_index);
#endif
	}
}

int power_signal_disable_interrupt(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -EINVAL;

#if HAS_GPIO_SIGNALS
	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_disable_int(cp->src_index);
#endif

#if HAS_INTERRUPT_SIGNALS
	case PWR_SIG_SRC_INTERRUPT:
		return power_signal_interrupt_disable_int(cp->src_index);
#endif
	}
}

const char *power_signal_name(enum power_signal signal)
{
	return sig_config[signal].debug_name;
}

void power_signal_init(void)
{
	if (IS_ENABLED(HAS_GPIO_SIGNALS)) {
		power_signal_gpio_init();
	}
	if (IS_ENABLED(HAS_VW_SIGNALS)) {
		power_signal_vw_init();
	}
	if (IS_ENABLED(HAS_INTERRUPT_SIGNALS)) {
		power_signal_interrupt_init();
	}
}
