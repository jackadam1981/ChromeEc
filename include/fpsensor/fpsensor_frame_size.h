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
	 * @brief Constructor that attempts to populate the cache by querying
	 * the EC. Since initialization involves a fallible I/O operation, the
	 * caller must immediately check is_initialized() after creation.
	 */
	FpFrameSizeCache();

	/**
	 * @brief Checks if the object was successfully populated during
	 * creation.
	 *
	 * @return true if the cache was successfully populated from the EC.
	 */
	bool is_initialized() const;

	/**
	 * @brief Looks up the frame size for a given capture type.
	 *
	 * @param capture_type The enum fp_capture_type to look up.
	 *
	 * @return std::optional<uint32_t> containing the frame size if found
	 * and non-zero, or std::nullopt otherwise.
	 */
	std::optional<uint32_t>
	get_frame_size(enum fp_capture_type capture_type) const;

    private:
	/**
	 * @brief Internal method to populate the frame size array.
	 *
	 * @return EC_RES_SUCCESS on success, or an EC_ERROR code on failure.
	 */
	int populate_cache();

	bool initialized_ = false;
	std::array<uint32_t, FP_CAPTURE_TYPE_MAX> frame_sizes_ = {};
};

/**
 * @brief Checks if the object was successfully populated during
 * creation.
 *
 * @return true if the cache was successfully populated, false otherwise.
 */
inline bool FpFrameSizeCache::is_initialized() const
{
	return initialized_;
}

#endif /* __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H */
