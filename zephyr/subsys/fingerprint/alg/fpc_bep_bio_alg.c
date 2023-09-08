/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpc_bep_bio_alg.h"

#include <zephyr/logging/log.h>

#include <fingerprint/fingerprint_alg.h>

LOG_MODULE_REGISTER(fpc1025_bio_alg, LOG_LEVEL_INF);

/*
 * This file contains structures required by FPC library (algorithm) and
 * implementation of algorithm API
 */

extern const fpc_bep_algorithm_t fpc_bep_algorithm_pfe_1025;

const fpc_bio_info_t fpc_bio_info = {
	.algorithm = &fpc_bep_algorithm_pfe_1025,
	.template_size = CONFIG_FP_ALGORITHM_TEMPLATE_SIZE,
};

/*
 * Constant value for the enrollment data size
 *
 * Size of private fp_bio_enrollment_t
 */
#define FP_ALGORITHM_ENROLLMENT_SIZE (4)

static uint32_t enroll_ctx;
BUILD_ASSERT(sizeof(enroll_ctx) == FP_ALGORITHM_ENROLLMENT_SIZE,
	     "Wrong enroll_ctx size");

int fingerprint_algorithm_init(void)
{
	int rc;

	rc = bio_algorithm_init();
	if (rc < 0) {
		LOG_ERR("bio_algorithm_init() failed, result %d", rc);
		return -EINVAL;
	}

	return 0;
}

int fingerprint_algorithm_exit(void)
{
	int rc;

	rc = bio_algorithm_exit();
	if (rc < 0) {
		LOG_ERR("bio_algorithm_exit() failed, result %d", rc);
		return -EINVAL;
	}

	return 0;
}

int fingerprint_enroll_start(void)
{
	int rc;
	bio_enrollment_t bio_enroll = &enroll_ctx;

	rc = bio_enrollment_begin(&bio_enroll);
	if (rc < 0) {
		LOG_ERR("bio_enrollment_begin() failed, result %d", rc);
		return -EINVAL;
	}

	return 0;
}

int fingerprint_enroll_step(uint8_t *image, int *completion)
{
	int rc;
	bio_enrollment_t bio_enroll = &enroll_ctx;

	rc = bio_enrollment_add_image(bio_enroll, image);
	if (rc < 0) {
		LOG_ERR("bio_enrollment_add_image() failed, result %d", rc);
		return -EINVAL;
	}

	*completion = bio_enrollment_get_percent_complete(bio_enroll);

	/*
	 * FP_ENROLLMENT_* are synchronized with BIO_ENROLLMENT_*, so there is
	 * no need to translate codes.
	 */

	return rc;
}

int fingerprint_enroll_finish(void *templ)
{
	int rc;
	bio_enrollment_t bio_enroll = &enroll_ctx;
	bio_template_t bio_templ = templ;

	rc = bio_enrollment_finish(bio_enroll, templ ? &bio_templ : NULL);
	if (rc < 0) {
		LOG_ERR("bio_enrollment_finish() failed, result %d", rc);
		return -EINVAL;
	}

	return 0;
}

int fingerprint_match(void *templ, uint32_t templ_count, uint8_t *image,
		      int32_t *match_index, uint32_t *update_bitmap)
{
	int rc;

	rc = bio_template_image_match_list(templ, templ_count, image,
					   match_index, update_bitmap);
	if (rc < 0) {
		LOG_ERR("bio_template_image_match_list() failed, result %d",
			rc);
		return -EINVAL;
	}

	/*
	 * FP_TEMPLATE_* are synchronized with BIO_TEMPLATE_*, so there is no
	 * need to translate codes.
	 */

	return rc;
}
