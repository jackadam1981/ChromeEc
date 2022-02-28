/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <power_host_sleep.h>

void power_chipset_handle_host_sleep_event(
		enum host_sleep_event state,
		struct host_sleep_event_context *ctx)
{
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	ap_power_chipset_handle_host_sleep_event(state, ctx);
#endif
}
