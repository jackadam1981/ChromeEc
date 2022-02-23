/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_POWER_SIGNALS_EXT_H__
#define __AP_PWRSEQ_POWER_SIGNALS_EXT_H__

#include <devicetree.h>


#define COMPAT_EXT	intel_ap_pwrseq_external

#if DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT)
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
 *         LOG(LOG_ERR, "Unknown board signal!");
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

#endif /* DT_HAS_COMPAT_STATUS_OKAY(COMPAT_EXT) */

#endif /* __AP_PWRSEQ_POWER_SIGNALS_EXT_H__ */
