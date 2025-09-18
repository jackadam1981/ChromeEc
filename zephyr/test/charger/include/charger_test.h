/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_TEST_CHARGER_INCLUDE_MAIN_H
#define __ZEPHYR_TEST_CHARGER_INCLUDE_MAIN_H

#include <stdbool.h>

bool charger_predicate_pre_main(const void *state);
bool charger_predicate_post_main(const void *state);

#endif /* __ZEPHYR_TEST_CHARGER_INCLUDE_MAIN_H */