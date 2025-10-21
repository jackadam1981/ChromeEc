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

ZTEST(fpsensor_frame_size, test_cache_initialization_and_lookup)
{
	auto cache = FpFrameSizeCache::Create();

	zassert_not_null(
		cache.get(),
		"FpFrameSizeCache::Create() failed (returned nullptr).");

	for (const auto &image_frame_params : image_frame_params_arr) {
		uint32_t expected_size = image_frame_params.frame_size;
		enum fp_capture_type type =
			(enum fp_capture_type)image_frame_params.fp_capture_type;

		uint32_t actual_size = cache->get_frame_size(type);

		zassert_equal(expected_size, actual_size,
			      "Mismatch for type %d. Expected: %u, Actual: %u",
			      (int)type, expected_size, actual_size);
	}

	zassert_equal(cache->get_frame_size(FP_CAPTURE_TYPE_MAX), 0,
		      "Expected zero size for invalid capture type (%d).",
		      FP_CAPTURE_TYPE_MAX);
}
