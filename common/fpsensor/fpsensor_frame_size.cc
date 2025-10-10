/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_console.h"
#include "fpsensor/fpsensor_frame_size.h"

#include <stddef.h>

#include <memory>
#include <utility>

std::unique_ptr<FpFrameSizeCache>
FpFrameSizeCache::Create(uint32_t max_frame_size_bytes)
{
	std::unique_ptr<FpFrameSizeCache> cache(new FpFrameSizeCache());

	if (!cache->populate_cache(max_frame_size_bytes)) {
		return nullptr;
	}

	return cache;
}

bool FpFrameSizeCache::populate_cache(uint32_t max_frame_size_bytes)
{
	const size_t buffer_size =
		sizeof(struct ec_response_fp_info_v2) +
		sizeof(struct fp_image_frame_params) * frame_sizes_.size();

	auto buffer_ptr = std::make_unique<uint8_t[]>(buffer_size);
	uint8_t *buffer = buffer_ptr.get();

	auto *info = reinterpret_cast<struct ec_response_fp_info_v2 *>(buffer);

	if (fp_sensor_get_info(info, buffer_size) < 0) {
		CPRINTF("Error: Failed to get fingerprint sensor info.");
		return false;
	}

	const uint8_t num_types = info->sensor_info.num_capture_types;

	if (num_types > frame_sizes_.size()) {
		CPRINTF("ERROR - EC returned %u types, max supported is %u.",
			num_types, frame_sizes_.size());
		return false;
	}

	auto *params = info->image_frame_params;

	for (uint8_t i = 0; i < num_types; ++i) {
		const uint8_t type = params[i].fp_capture_type;
		const uint32_t size = params[i].frame_size;

		if (size > max_frame_size_bytes) {
			CPRINTF("Error: Type %u frame size (%u) exceeds max allowed (%u).",
				type, size, max_frame_size_bytes);
			return false;
		}

		if (type < frame_sizes_.size()) {
			frame_sizes_[type] = size;

		} else {
			CPRINTF("ERROR: Invalid fp_capture_type %u received from EC, max "
				"supported is %zu.",
				type, frame_sizes_.size());
			return false;
		}
	}

	return true;
}

uint32_t
FpFrameSizeCache::get_frame_size(enum fp_capture_type capture_type) const
{
	if (capture_type >= frame_sizes_.size() || capture_type < 0) {
		return 0;
	}

	return frame_sizes_[capture_type];
}
