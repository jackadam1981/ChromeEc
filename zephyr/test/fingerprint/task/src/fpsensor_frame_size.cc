/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"

#include <zephyr/fff.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <drivers/fingerprint_sim.h>
#include <fingerprint/v4l2_types.h>
#include <fpsensor/fpsensor_frame_size.h>
#include <mkbp_event.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, mkbp_send_event, uint8_t);

#define IMAGE_SIZE                                                 \
	MAX_FROM_LIST(LISTIFY(NUM_IMAGE_CAPTURE_TYPES,             \
			      FINGERPRINT_SENSOR_FRAME_SIZE, (, ), \
			      DT_CHOSEN(cros_fp_fingerprint_sensor)))
static uint8_t image_buffer[IMAGE_SIZE];

#define FP_SIMULATOR_IMAGE_FRAME_PARAM_INITIALIZER(idx, node_id)            \
	{                                                                   \
		.frame_size = FINGERPRINT_SENSOR_FRAME_SIZE(idx, node_id),  \
		.pixel_format =                                             \
			FINGERPRINT_SENSOR_V4L2_PIXEL_FORMAT(idx, node_id), \
		.width = FINGERPRINT_SENSOR_RES_X(idx, node_id),            \
		.height = FINGERPRINT_SENSOR_RES_Y(idx, node_id),           \
		.bpp = FINGERPRINT_SENSOR_RES_BPP(idx, node_id),            \
		.fp_capture_type =                                          \
			FINGERPRINT_SENSOR_CAPTURE_TYPE(idx, node_id),      \
		.reserved = 0,                                              \
	}

static const struct fingerprint_image_frame_params image_frame_params_arr[] = {
	LISTIFY(NUM_IMAGE_CAPTURE_TYPES,
		FP_SIMULATOR_IMAGE_FRAME_PARAM_INITIALIZER, (, ),
		DT_NODELABEL(fpsensor_sim))
};

ZTEST_SUITE(fpsensor_frame_size, NULL, NULL, NULL, NULL, NULL);

ZTEST(fpsensor_frame_size, test_cache_initialize_size_exceeded)
{
	/*
	 * Set the maximum allowed size to be one byte LESS than the actual size
	 * of the first frame (index 0) to force the validation check inside
	 * FpFrameSizeCache::Create() to fail.
	 */
	uint32_t max_frame_size_bytes =
		image_frame_params_arr[0].frame_size - 1;

	auto cache = FpFrameSizeCache::Create(max_frame_size_bytes);

	zassert_is_null(
		cache.get(),
		"FpFrameSizeCache::Create() must return nullptr when "
		"max_frame_size_bytes is exceeded by a reported frame size.");
}

ZTEST(fpsensor_frame_size, test_cache_initialization_and_lookup)
{
	auto cache = FpFrameSizeCache::Create(sizeof(image_buffer));

	zassert_not_null(
		cache.get(),
		"FpFrameSizeCache::Create() failed (returned nullptr).");

	std::array<bool, FP_CAPTURE_TYPE_MAX> is_type_present;
	is_type_present.fill(false);

	/* Test all types that should be present */
	for (const auto &params : image_frame_params_arr) {
		uint32_t expected_size = params.frame_size;
		enum fp_capture_type type =
			(enum fp_capture_type)(params.fp_capture_type);

		/* 
         * Ensure the type is within the valid index range [0,
		 * FP_CAPTURE_TYPE_MAX - 1].
         */
		zassert_true(
			type >= 0 && type < FP_CAPTURE_TYPE_MAX,
			"fp_capture_type %d is out of expected range [0, %d).",
			type, FP_CAPTURE_TYPE_MAX);

		if (type >= 0 && type < FP_CAPTURE_TYPE_MAX) {
			zassert_true(
				!is_type_present[type],
				"Duplicate entry found for fp_capture_type %d in "
				"image_frame_params_arr.",
				static_cast<int>(type));
			is_type_present[type] = true;

			uint32_t actual_size = cache->get_frame_size(type);
			zassert_equal(
				expected_size, actual_size,
				"Mismatch for type %d. Expected: %u, Actual: %u",
				static_cast<int>(type), expected_size,
				actual_size);
		}
	}

	/* Validate all possible fp_capture_types (0 to FP_CAPTURE_TYPE_MAX). */
	for (int i = 0; i <= FP_CAPTURE_TYPE_MAX; ++i) {
		enum fp_capture_type type =
			static_cast<enum fp_capture_type>(i);
		uint32_t actual_size = cache->get_frame_size(type);

		if (i < FP_CAPTURE_TYPE_MAX && is_type_present[i]) {
			zassert_true(
				actual_size > 0,
				"Type %d was expected to be valid but returned zero size.",
				i);
		} else {
			zassert_equal(
				0, actual_size,
				"Type %d was not expected to be valid but returned non-zero "
				"size %u.",
				i, actual_size);
		}
	}

	/* Validate negative value. */
	zassert_equal(cache->get_frame_size((enum fp_capture_type)(-1)), 0,
		      "Expected zero size for negative capture type (-1).");
}
