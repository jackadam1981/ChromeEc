/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Internal API for sending AP power related event callbacks.
 */

#ifndef __AP_POWER_AP_EVENTS_H__
#define __AP_POWER_AP_EVENTS_H__

#include <ap_power/ap_power.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dispatch callbacks for an event.
 *
 * @param event The event to invoke callbacks for.
 */
void ap_power_ev_send_callbacks(enum ap_power_events event);

#ifdef __cplusplus
}
#endif

#endif /* __AP_POWER_AP_EVENTS_H__ */
