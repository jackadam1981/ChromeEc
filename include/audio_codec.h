/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_AUDIO_CODEC_H
#define __CROS_EC_AUDIO_CODEC_H

#include "stdint.h"

/*
 * Common abstract layer
 */

/*
 * Translates SHM address from EC codec to AP.
 *
 * @ec_addr is the source address.
 * @ap_addr is the destination address.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal errors.
 */
extern int audio_codec_translate_addr_ec_to_ap(
		uintptr_t ec_addr, uintptr_t *ap_addr);

/*
 * Checks capabilitiy of audio codec.
 *
 * Returns:
 *   1 if capable.
 *   0 if not capable.
 */
int audio_codec_capable(uint8_t cap);

/*
 * Registers shared memory.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal errors.
 *   EC_ERROR_INVAL if invalid shm_id.
 *   EC_ERROR_INVAL if invalid cap.
 */
int audio_codec_register_shm(uint8_t shm_id, uint8_t cap,
		uintptr_t addr, uint32_t len, uint8_t type);

#endif
