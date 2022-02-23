/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_POWER_SIGNALS_EXT_H__
#define __AP_PWRSEQ_POWER_SIGNALS_EXT_H__

#include <devicetree.h>

#define COMPAT_EXT	intel_ap_pwrseq_external

/**
 * Definitions required for external (board-specific)
 * power signals. The externally provided power signals
 * are represented as an enum that is the same
 * name as the power_signal enum, with 'PWR_EXT_' prepended
 * E.g if the signal PWR_VCCST_PWRGD is to be provided by the
 * board-specific function, an enum is created PWR_EXT_PWR_VCCST_PWRGD
 * and the board function would appear:
 *
 * int board_power_signal_get(enum power_signal_ext signal)
 * {
 *     int value;
 *
 *     switch(signal) {
 *     default:
 *         LOG(LOG_ERR, "Unknown board signal!");
 *         return -1;
 *
 *     case PWR_EXT_PWR_VCCST_PWRGD:
 *         value = ...
 *         return value;
 *     }
 * }
 *
 */

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT)

#define TAG_EXT_PWR_SIGNAL	PWR_EXT_

#define ADD_TAG(tag, enum) DT_CAT(tag, enum)

#define GEN_EXT_PWR_ENUM(id)	\
	ADD_TAG(TAG_EXT_PWR_SIGNAL, DT_STRING_UPPER_TOKEN(id, enum_name)),

/**
 * @brief Enum of external power signals
 *
 * Defines the enums of all the board specific signals.
 */
enum power_signal_ext {
DT_FOREACH_STATUS_OKAY(COMPAT_EXT, GEN_EXT_PWR_ENUM)
};

#undef GEN_EXT_PWR_ENUM
#undef ADD_TAG

/*
 * Board specific functions (if required)
 */
int board_power_signal_get(enum power_signal_ext signal);
int board_power_signal_set(enum power_signal_ext signal, int value);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT) */

#endif /* __AP_PWRSEQ_POWER_SIGNALS_EXT_H__ */
