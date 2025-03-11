/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_EGIS630_SRC_TEST_HELPERS_H_
#define ZEPHYR_TEST_DRIVERS_EGIS630_SRC_TEST_HELPERS_H_

#include <zephyr/kernel.h>

#include <fingerprint_egis630_pal.h>

__syscall uint64_t egis630_plat_get_time(void);
__syscall void egis630_plat_wait_time(uint32_t mecs);
__syscall void egis630_plat_sleep_time(uint32_t timeInMs);
__syscall uint32_t egis630_plat_get_diff_time(uint64_t begin);

#include <zephyr/syscalls/egis630_pal_test_helpers.h>

#endif /* ZEPHYR_TEST_DRIVERS_EGIS630_SRC_TEST_HELPERS_H_ */
