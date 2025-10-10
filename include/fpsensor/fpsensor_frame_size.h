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
	 * the EC.
	 * * Since initialization involves a fallible I/O operation, the caller
	 * must immediately check is_initialized() after creation.
	 */
	FpFrameSizeCache();

	/**
	 * @brief Checks if the object was successfully populated during
	 * creation.
	 * @return true if the cache was successfully populated from the EC.
	 */
	bool is_initialized() const;

	/**
	 * @brief Looks up the frame size for a given capture type.
	 * * Performs an O(1) lookup on the cached array.
	 * @param capture_type The FP_CAPTURE_TYPE to look up (used as array
	 * index).
	 * @return std::optional<uint32_t> containing the frame size if found
	 * and non-zero, or std::nullopt otherwise.
	 */
	std::optional<uint32_t>
	get_frame_size(enum fp_capture_type capture_type) const;

    private:
	/**
	 * @brief Internal method to execute the EC command and populate the
	 * array.
	 * @return EC_RES_SUCCESS on success, or an EC_ERROR code on failure.
	 */
	int populate_cache();

	// Tracks if the constructor's I/O operation succeeded.
	bool initialized_ = false;

	// Fixed-size array for O(1) lookups. Key is capture_type enum value.
	std::array<uint32_t, FP_CAPTURE_TYPE_MAX> frame_sizes_ = {};
};

// Moving the small implementation here is acceptable for
// performance/simplicity.
inline bool FpFrameSizeCache::is_initialized() const
{
	return initialized_;
}

#endif /* __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H */
