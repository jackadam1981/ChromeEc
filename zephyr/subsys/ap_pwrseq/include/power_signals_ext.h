/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_EXT_POWER_SIGNALS_H__
#define __AP_PWRSEQ_EXT_POWER_SIGNALS_H__

#include <devicetree.h>

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq)

#define TAG_EXT_PWR_SIGNAL	PWR_EXT_

#define ADD_TAG(tag, enum) DT_CAT(tag, enum)

#define GEN_EXT_PWR_ENUM(id)	\
	COND_CODE_1(DT_NODE_HAS_PROP(id, external), \
	(ADD_TAG(TAG_EXT_PWR_SIGNAL, DT_STRING_UPPER_TOKEN(id, enum_name)), ), \
	())

/**
 * @brief Enum of external power signals
 *
 * Defines the enums of all the board specific signals.
 */
enum power_signal_ext {
DT_FOREACH_CHILD(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	GEN_EXT_PWR_ENUM)
	EXT_POWER_SIGNAL_COUNT
};

#undef GEN_EXT_PWR_ENUM
#undef ADD_TAG

/*
 * Board specific functions (if required)
 */
int board_power_signal_get(enum power_signal_ext signal);
int board_power_signal_set(enum power_signal_ext signal, int value);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq) */

#endif /* __AP_PWRSEQ_EXT_POWER_SIGNALS_H__ */
