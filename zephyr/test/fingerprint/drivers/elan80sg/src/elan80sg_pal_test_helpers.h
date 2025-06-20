/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_ELAN80SG_SRC_TEST_HELPERS_H_
#define ZEPHYR_TEST_DRIVERS_ELAN80SG_SRC_TEST_HELPERS_H_

#include <zephyr/kernel.h>

#include <fingerprint_elan80sg_pal.h>

__syscall int elan80sg_elan_usleep(unsigned int us);
__syscall void *elan80sg_elan_malloc(uint32_t size);
__syscall void elan80sg_elan_free(void *data);

#include <zephyr/syscalls/elan80sg_pal_test_helpers.h>

#endif /* ZEPHYR_TEST_DRIVERS_ELAN80SG_SRC_TEST_HELPERS_H_ */
