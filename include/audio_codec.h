/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_AUDIO_CODEC_H
#define __CROS_EC_AUDIO_CODEC_H

#include "console.h"

#define ERR(format, args...) \
	cprintf(CC_AUDIO_CODEC, "[ERR] " format "\n", ##args)
#define WARN(format, args...) \
	cprintf(CC_AUDIO_CODEC, "[WARN] " format "\n", ##args)

#ifdef DEBUG_AUDIO_CODEC
#define DBG(format, args...) \
do { \
	cprintf(CC_AUDIO_CODEC, "[DBG] " format " (%s:%d)\n", ##args, \
		__FILE__, __LINE__); \
	cflush(); \
} while (0)
#else
#define DBG(format, args...)
#endif /* DEBUG_AUDIO_CODEC */


struct audio_codec_driver {
	int (*translate_addr_ec_to_ap)(uintptr_t ec_addr, uintptr_t *ap_addr);
};

/*
 * Check capabilitiy of audio codec.
 *
 * Returns:
 *   1 if capable.
 *   0 if not capable.
 */
int audio_codec_capable(uint8_t cap);

/*
 * Register shared memory.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_INVAL if invalid shm_id.
 *   EC_ERROR_INVAL if invalid cap.
 */
int audio_codec_register_shm(uint8_t shm_id, uint8_t cap,
		uintptr_t addr, uint32_t len, uint8_t type);

/*
 * Register driver.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_register_driver(struct audio_codec_driver *driver);


struct audio_codec_dmic_driver {
	uint8_t max_gain;
	int (*get_max_gain)(uint8_t *max_gain);
	int (*set_gain_idx)(uint8_t channel, uint8_t gain);
	int (*get_gain_idx)(uint8_t channel, uint8_t *gain);
};

/*
 * Register DMIC driver.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_register_dmic_driver(struct audio_codec_dmic_driver *driver);

/*
 * Default get_max_gain().
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_get_max_gain(uint8_t *gain);

/*
 * Default set_gain_idx() for software gain on DMIC.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_set_gain_idx(uint8_t channel, uint8_t gain);

/*
 * Default get_gain_idx() for software gain on DMIC.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_get_gain_idx(uint8_t channel, uint8_t *gain);


struct audio_codec_i2s_rx_driver {
	int (*enable)(void);
	int (*disable)(void);

	int (*set_sample_depth)(uint8_t depth);
	int (*set_daifmt)(uint8_t daifmt);
	int (*set_bclk)(uint32_t bclk);
	int (*set_tdm_config)(void);
};

/*
 * Register I2S RX driver.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_register_i2s_rx_driver(
	struct audio_codec_i2s_rx_driver *driver);


struct audio_codec_wov_driver {
	int (*enable)(void);
	int (*disable)(void);

	int32_t (*read)(void *buf, uint32_t count);
	int (*enable_notifier)(void);
	int (*disable_notifier)(void);
	void (*set_read_notifiee)(void (*cb)(void *priv_data), void *priv_data);

	uintptr_t audio_buf_addr;
	uint32_t audio_buf_len;
	uint8_t audio_buf_type;

	uintptr_t lang_buf_addr;
	uint32_t lang_buf_len;
	uint8_t lang_buf_type;
};

/*
 * Register WoV driver.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_register_wov_driver(struct audio_codec_wov_driver *driver);

/*
 * Task for running WoV.
 */
void audio_codec_wov_task(void *arg);

#endif
