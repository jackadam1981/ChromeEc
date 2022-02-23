/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <power_signals.h>

#include "signal_interface.h"
#include "signal_gpio.h"
#include "signal_vw.h"

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
	uint8_t src_id;
};

#define GEN_PS_ENTRY(id, src, tag) \
[PWR_SIGNAL_ENUM(id)] =					\
{							\
	.debug_name = DT_PROP(id, dbg_label),		\
	.source = src,					\
	.src_id = SRC_PWR_SIGNAL_ENUM(id, tag),		\
},

#define SELECT_SIGNAL(id, prop, src, tag)		\
	COND_CODE_1(DT_NODE_HAS_PROP(id, prop),		\
	(GEN_PS_ENTRY(id, src, tag)),			\
	())

/*
 * Generate the power signal configuration array.
 */
static const struct ps_config sig_config[] = {
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	SELECT_SIGNAL, input_gpios, PWR_SIG_SRC_GPIO, TAG_GPIO_PWR_SIGNAL)
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	SELECT_SIGNAL, output_gpios, PWR_SIG_SRC_GPIO, TAG_GPIO_PWR_SIGNAL)
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	SELECT_SIGNAL, virtual_wire, PWR_SIG_SRC_VW, TAG_VW_PWR_SIGNAL)
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	SELECT_SIGNAL, external, PWR_SIG_SRC_EXT, TAG_EXT_PWR_SIGNAL)
};

int power_signal_get(enum power_signal signal)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -1;  /* should never happen */

	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_get(cp->src_id);

	case PWR_SIG_SRC_VW:
		return power_signal_vw_get(cp->src_id);

	case PWR_SIG_SRC_EXT:
		return board_power_signal_get(cp->src_id);
	}
}

int power_signal_set(enum power_signal signal, int value)
{
	const struct ps_config *cp = &sig_config[signal];

	switch (cp->source) {
	default:
		return -1; /* should never happen */

	case PWR_SIG_SRC_GPIO:
		return power_signal_gpio_set(cp->src_id, value);

	case PWR_SIG_SRC_VW:
		/* No virtual wire output */
		return -1;

	case PWR_SIG_SRC_EXT:
		return board_power_signal_set(cp->src_id, value);
	}
}

const char *power_signal_name(enum power_signal signal)
{
	return sig_config[signal].debug_name;
};

void power_signal_init(void)
{
	power_signal_gpio_init();
}

/*
 * Default functions for board external signals.
 */
__attribute__((weak)) int board_power_signal_get(enum power_signal_ext sig)
{
	/* Maybe log this */
	return -1;
}

__attribute__((weak)) int board_power_signal_set(enum power_signal_ext sig,
						  int value)
{
	return -1;
}
