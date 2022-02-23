/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_POWER_SIGNALS_H__
#define __AP_PWRSEQ_POWER_SIGNALS_H__

#include <devicetree.h>

/*
 * DTS Compats used.
 */

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq)

#define	HAS_VW_SIGNALS	  DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_vw)
#define	HAS_GPIO_SIGNALS  DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_gpio)
#define	HAS_EXT_SIGNALS   DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_external)

/**
 * @brief Definitions for AP power sequence signals.
 *
 * Defines the enums for the AP power sequence signals.
 */

/**
 * @brief Generate the enum for this power signal.
 */
#define PWR_SIGNAL_ENUM(id) \
	 DT_STRING_UPPER_TOKEN(id, enum_name)

#define PWR_SIGNAL_ENUM_COMMA(id) \
	 PWR_SIGNAL_ENUM(id),
/**
 * @brief Enum of all power signals
 *
 * Defines the enums of all the power signals configured
 * in the system. Uses the 'enum-name' property to name
 * the signal.
 */
enum power_signal {
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_gpio, PWR_SIGNAL_ENUM_COMMA)
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_vw, PWR_SIGNAL_ENUM_COMMA)
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_external, PWR_SIGNAL_ENUM_COMMA)
	POWER_SIGNAL_COUNT,
};

#undef PWR_SIGNAL_ENUM_COMMA

#if HAS_EXT_SIGNALS
/**
 * Definitions required for external (board-specific)
 * power signals.
 *
 * int board_power_signal_get(enum power_signal signal)
 * {
 *     int value;
 *
 *     switch(signal) {
 *     default:
 *         LOG(LOG_ERR, "Unknown power signal!");
 *         return -1;
 *
 *     case PWR_VCCST_PWRGD:
 *         value = ...
 *         return value;
 *     }
 * }
 *
 */

/*
 * Board specific functions (if required)
 */
int board_power_signal_get(enum power_signal signal);
int board_power_signal_set(enum power_signal signal, int value);

#endif /* HAS_EXT_SIGNALS */

#endif /* DT_HAS_COMPAT_STATUS_OKAY */

#define POWER_SIGNAL_MASK(signal) (1 << (signal))

#endif /* __AP_PWRSEQ_POWER_SIGNALS_H__ */
