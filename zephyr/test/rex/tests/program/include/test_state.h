/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_REX_TESTS_PROGRAM_INCLUDE_TEST_STATE_H
#define ZEPHYR_TEST_REX_TESTS_PROGRAM_INCLUDE_TEST_STATE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct test_state {
	bool ec_app_main_run;
};

bool rex_predicate_post_main(const void *state);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_TEST_REX_TESTS_PROGRAM_INCLUDE_TEST_STATE_H */
