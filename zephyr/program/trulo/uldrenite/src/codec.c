/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "timer.h"

static void ecmute_off(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_mute_l), 1);
}
DECLARE_DEFERRED(ecmute_off);

/* When system startup, the EC waits for the HDA_RST signal in the Codec to
 * become active before disabling the mute. Since the HDA_RST signal is not
 * connected to the EC, a 7-second delay, determined from waveform analysis, is
 * implemented to ensure the EC mute is released after HDA_RST is active.
 */
static void ecmute_off_delay(void)
{
	hook_call_deferred(&ecmute_off_data, 7 * USEC_PER_SEC);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, ecmute_off_delay, HOOK_PRIO_DEFAULT);

/* When system shutdown, the EC enables EC mute function to prevent the Codec
 * from emitting unexpected noise.
 */

static void ecmute_on(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_mute_l), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, ecmute_on, HOOK_PRIO_DEFAULT);
