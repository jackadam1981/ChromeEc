/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 *@brief Provides implementations of syscalls needed by libc. The newlib
 * documentation provides a list of the required syscalls:
 * https://sourceware.org/newlib/libc.html#Syscalls and minimal implementations
 * can be found in newlib's "nosys" library:
 * https://sourceware.org/git/?p=newlib-cygwin.git;a=tree;f=libgloss/libnosys.
 */

#include "builtin/assert.h"
#include "gettimeofday.h"
#include "link_defs.h"
#include "panic.h"
#include "shared_mem.h"
#include "software_panic.h"
#include "system.h"
#include "task.h"
#include "uart.h"

#include <errno.h>
#include <stdlib.h>

#include <sys/stat.h>

/**
 * Reboot the system.
 *
 * This function is called from libc functions such as abort() or exit().
 *
 * @param rc exit code
 */
void _exit(int rc)
{
	panic_printf("%s called with rc: %d\n", __func__, rc);
	software_panic(PANIC_SW_EXIT, task_get_current());
}

/**
 * Write to the UART.
 *
 * This function is called from libc functions such as printf().
 *
 * @param fd  ignored
 * @param buf buffer to write
 * @param len number of bytes in @buf to write
 * @return number of bytes successfully written
 */
int _write(int fd, char *buf, int len)
{
	return uart_put(buf, len);
}

/**
 * Create a directory.
 *
 * @warning Not implemented.
 *
 * @note Unlike the other functions in this file, this is not overriding a
 * stub version in libnosys. There's no "_mkdir" stub in libnosys, so this
 * provides the actual "mkdir" function.
 *
 * @param pathname directory to create
 * @param mode mode for the new directory
 * @return -1 (error) always
 */
int mkdir(const char *pathname, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

/**
 * Get the time.
 *
 * This function is called from the libc gettimeofday() function.
 *
 * @warning This does not match gettimeofday() exactly; it does not return the
 * time since the Unix Epoch.
 *
 * @param[out] tv time
 * @param[in] tz ignored
 * @return 0 on success
 * @return -1 on error (errno is set to indicate error)
 */
int _gettimeofday(struct timeval *restrict tv, void *restrict tz)
{
	enum ec_error_list ret = ec_gettimeofday(tv, tz);

	if (ret == EC_ERROR_INVAL) {
		errno = EFAULT;
		return -1;
	}

	return 0;
}

/**
 * Change program's data space by increment bytes.
 *
 * This function is called from the libc sbrk() function (which is in turn
 * called from malloc() when memory needs to be allocated or released).
 *
 * @param incr[in] amount to increment or decrement. 0 means return current
 * program break.
 * @return the previous program break (address) on success
 * @return (void*)-1 on error and errno is set to ENOMEM.
 */
void *_sbrk(intptr_t incr)
{
	static char *heap_end = __shared_mem_buf;
	char *prev_heap_end;

	if ((heap_end + incr < __shared_mem_buf) ||
	    (heap_end + incr > (__shared_mem_buf + shared_mem_size()))) {
		errno = ENOMEM;
		return (void *)-1;
	}

	prev_heap_end = heap_end;
	heap_end += incr;

	return (void *)prev_heap_end;
}

/*
 * Newlib Retargetable Locking Interface Implementation for EC.
 */

/* Make sure that Newlib was compilet with retargetable locking support. */
#ifndef _RETARGETABLE_LOCKING
#error "Newlib must be compiled with retargetable locking support"
#endif

/* Non-recursive mutex. */

/*
 * Static locks
 *
 * We don't use K_MUTEX_DEFINE macro, because macro uses 'static' keyword which
 * makes mutexes unvisible outside of the file.
 */
struct mutex __lock___at_quick_exit_mutex;
struct mutex __lock___tz_mutex;
struct mutex __lock___dd_hash_mutex;
struct mutex __lock___arc4random_mutex;

void __retarget_lock_init(_LOCK_T *lock)
{
	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	*lock = malloc(sizeof(struct mutex));
	ASSERT(*lock != NULL);

	memset(*lock, 0, sizeof(struct mutex));
}

void __retarget_lock_close(_LOCK_T lock)
{
	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	free(lock);
}

void __retarget_lock_acquire(_LOCK_T lock)
{
	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	mutex_lock((struct mutex *)lock);
}

int __retarget_lock_try_acquire(_LOCK_T lock)
{
	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	return mutex_try_lock((struct mutex *)lock);
}

void __retarget_lock_release(_LOCK_T lock)
{
	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	mutex_unlock((struct mutex *)lock);
}

/* Recursive mutex. */

struct mutex_r {
	atomic_t owner;
	atomic_t waiters;
	int count;
};

#define K_MUTEX_R_DEFINE(name)  \
	struct mutex_r name = { \
		.owner = -1,    \
		.waiters = 0,   \
		.count = 0,     \
	}

/* Static locks */
K_MUTEX_R_DEFINE(__lock___sinit_recursive_mutex);
K_MUTEX_R_DEFINE(__lock___sfp_recursive_mutex);
K_MUTEX_R_DEFINE(__lock___atexit_recursive_mutex);
K_MUTEX_R_DEFINE(__lock___malloc_recursive_mutex);
K_MUTEX_R_DEFINE(__lock___env_recursive_mutex);

void __retarget_lock_init_recursive(_LOCK_T *lock)
{
	struct mutex_r *mutex_r;

	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	*lock = malloc(sizeof(struct mutex_r));
	ASSERT(*lock != NULL);

	mutex_r = (struct mutex_r *)*lock;
	mutex_r->owner = -1;
	mutex_r->count = 0;
	mutex_r->waiters = 0;
}

void __retarget_lock_close_recursive(_LOCK_T lock)
{
	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	free(lock);
}

void __retarget_lock_acquire_recursive(_LOCK_T lock)
{
	struct mutex_r *mutex_r = (struct mutex_r *)lock;
	atomic_val_t owner, current;

	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	owner = -1;
	current = task_get_current();

	atomic_or(&mutex_r->waiters, 1 << current);
	while (!atomic_compare_exchange(&mutex_r->owner, &owner, current) &&
	       owner != current) {
		owner = -1;
		task_wait_event_mask(TASK_EVENT_MUTEX, 0);
	}

	/* Only owner can escape while loop above. */

	/* We are not waiting for this mutex, so remove from waiters. */
	atomic_clear_bits(&mutex_r->waiters, 1 << current);
	mutex_r->count++;
}

int __retarget_lock_try_acquire_recursive(_LOCK_T lock)
{
	struct mutex_r *mutex_r = (struct mutex_r *)lock;
	atomic_val_t owner, current;

	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	owner = -1;
	current = task_get_current();

	if (!atomic_compare_exchange(&mutex_r->owner, &owner, current) &&
	    owner != current) {
		return 0;
	}

	/* Only owner can escape if above. */

	mutex_r->count++;

	return 1;
}

void __retarget_lock_release_recursive(_LOCK_T lock)
{
	struct mutex_r *mutex_r = (struct mutex_r *)lock;
	uint32_t waiters;

	ASSERT(lock != NULL);
	ASSERT(!in_interrupt_context());

	/* Panic if not called by owner */
	ASSERT(mutex_r->owner == task_get_current());
	ASSERT(mutex_r->count > 0);

	mutex_r->count--;
	if (mutex_r->count == 0) {
		mutex_r->owner = -1;
		waiters = atomic_load(&mutex_r->waiters);
		if (waiters) {
			task_set_event(__fls(waiters), TASK_EVENT_MUTEX);
		}
	}
}
