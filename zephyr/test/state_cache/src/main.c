/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "state_cache.h"

#include <zephyr/device.h>
#include <zephyr/fff.h>
#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test.h>

ZTEST_SUITE(state_cache, NULL, NULL, NULL, NULL, NULL);

STATE_CACHE_BIND(CURRENT_TASK, uint8_t, current_task_03) = 3;
STATE_CACHE_BIND(CURRENT_TASK, uint8_t, current_task_00) = 0;
STATE_CACHE_BIND(CURRENT_TASK, uint8_t, current_task_01) = 1;
STATE_CACHE_BIND(CURRENT_TASK, uint8_t, current_task_02) = 2;
STATE_CACHE_BIND(CURRENT_TASK, uint8_t, current_task_10) = 10;

STATE_CACHE_BIND(LAST_HOOK, uint8_t, last_hook) = 10;

uint32_t flags = 100;
STATE_CACHE_BIND_PTR(RESET_FLAGS, uint32_t, reset_flags) = &flags;

ZTEST(state_cache, test_dump)
{
    reset_flags = &flags;
	state_cache_dump();
    last_hook = 50;
    current_task_00*=2;
    current_task_01*=2;
    current_task_02*=2;
    current_task_03*=2;
    current_task_10*=2;
    flags = 50;
    state_cache_dump();
}
#if 0
ZTEST(state_cache, test_pack)
{
	uint8_t buffer[256];
	size_t size = state_cache_pack(buffer, sizeof(buffer));
	zassert_true(size > 0, NULL);
    printk("size: %d\n", size);
}

ZTEST(state_cache, test_unpack)
{
	uint8_t buffer[256];
	size_t size = state_cache_pack(buffer, sizeof(buffer));
	zassert_true(size > 0, NULL);
	int rv = state_cache_dump_packed(buffer, size);
    zassert_true(rv == 0, NULL);
}
#endif
