/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Hylia DP functions
 */

#include "gpio_signal.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hylia_usbc, LOG_LEVEL_DBG);

void hdmi_hpd_interrupt(enum gpio_signal signal)
{
	/* TODO(b:380121396): implement me */
}

void hylia_dp_attention(int port, uint32_t vdo_dp_status)
{
	/* TODO(b:380121396): implement me */
}

void hylia_set_unattached(int port)
{
	/* TODO(b:380121396): implement me */
}
