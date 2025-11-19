/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_ASCP_H_
#define ZEPHYR_INCLUDE_ASCP_H_

#include <errno.h>
#include <stdint.h>

#include <zephyr/sys/iterable_sections.h>

struct ec_response_fp_ascp_claim;

/** ASCP data access API. */
struct ascp_api {
	int (*get_claim)(struct ec_response_fp_ascp_claim *res);
	uint8_t *(*get_sk_f)(void);
};

/**
 * Get pointer to ASCP API instance.
 *
 * @return Pointer to the algorithm instance.
 */
static inline const struct ascp_api *ascp_api_get()
{
	const struct ascp_api *alg;
	int cnt;

	STRUCT_SECTION_COUNT(ascp_api, &cnt);
	__ASSERT_NO_MSG(cnt == 1);

	STRUCT_SECTION_GET(ascp_api, 0, &alg);

	return alg;
}

#endif /* ZEPHYR_INCLUDE_ASCP_H_ */
