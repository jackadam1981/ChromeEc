/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Crystal module for Rotor MCU */

#include "common.h"

/**
 * Check to see if the USB controller has detected a  wake event.
 *
 * If a USB wake event occurs, this function will send a notification to the
 * APMU.
 *
 * @param chip	Which Crystal to query.
 * @return EC_SUCCESS on success, otherwise an error.
 */
int crystal_check_usb_wake_evt(uint8_t chip);
