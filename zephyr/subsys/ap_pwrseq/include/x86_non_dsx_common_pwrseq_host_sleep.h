/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_PWRSEQ_HOST_SLEEP_H__
#define __X86_NON_DSX_COMMON_PWRSEQ_HOST_SLEEP_H__

#include <ap_power_host_sleep.h>

#ifdef CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY

/**
 * Handles if sleep hang is detected.
 *
 * It is called in sleep_transition_timeout(struct k_work *work), once timeout
 * happens after sleep hang is detected.
 *
 *
 * @param hang_type- type of hang detected.
 */
void power_chipset_handle_sleep_hang(enum sleep_hang_type hang_type);

#endif /* !CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY */

#endif /* __X86_NON_DSX_COMMON_PWRSEQ_HOST_SLEEP_H__ */
