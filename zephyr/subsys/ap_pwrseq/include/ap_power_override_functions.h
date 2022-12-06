/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Declare functions that are supplied externally.
 * The functions are all prepended with board_ap_power_ to indicate
 * they have external implementations.
 *
 * TODO(b/223923728): Longer term, a framework should be put in place to
 * allow extensibility for selected functions.
 *
 * The external functions may need to access
 * devicetree properties for values such
 * as timeouts etc.
 */

#ifndef __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__
#define __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__

#include <zephyr/devicetree.h>

/**
 * @brief Force AP shutdown
 *
 * Immediately shut down the AP.
 */
void board_ap_power_force_shutdown(void);

/**
 * @brief macro to access configuration properties from DTS
 */
#define AP_PWRSEQ_DT_VALUE(p) \
	DT_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq), p)

#endif /* __AP_PWRSEQ_AP_POWER_BOARD_FUNCTIONS_H__ */
