/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_WOV_H
#define __CROS_EC_WOV_H

struct wov_driver {
	int (*enable)(void);
	int (*disable)(void);
	int32_t (*read)(void *buf, uint32_t count);
	void (*set_callback)(void (*cb)(void *priv_data), void *priv_data);
	/* Optional Operators */
	int (*set_sample_rate)(uint32_t rate);
	int (*set_sample_width)(uint32_t width);
	int (*set_num_channels)(uint32_t num_channels);

	uintptr_t audio_buf_addr;
	uint32_t audio_buf_len;
	uint8_t audio_buf_type;

	uintptr_t lang_buf_addr;
	uint32_t lang_buf_len;
	uint8_t lang_buf_type;
};

/*
 * Register the driver.
 *
 * Returns:
 *   EC_SUCCESS if success.
 *   EC_ERROR_ACCESS_DENIED if driver have registered.
 */
int wov_register_driver(struct wov_driver *driver);

/*
 * Task for running WoV.
 */
void wov_task(void);

#endif /* __CROS_EC_WOV_H */
