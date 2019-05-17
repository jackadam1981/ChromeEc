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
int audio_codec_translate_addr_ec_to_ap(
		uintptr_t ec_addr, uintptr_t *ap_addr);

/*
 * Checks capability of audio codec.
 *
 * Returns:
 *   1 if audio codec capabilities include cap passed as parameter.
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


/*
 * DMIC abstract layer
 */

/*
 * Gets the maximum possible gain value.  All channels share the same maximum
 * gain value [0, max].
 *
 * The gain has no unit and should fit in a scale to represent relative dB.
 *
 * For example, suppose maximum possible gain value is 4, one could define a
 * mapping:
 * - 0 => -10 dB
 * - 1 => -5 dB
 * - 2 => 0 dB
 * - 3 => 5 dB
 * - 4 => 10 dB
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 */
int audio_codec_dmic_get_max_gain(uint8_t *max_gain);

/*
 * Sets the microphone gain for the specified channel.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_INVAL if channel does not look good.
 *   EC_ERROR_INVAL if gain does not look good.
 */
int audio_codec_dmic_set_gain_idx(uint8_t channel, uint8_t gain);

/*
 * Gets the microphone gain of the specified channel.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_INVAL if channel does not look good.
 */
int audio_codec_dmic_get_gain_idx(uint8_t channel, uint8_t *gain);

#endif
