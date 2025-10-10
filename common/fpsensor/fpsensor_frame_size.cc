/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_console.h"
#include "fpsensor/fpsensor_frame_size.h"

#include <stddef.h>

#include <optional>

FpFrameSizeCache::FpFrameSizeCache() {
    if (populate_cache() == EC_RES_SUCCESS) {
        initialized_ = true;
    }
}

std::unique_ptr<FpFrameSizeCache> FpFrameSizeCache::Create() {
    std::unique_ptr<FpFrameSizeCache> cache(new FpFrameSizeCache());

    if (!cache->is_initialized()) {
        return nullptr; 
    }

    return cache;
}

int FpFrameSizeCache::populate_cache()
{
	const size_t buffer_size =
		sizeof(ec_response_fp_info_v2) +
		sizeof(fp_image_frame_params) * FP_MAX_CAPTURE_TYPES;

	std::vector<uint8_t> buffer(buffer_size);
	auto *info = reinterpret_cast<ec_response_fp_info_v2 *>(buffer.data());

	if (fp_sensor_get_info(info, buffer.size()) < 0) {
		return EC_ERROR_UNKNOWN;
	}

	frame_sizes_.fill(0);
	const uint8_t num_types = info->sensor_info.num_capture_types;

	if (num_types > FP_MAX_CAPTURE_TYPES) {
		return EC_ERROR_OVERFLOW;
	}

	auto *params = reinterpret_cast<fp_image_frame_params *>(
		reinterpret_cast<uint8_t *>(info) +
		sizeof(ec_response_fp_info_v2));

	for (uint8_t i = 0; i < num_types; ++i) {
		const uint8_t type = params[i].fp_capture_type;
		const uint32_t size = params[i].frame_size;

		if (type < FP_CAPTURE_TYPE_MAX) {
			frame_sizes_[type] = size;
		} else {
			CPRINTF("warning: EC returned a type ID we don't have space for.");
		}
	}
	return EC_RES_SUCCESS;
}

std::optional<uint32_t>
FpFrameSizeCache::get_frame_size(enum fp_capture_type capture_type) const
{
	if (capture_type >= FP_CAPTURE_TYPE_MAX) {
		return std::nullopt;
	}

	const uint32_t size = frame_sizes_[capture_type];
	if (size == 0) {
		return std::nullopt;
	}

	return size;
}
