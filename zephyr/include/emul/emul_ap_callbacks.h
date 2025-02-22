/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EMUL_AP_CALLBACKS_H_
#define EMUL_AP_CALLBACKS_H_

#include <ap_power/ap_pwrseq.h>

#define AP_PWRSEQ_STATES_MASK GENMASK(AP_POWER_STATE_COUNT - 1, 0)

struct ap_pwrseq_cb_list {
	uint32_t states;
	sys_slist_t list;
	struct k_spinlock lock;
};

/**
 * @brief Emulate sending AP power sequence entry callback.
 *
 * @param dev Pointer of AP power sequence device driver.
 * @param new State that you want to emulate entering.
 * @param curr State that you want to emulate leaving.
 *
 * @retval None.
 **/
void ap_pwrseq_emul_entry_callback(const struct device *dev,
				   enum ap_pwrseq_state new,
				   enum ap_pwrseq_state curr);
/**
 * @brief Emulate sending AP power sequence entry callback.
 *
 * @param dev Pointer of AP power sequence device driver.
 * @param new State that you want to emulate exiting.
 * @param curr State that you want to emulate entering.
 *
 * @retval None.
 **/
void ap_pwrseq_emul_exit_callback(const struct device *dev,
				   enum ap_pwrseq_state curr,
				   enum ap_pwrseq_state new);
#endif /* EMUL_AP_CALLBACKS_H_ */
