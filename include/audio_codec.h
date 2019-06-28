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
 * Scales a S16_LE sample by multiplying scalar.
 */
int16_t audio_codec_s16_scale_and_clip(int16_t orig, uint8_t scalar);


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


/*
 * I2S RX abstract layer
 */

/*
 * Enables I2S RX.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_BUSY if has enabled.
 */
int audio_codec_i2s_rx_enable(void);

/*
 * Disables I2S RX.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_BUSY if has not enabled.
 */
int audio_codec_i2s_rx_disable(void);

/*
 * Sets I2S RX sample depth.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_INVAL if depth does not look good.
 */
int audio_codec_i2s_rx_set_sample_depth(uint8_t depth);

/*
 * Sets I2S RX DAI format.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_INVAL if daifmt does not look good.
 */
int audio_codec_i2s_rx_set_daifmt(uint8_t daifmt);

/*
 * Sets I2S RX BCLK.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_INVAL if bclk does not look good.
 */
int audio_codec_i2s_rx_set_bclk(uint32_t bclk);


/*
 * WoV abstract layer
 */

/*
 * Enables WoV.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_BUSY if has enabled.
 */
int audio_codec_wov_enable(void);

/*
 * Disables WoV.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_BUSY if has not enabled.
 */
int audio_codec_wov_disable(void);

/*
 * Reads the WoV audio data from chip.
 *
 * @buf is the target pointer to put the data.
 * @count is the maximum number of bytes to read.
 *
 * Returns:
 *   -1 if any errors.
 *   0 if no data.
 *   >0 if success.  The returned value denotes number of bytes read.
 */
int32_t audio_codec_wov_read(void *buf, uint32_t count);

/*
 * Enables notification if WoV audio data is available.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_BUSY if has enabled.
 *   EC_ERROR_ACCESS_DENIED if the notifiee has not set.
 */
int audio_codec_wov_enable_notifier(void);

/*
 * Disables WoV data notification.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_UNKNOWN if internal error.
 *   EC_ERROR_BUSY if has not enabled.
 *   EC_ERROR_ACCESS_DENIED if the notifiee has not set.
 */
int audio_codec_wov_disable_notifier(void);

/*
 * Sets the notifiee when WoV data is available.
 *
 * Returns:
 *   None.
 */
void audio_codec_wov_set_read_notifiee(
		void (*cb)(void *priv_data), void *priv_data);

/*
 * Audio buffer for 2 seconds S16_LE, 16kHz, mono.
 */
extern uintptr_t audio_codec_wov_audio_buf_addr;
extern uint32_t audio_codec_wov_audio_buf_len;
extern uint8_t audio_codec_wov_audio_buf_type;

/*
 * Language model buffer for speech-micro.  At least 67KB.
 */
extern uintptr_t audio_codec_wov_lang_buf_addr;
extern uint32_t audio_codec_wov_lang_buf_len;
extern uint8_t audio_codec_wov_lang_buf_type;

/*
 * Task for running WoV.
 */
void audio_codec_wov_task(void *arg);

#endif
