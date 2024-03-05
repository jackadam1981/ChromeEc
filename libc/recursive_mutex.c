/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "builtin/assert.h"
#include "recursive_mutex.h"
#include "system.h"
#include "task.h"

/* Recursive mutex implementation */

void mutex_init_recursive(struct mutex_r *mtx)
{
	ASSERT(mtx != NULL);

	mtx->owner = -1;
	mtx->count = 0;
	mtx->waiters = 0;
}

void mutex_lock_recursive(struct mutex_r *mtx)
{
	atomic_val_t owner, current;

	ASSERT(mtx != NULL);
	ASSERT(!in_interrupt_context());

	owner = -1;
	current = task_get_current();

	atomic_or(&mtx->waiters, 1 << current);
	while (!atomic_compare_exchange(&mtx->owner, &owner, current) &&
	       owner != current) {
		owner = -1;
		task_wait_event_mask(TASK_EVENT_MUTEX, 0);
	}

	/* Only owner can escape while loop above. */

	/* We are not waiting for this mutex, so remove from waiters. */
	atomic_clear_bits(&mtx->waiters, 1 << current);
	mtx->count++;
}

int mutex_try_lock_recursive(struct mutex_r *mtx)
{
	atomic_val_t owner, current;

	ASSERT(mtx != NULL);
	ASSERT(!in_interrupt_context());

	owner = -1;
	current = task_get_current();

	if (!atomic_compare_exchange(&mtx->owner, &owner, current) &&
	    owner != current) {
		return 0;
	}

	/* Only owner can escape if above. */

	mtx->count++;

	return 1;
}

void mutex_unlock_recursive(struct mutex_r *mtx)
{
	uint32_t waiters;

	ASSERT(mtx != NULL);
	ASSERT(!in_interrupt_context());

	/* Panic if not called by owner */
	ASSERT(mtx->owner == task_get_current());
	ASSERT(mtx->count > 0);

	mtx->count--;
	if (mtx->count == 0) {
		mtx->owner = -1;
		waiters = atomic_load(&mtx->waiters);
		if (waiters) {
			task_set_event(__fls(waiters), TASK_EVENT_MUTEX);
		}
	}
}
