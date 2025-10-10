/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_console.h"
#include "fpsensor/fpsensor_frame_size.h"

#include <stddef.h>

#include <utility>

// Removed the no-longer-needed includes for <optional> and <vector>
// if they are not used elsewhere in this file. Keeping the standard ones
// from the original if they were implicitly used.

std::unique_ptr<FpFrameSizeCache> FpFrameSizeCache::Create()
{
	std::unique_ptr<FpFrameSizeCache> cache(new FpFrameSizeCache());

	if (cache->populate_cache() == EC_RES_SUCCESS) {
		return cache;
	}

	CPRINTF("FP: WARNING - FpFrameSizeCache failed to initialize from EC.");
	return nullptr;
}

int FpFrameSizeCache::populate_cache()
{
	constexpr size_t buffer_size =
		sizeof(struct ec_response_fp_info_v2) +
		sizeof(struct fp_image_frame_params) * FP_CAPTURE_TYPE_MAX;

	uint8_t buffer[buffer_size];
	auto *info = reinterpret_cast<struct ec_response_fp_info_v2 *>(buffer);

	if (fp_sensor_get_info(info, buffer_size) < 0) {
		return EC_ERROR_UNKNOWN;
	}

	frame_sizes_.fill(0);
	const uint8_t num_types = info->sensor_info.num_capture_types;

	if (num_types > FP_CAPTURE_TYPE_MAX) {
		CPRINTF("FP: ERROR - EC returned %u types, max supported is %u\n",
			num_types, FP_CAPTURE_TYPE_MAX);
		return EC_ERROR_OVERFLOW;
	}

	auto *params = info->image_frame_params;

	for (uint8_t i = 0; i < num_types; ++i) {
		const uint8_t type = params[i].fp_capture_type;
		const uint32_t size = params[i].frame_size;

		if (type < FP_CAPTURE_TYPE_MAX) {
			frame_sizes_[type] = size;
		} else {
			CPRINTF("warning: type ID %u, which is out of bounds (%u).\n",
				type, FP_CAPTURE_TYPE_MAX);
		}
	}

	return EC_RES_SUCCESS;
}

uint32_t
FpFrameSizeCache::get_frame_size(enum fp_capture_type capture_type) const
{
	const uint8_t type_index = static_cast<uint8_t>(capture_type);

	if (type_index >= FP_CAPTURE_TYPE_MAX) {
		return 0;
	}

	return frame_sizes_[type_index];
}
