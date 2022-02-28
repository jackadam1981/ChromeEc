/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_POWER_HOST_SLEEP_H
#define __AP_POWER_HOST_SLEEP_H

#include <power_host_sleep.h>

void handle_s0ix_in_chipset_suspend(void);
void power_set_active_wake_mask(void);

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
void power_reset_host_sleep_state(void);
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_S0IX */

#endif /* __AP_PWRSEQ_HOST_SLEEP_H */
