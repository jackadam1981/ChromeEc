/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H
#define __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H

#include "ec_commands.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

class FpFrameSizeCache {
    public:
	/**
	 * @brief Returns a valid pointer on success, or nullptr on failure..
	 */
	static std::unique_ptr<FpFrameSizeCache> Create();

	/**
	 * @brief Checks if the object was successfully populated during
	 * creation.
	 */
	bool is_initialized() const
	{
		return initialized_;
	}

	/**
	 * @brief Looks up the frame size for a given capture type.
	 */
	std::optional<uint32_t>
	get_frame_size(enum fp_capture_type capture_type) const;

    private:
	FpFrameSizeCache();
	int populate_cache();

	bool initialized_ = false;
	std::array<uint32_t, FP_CAPTURE_TYPE_MAX> frame_sizes_ = {};
};

// Global instance (to be defined in the .cc file)
extern std::unique_ptr<FpFrameSizeCache> fpsensor_frame_size_cache;

#endif /* __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H */
