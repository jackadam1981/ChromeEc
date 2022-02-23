/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_PWRSEQ_SM_HANDLER_H__
#define __X86_NON_DSX_COMMON_PWRSEQ_SM_HANDLER_H__

#include <x86_common_pwrseq.h>
#include <kernel.h>
#include <init.h>
#include <zephyr/types.h>

#define DT_DRV_COMPAT	COMPAT_BASE

/* Common device tree configurable attributes */
struct common_pwrseq_config {
	int pch_dsw_pwrok_delay_ms;
	int pch_pm_pwrbtn_delay_ms;
	int pch_rsmrst_delay_ms;
	int wait_signal_timeout_ms;
	int s5_timeout_s;
};

/* The wait time is ~150 msec, allow for safety margin. */
#define IN_PCH_SLP_SUS_WAIT_TIME_MS 250

void power_update_signals(void);
enum power_states_ndsx chipset_pwr_sm_run(
				enum power_states_ndsx curr_state,
				const struct common_pwrseq_config *com_cfg);
void chipset_force_shutdown(enum pwrseq_chipset_shutdown_reason reason,
				const struct common_pwrseq_config *com_cfg);
void chipset_reset(enum pwrseq_chipset_shutdown_reason reason);
void init_chipset_pwr_seq_state(void);
enum power_states_ndsx pwr_sm_get_state(void);
uint32_t pwrseq_get_input_signals(void);
void pwrseq_set_debug_signals(uint32_t signals);
uint32_t pwrseq_get_debug_signals(void);
void apshutdown(void);

extern const char pwrsm_dbg[][25];

#endif /* __X86_NON_DSX_COMMON_PWRSEQ_SM_HANDLER_H__ */
