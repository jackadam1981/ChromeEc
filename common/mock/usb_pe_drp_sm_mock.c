/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Mock USB Policy Engine Sink / Source module */

#include <stdint.h>
#ifndef CONFIG_COMMON_RUNTIME
#define cprints(format, args...)
#endif

#ifndef TEST_BUILD
#error "Mocks should only be in the test build."
#endif

int pd_dev_store_rw_hash(int port, uint16_t dev_id, uint32_t *rw_hash,
			 uint32_t current_image)
{
	return 0;
}
