/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_console.h"
#include "fpsensor/fpsensor_frame_size.h"

#include <stddef.h>

#include <optional>
#include <vector>

FpFrameSizeCache::FpFrameSizeCache()
{
	if (populate_cache() == EC_RES_SUCCESS) {
		initialized_ = true;
	} else {
		CPRINTF("FP: WARNING - FpFrameSizeCache failed to initialize from EC.");
		initialized_ = false;
	}
}

int FpFrameSizeCache::populate_cache()
{
	const size_t buffer_size =
		sizeof(struct ec_response_fp_info_v2) +
		sizeof(struct fp_image_frame_params) * FP_MAX_CAPTURE_TYPES;

	std::vector<uint8_t> buffer(buffer_size);
	auto *info = reinterpret_cast<struct ec_response_fp_info_v2 *>(
		buffer.data());

	if (fp_sensor_get_info(info, buffer.size()) < 0) {
		return EC_ERROR_UNKNOWN;
	}

	frame_sizes_.fill(0);
	const uint8_t num_types = info->sensor_info.num_capture_types;

	if (num_types > FP_MAX_CAPTURE_TYPES) {
		CPRINTF("FP: ERROR - EC returned %u types, max supported is %u\n",
			num_types, FP_MAX_CAPTURE_TYPES);
		return EC_ERROR_OVERFLOW;
	}

	auto *params = reinterpret_cast<struct fp_image_frame_params *>(
		reinterpret_cast<uint8_t *>(info) +
		sizeof(struct ec_response_fp_info_v2));

	for (uint8_t i = 0; i < num_types; ++i) {
		const uint8_t type = params[i].fp_capture_type;
		const uint32_t size = params[i].frame_size;

		if (type < FP_CAPTURE_TYPE_MAX) {
			frame_sizes_[type] = size;
		} else {
			CPRINTF("FP: warning: EC returned type ID %u, which is out of bounds (%u).\n",
				type, FP_CAPTURE_TYPE_MAX);
		}
	}

	return EC_RES_SUCCESS;
}

std::optional<uint32_t>
FpFrameSizeCache::get_frame_size(enum fp_capture_type capture_type) const
{
	if (!initialized_) {
		return std::nullopt;
	}

	const uint8_t type_index = static_cast<uint8_t>(capture_type);

	if (type_index >= FP_CAPTURE_TYPE_MAX) {
		return std::nullopt;
	}

	const uint32_t size = frame_sizes_[type_index];

	if (size == 0) {
		return std::nullopt;
	}

	return size;
}
