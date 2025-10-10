/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H
#define __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H

#include "ec_commands.h"

#include <array>
#include <cstdint>

class FpFrameSizeCache {
    public:
	/**
	 * @brief Public constructor.
	 *
	 * It attempts to populate the cache by querying the EC. The result of
	 * the initialization can be checked with is_initialized().
	 *
	 * @param max_frame_size_bytes The maximum allowable size for any
	 * fingerprint frame, used for validation during population.
	 */
	explicit FpFrameSizeCache(uint32_t max_frame_size_bytes);

	/**
	 * @brief Checks if the cache was successfully initialized/populated.
	 *
	 * @return true if the cache was successfully populated during
	 * construction, false otherwise.
	 */
	bool is_initialized() const;

	/**
	 * @brief Looks up the frame size for a given capture type.
	 *
	 * NOTE: Callers should check is_initialized() before calling this,
	 * or handle the case where the returned size is 0 due to an
	 * uninitialized cache.
	 *
	 * @param capture_type The enum fp_capture_type to look up.
	 *
	 * @return The frame size (uint32_t) if found, or 0 otherwise (e.g., for
	 * an invalid type or if size is 0, or if the cache is uninitialized).
	 */
	uint32_t get_frame_size(enum fp_capture_type capture_type) const;

    private:
	/* No copying or moving of this object. */
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
	 * @retval true  On successful population of the cache.
	 * @retval false On failure to populate the cache.
	 */
	bool populate_cache(uint32_t max_frame_size_bytes);

	std::array<uint32_t, FP_CAPTURE_TYPE_MAX> frame_sizes_ = {};
	bool is_initialized_ = false;
};

#endif /* __CROS_EC_FPSENSOR_FPSENSOR_FRAME_SIZE_H */
