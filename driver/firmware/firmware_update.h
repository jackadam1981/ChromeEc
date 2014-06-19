/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Firmware Update Driver.
 */

#ifndef __EC_FIRMWARE_UPDATE__
#define __EC_FIRMWARE_UPDATE__

#define NUM_OF_EC_FIRMWARE 1

/**
 * Init firmware Update Initial State
 *
 * @return NONE
 */
void ec_firmware_update_state_init(void);

/**
 * Check if a Firmware Update is inprogress.
 *
 * @return 1 if YES, 0 if NO.
 */
int ec_firmware_update_is_inprogress(void);

#endif
