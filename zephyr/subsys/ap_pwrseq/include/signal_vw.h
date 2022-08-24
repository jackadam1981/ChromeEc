/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_VW_H__
#define __AP_PWRSEQ_SIGNAL_VW_H__

#include <zephyr/drivers/espi.h>

#define PWR_SIG_TAG_VW PWR_VW_

/*
 * Generate enums for the virtual wire signals.
 * These enums are only used internally
 * to assign an index to each signal that is specific
 * to the source.
 */

#define TAG_VW(tag, name) DT_CAT(tag, name)

#define PWR_VW_ENUM(id) TAG_VW(PWR_SIG_TAG_VW, PWR_SIGNAL_ENUM(id)),

enum pwr_sig_vw {
#if HAS_VW_SIGNALS
	DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_vw, PWR_VW_ENUM)
#endif
		PWR_SIG_VW_COUNT
};

#undef PWR_VW_ENUM
#undef TAG_VW

/**
 * @brief Get the value of the virtual wire signal.
 *
 * @param index The VW signal index to get.
 * @return the current value of the virtual wire.
 */
int power_signal_vw_get(enum pwr_sig_vw vw);

/**
 * @brief Initialize the power signal interface.
 *
 * Called when the power sequence code is ready to start
 * processing inputs and outputs.
 */
void power_signal_vw_init(void);

/**
 * @brief Send eSPI Virtual Wire GPIO
 *
 * @param signal The Virtual Wire signal
 * @param level Level of Virtual Wire signal to be set
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error, failed to send over the bus.
 */
int espi_vw_set_wire(enum espi_vwire_signal signal, uint8_t level);

#endif /* __AP_PWRSEQ_SIGNAL_VW_H__ */
