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


struct audio_codec_dmic_driver {
	int (*set_gain)(uint8_t left, uint8_t right);
	int (*get_gain)(uint8_t *left, uint8_t *right);
};

/*
 * Register DMIC driver.
 *
 * Returns:
 *   EC_SUCCESS if success.
 */
int audio_codec_register_dmic_driver(struct audio_codec_dmic_driver *driver);

#endif
