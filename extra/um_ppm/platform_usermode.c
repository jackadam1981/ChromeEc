/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/platform.h"
#include "ppm_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

bool debug_enabled = false;

void *platform_malloc(size_t size)
{
	return malloc(size);
}
void *platform_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}
void platform_free(void *ptr)
{
	free(ptr);
}

void platform_memcpy(void *dest, const void *src, size_t length)
{
	memcpy(dest, src, length);
}
void platform_memset(void *dest, uint8_t data, size_t length)
{
	memset(dest, data, length);
}

void platform_set_debug(bool enable)
{
	debug_enabled = enable;
}
bool platform_debug_enabled()
{
	return debug_enabled;
}

void platform_printf(const char *format, ...)
{
	va_list arglist;

	va_start(arglist, format);
	vprintf(format, arglist);
	va_end(arglist);
}
void platform_eprintf(const char *format, ...)
{
	va_list arglist;

	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	va_end(arglist);
}

void platform_usleep(uint32_t usec)
{
	usleep(usec);
}

struct task_handle {
	pthread_t thread;
};

#ifdef __ZEPHYR__
#include <zephyr/kernel/thread_stack.h>
#define STACK_SIZE (1024)
K_THREAD_STACK_DEFINE(stack, STACK_SIZE);
#endif

int platform_task_init(void *start_fn, void *arg, struct task_handle **handle)
{
	int res;

	if (!*handle) {
		ELOG("Handle=NULL not supported");
		return -1;
	}

#ifdef __ZEPHYR__
	pthread_attr_t attr;

	/* Zephyr requires a non-NULL attribute for pthread_create */
	res = pthread_attr_init(&attr);
	if (res != 0) {
		errno = res;
		perror("pthread_attr_init");
		return -1;
	}

	res = pthread_attr_setstack(&attr, &stack, STACK_SIZE);
	if (res != 0) {
		errno = res;
		perror("pthread_attr_setstack");
		return -1;
	}
	res = pthread_create(&(*handle)->thread, &attr, start_fn, arg);
#else
	res = pthread_create(&(*handle)->thread, NULL, start_fn, arg);
#endif
	if (res != 0) {
		ELOG("Failed to start thread with error %d for start_fn %p",
		     res, start_fn);
		return -1;
	}

	return 0;
}

void platform_task_exit()
{
	pthread_exit(NULL);
}

int platform_task_complete(struct task_handle *handle)
{
	return pthread_join(handle->thread, NULL);
}

struct platform_mutex {
	pthread_mutex_t lock;
};

int platform_mutex_init(struct platform_mutex **mutex)
{
	if (!*mutex) {
		return -1;
	}

	if (pthread_mutex_init(&(*mutex)->lock, NULL)) {
		return -1;
	}

	return 0;
}

void platform_mutex_lock(struct platform_mutex *mutex)
{
	pthread_mutex_lock(&mutex->lock);
}
void platform_mutex_unlock(struct platform_mutex *mutex)
{
	pthread_mutex_unlock(&mutex->lock);
}

struct platform_condvar {
	pthread_cond_t var;
};

int platform_condvar_init(struct platform_condvar **cond)
{
	if (!*cond) {
		return -1;
	}

	if (pthread_cond_init(&(*cond)->var, NULL)) {
		return -1;
	}

	return 0;
}

void platform_condvar_wait(struct platform_condvar *condvar,
			   struct platform_mutex *mutex)
{
	pthread_cond_wait(&condvar->var, &mutex->lock);
}

void platform_condvar_signal(struct platform_condvar *condvar)
{
	pthread_cond_signal(&condvar->var);
}

void platform_init_ppm_vars(struct ppm_common_device *dev)
{
	static struct platform_condvar ppm_condvar;
	static struct platform_mutex ppm_lock;
	static struct task_handle ppm_task_handle;

	platform_memset(&ppm_condvar, 0, sizeof(ppm_condvar));
	platform_memset(&ppm_lock, 0, sizeof(ppm_lock));
	platform_memset(&ppm_task_handle, 0, sizeof(ppm_task_handle));

	dev->ppm_condvar = &ppm_condvar;
	dev->ppm_lock = &ppm_lock;
	dev->ppm_task_handle = &ppm_task_handle;
}
