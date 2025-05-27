/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>

#include <fingerprint/fingerprint_alg.h>

LOG_MODULE_REGISTER(ft9865_bio_alg, LOG_LEVEL_INF);

static int ft9865_algorithm_init(const struct fingerprint_algorithm *const alg)
{
	return 0;
}

static int ft9865_algorithm_exit(const struct fingerprint_algorithm *const alg)
{
	return 0;
}

static int ft9865_enroll_start(const struct fingerprint_algorithm *const alg)
{
	return 0;
}

static int ft9865_enroll_step(const struct fingerprint_algorithm *const alg,
			       const uint8_t *const image, int *completion)
{
	*completion = 100;
	return 0;
}

static int ft9865_enroll_finish(const struct fingerprint_algorithm *const alg,
				 void *templ)
{
	return 0;
}

static int ft9865_match(const struct fingerprint_algorithm *const alg,
			 void *templ, uint32_t templ_count,
			 const uint8_t *const image, int32_t *match_index,
			 uint32_t *update_bitmap)
{
	return 0;
}

const struct fingerprint_algorithm_api ft9865_api = {
	.init = ft9865_algorithm_init,
	.exit = ft9865_algorithm_exit,
	.enroll_start = ft9865_enroll_start,
	.enroll_step = ft9865_enroll_step,
	.enroll_finish = ft9865_enroll_finish,
	.match = ft9865_match,
};

FINGERPRINT_ALGORITHM_DEFINE(ft9865_algorithm, NULL, &ft9865_api);
