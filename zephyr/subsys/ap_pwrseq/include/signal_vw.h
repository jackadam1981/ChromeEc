/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_VW_H__
#define __AP_PWRSEQ_SIGNAL_VW_H__

/**
 * @brief Get the value of the virtual wire signal.
 *
 * @param index The VW signal index to get.
 * @return the current value of the virtual wire.
 */
int power_signal_vw_get(int vw);

/**
 * @brief Initialize the power signal interface.
 *
 * Called when the power sequence code is ready to start
 * processing inputs and outputs.
 */
void power_signal_vw_init(void);

#endif /* __AP_PWRSEQ_SIGNAL_VW_H__ */
