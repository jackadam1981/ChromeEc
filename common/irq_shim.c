/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file is used for platform/ec implementations of irq_lock and irq_unlock
 * which are defined by Zephyr. Once migration to the Zephyr APIs is complete,
 * this file can be removed and the implementation of Zephyr APIs can be used
 * directly.
 */

#include "task.h"
#include "util.h"

static uint32_t lock_count;

uint32_t irq_lock(void)
{
	interrupt_disable();

	/* Never allow lock_count to overflow. */
	if (lock_count < 0xffffffff)
		++lock_count;

	return lock_count - 1;
}

void irq_unlock(uint32_t key)
{
	lock_count = MIN(lock_count, key);

	/*
	 * Since we're allowing nesting locks, we only actually want to release
	 * the lock if the lock count reached 0.
	 */
	if (!lock_count)
		interrupt_enable();
}
