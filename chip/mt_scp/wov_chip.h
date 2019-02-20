/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_WOV_CHIP_H
#define __CROS_EC_WOV_CHIP_H

#include "common.h"

#define WOV_SAMPLERATE_16K 16000
#define WOV_SAMPLERATE_32K 32000

enum wov_mic_type {
	WOV_MICTYPE_AMIC,
	WOV_MICTYPE_DMIC,
	WOV_MICTYPE_DMIC_LP,
};

struct wov_driver {
	void (*enable)(int en);
	size_t (*get_fifo_level)(void);
	size_t (*read_fifo)(uint16_t *output_buffer, size_t max_read_size);
};

struct wov_config {
	enum wov_mic_type mic_type;
	uint16_t samplerate;
	void (*fifo_notify)(size_t fifo_level);
};

/* Initialize WoV hardware and get driver instance. */
struct wov_driver const *wov_driver_init(const struct wov_config *cfg);

#endif /* __CROS_EC_WOV_CHIP_H */
