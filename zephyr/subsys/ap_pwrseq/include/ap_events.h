/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Internal API for sending AP event callbacks.
 */

#ifndef __AP_PWRSEQ_INCLUDE_AP_EVENTS_H__
#define __AP_PWRSEQ_INCLUDE_AP_EVENTS_H__

/**
 * @brief Add an AP event callback.
 *
 * @param callback A valid ap_ev_callback structure pointer.
 * @return 0 on success, negative errno on failure.
 */
void ap_ev_send_callbacks(enum ap_events event);

#endif /* __AP_PWRSEQ_INCLUDE_AP_EVENTS_H__ */
