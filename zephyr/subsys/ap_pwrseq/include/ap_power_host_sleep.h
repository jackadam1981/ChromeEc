/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_POWER_HOST_SLEEP_H
#define __AP_POWER_HOST_SLEEP_H

#include <ap_power/ap_power_interface.h>
#include <power_host_sleep.h>

void ap_power_set_active_wake_mask(void);

int ap_power_get_lazy_wake_mask(
	enum power_states_ndsx state, host_event_t *mask);

#if CONFIG_AP_PWRSEQ_S0IX
void ap_power_reset_host_sleep_state(void);
#endif /* CONFIG_AP_PWRSEQ_S0IX */

#endif /* __AP_PWRSEQ_HOST_SLEEP_H */
