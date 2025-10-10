/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H
#define __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H

#include "ec_commands.h"

#include <array>
#include <cstdint>
#include <memory>

class FpFrameSizeCache {
    public:
	/**
	 * @brief Static factory method to create an FpFrameSizeCache instance.
	 *
	 * It attempts to populate the cache by querying the EC.
	 *
	 * @param max_frame_size_bytes The maximum allowable size for any
	 * fingerprint frame, used for validation during population.
	 *
	 * @return A std::unique_ptr<FpFrameSizeCache> on success, or nullptr
	 * on failure (e.g., if querying the EC fails).
	 */
	static std::unique_ptr<FpFrameSizeCache>
	Create(uint32_t max_frame_size_bytes);

	/**
	 * @brief Looks up the frame size for a given capture type.
	 *
	 * This object is guaranteed to be initialized if it was successfully
	 * created via the static Create() method.
	 *
	 * @param capture_type The enum fp_capture_type to look up.
	 *
	 * @return The frame size (uint32_t) if found, or 0 otherwise (e.g., for
	 * an invalid type or if size is 0).
	 */
	uint32_t get_frame_size(enum fp_capture_type capture_type) const;

    private:
	/**
	 * @brief Private constructor to enforce construction via Create().
	 */
	FpFrameSizeCache() = default;

	// No copying or moving of this object.
	FpFrameSizeCache(const FpFrameSizeCache &) = delete;
	FpFrameSizeCache &operator=(const FpFrameSizeCache &) = delete;
	FpFrameSizeCache(FpFrameSizeCache &&) = delete;
	FpFrameSizeCache &operator=(FpFrameSizeCache &&) = delete;

	/**
	 * @brief Internal method to populate the frame size array.
	 *
	 * @param max_frame_size_bytes The maximum allowable size for any
	 * fingerprint frame, used for validation.
	 *
	 * @retval true  On successful population of the cache.
	 * @retval false On failure to populate the cache.
	 */
	bool populate_cache(uint32_t max_frame_size_bytes);

	std::array<uint32_t, FP_CAPTURE_TYPE_MAX> frame_sizes_ = {};
};

#endif /* __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H */
