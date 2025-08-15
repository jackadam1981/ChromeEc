/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros/math/iir_decimator.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(iir_decimator, CONFIG_LOG_DEFAULT_LEVEL);

int iir_decimator_init(struct iir_decimator_t *decimator, int sample_rate_mhz)
{
	if (sample_rate_mhz == 0) {
		return 0;
	}

	__ASSERT_NO_MSG(decimator->optimal_frequency_mhz != 0);

	decimator->decimation_factor = DIV_ROUND_CLOSEST(
		sample_rate_mhz, decimator->optimal_frequency_mhz);

	if (decimator->decimation_factor == 0) {
		decimator->decimation_factor = 1;
	}

	int ret = filter_butterworth_lpf_init(
		&decimator->x, decimator->filter_rank,
		1.0f / decimator->decimation_factor);
	if (ret != 0) {
		LOG_ERR("Failed to initialize decimation filter (%d)", ret);
		return ret;
	}

	memcpy(&decimator->y, &decimator->x, sizeof(struct iir_filter_t));
	memcpy(&decimator->z, &decimator->x, sizeof(struct iir_filter_t));

	decimator->decimation_count = 0;
	LOG_DBG("ODR = %dmHz, decimation=%u", sample_rate_mhz,
		decimator->decimation_factor);
	return 0;
}

bool iir_decimator_step(struct iir_decimator_t *decimator, float *x, float *y,
			float *z)
{
	if (decimator->decimation_factor == 0) {
		return true;
	}

	*x = iir_filter_step(&decimator->x, *x);
	*y = iir_filter_step(&decimator->y, *y);
	*z = iir_filter_step(&decimator->z, *z);

	decimator->decimation_count++;

	if (decimator->decimation_count < decimator->decimation_factor) {
		/* Skip this sample */
		return false;
	}

	decimator->decimation_count = 0;
	return true;
}
