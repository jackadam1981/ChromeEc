/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>

#include "syv682x.h"
#include "test_state.h"

ZTEST(ppc_syv682c, test_board_is_syv682c)
{
	zassert_true(syv682x_board_is_syv682c(0), NULL);
}

ZTEST_SUITE(ppc_syv682c, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
