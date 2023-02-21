/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ap_power/ap_power.h>

#include "ap_power/ap_power_events.h"
#include "mock/ap_power_events.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(mock_ap_power_events);

void ap_power_ev_send_callbacks_custom_fake(enum ap_power_events event)
{
}
