/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_FINGERPRINT_FINGERPRINT_ALG_H_
#define ZEPHYR_INCLUDE_FINGERPRINT_FINGERPRINT_ALG_H_

#include <stdint.h>

int fingerprint_algorithm_init(void);

int fingerprint_algorithm_exit(void);

int fingerprint_enroll_start(void);

#define FP_ENROLLMENT_OK 0
#define FP_ENROLLMENT_LOW_QUALITY 1
#define FP_ENROLLMENT_IMMOBILE 2
#define FP_ENROLLMENT_LOW_COVERAGE 3
#define FP_ENROLLMENT_INTERNAL_ERROR 5
int fingerprint_enroll_step(uint8_t *image, int *completion);

int fingerprint_enroll_finish(void *templ);

#define FP_TEMPLATE_NO_MATCH 0
#define FP_TEMPLATE_MATCH 1
#define FP_TEMPLATE_MATCH_UPDATED 3
#define FP_TEMPLATE_MATCH_UPDATE_FAILED 5
#define FP_TEMPLATE_LOW_QUALITY 2
#define FP_TEMPLATE_LOW_COVERAGE 4

int fingerprint_match(void *templ, uint32_t templ_count, uint8_t *image,
		      int32_t *match_index, uint32_t *update_bitmap);

#endif /* ZEPHYR_INCLUDE_FINGERPRINT_FINGERPRINT_ALG_H_ */
