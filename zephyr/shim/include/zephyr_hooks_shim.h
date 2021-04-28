/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if !defined(__CROS_EC_HOOKS_H) || defined(__CROS_EC_ZEPHYR_HOOKS_SHIM_H)
#error "This file must only be included from hooks.h. Include hooks.h directly."
#endif
#define __CROS_EC_ZEPHYR_HOOKS_SHIM_H

#include <init.h>
#include <kernel.h>
#include <zephyr.h>

#include "common.h"

/* TODO: upstream this */
#define SYS_INIT_ARG(_init_fn, _init_arg, _level, _prio) \
	Z_INIT_ENTRY_DEFINE(Z_SYS_NAME(_init_fn), _init_fn, _init_arg, \
			_level, _prio)

/**
 * The internal data structure stored for a deferred function.
 */
struct deferred_data {
	void (*routine)(void);
	struct k_delayed_work delayed_work;
};

/**
 * See include/hooks.h for documentation.
 */
int hook_call_deferred(const struct deferred_data *data, int us);

/**
 * Runtime helper to setup deferred data.
 *
 * @param data		The struct deferred_data.
 */
int zephyr_shim_setup_deferred(const struct device *entry);

/**
 * See include/hooks.h for documentation.
 *
 * Typically Zephyr would put const data in the rodata section but that is
 * write-protected with native_posix. So force it into .data when building for
 * ARCH_POSIX and put it in .rodata in all other cases so that it does not take
 * extra RAM space.
 */
#ifdef CONFIG_ARCH_POSIX
#define DEFERRED_DATA_SECTION ".data.hooks"
#else
#define DEFERRED_DATA_SECTION ".rodata.hooks"
#endif

#define DECLARE_DEFERRED(routine) _DECLARE_DEFERRED(routine)
#define _DECLARE_DEFERRED(_routine)                                        \
	__maybe_unused const struct deferred_data _routine##_data          \
		__attribute__((section(DEFERRED_DATA_SECTION))) = {        \
		.routine = _routine,                                       \
	};                                                                 \
	SYS_INIT_ARG(zephyr_shim_setup_deferred, (void *)&_routine##_data, \
			APPLICATION, 1)

/**
 * Internal linked-list structure used to store hook lists.
 */
struct zephyr_shim_hook_list {
	void (*routine)(void);
	enum hook_priority priority;
	enum hook_type type;
	struct zephyr_shim_hook_list *next;
};

/**
 * Runtime helper for DECLARE_HOOK setup data.
 *
 * @param type		The type of hook.
 * @param routine	The handler for the hook.
 * @param priority	The priority (smaller values are executed first).
 * @param entry		A statically allocated list entry.
 */
int zephyr_shim_setup_hook(const struct device *entry);

/**
 * See include/hooks.h for documentation.
 */
#define DECLARE_HOOK(hooktype, routine, priority) \
	_DECLARE_HOOK_1(hooktype, routine, priority, __LINE__)
#define _DECLARE_HOOK_1(_hooktype, _routine, _priority, line) \
	_DECLARE_HOOK_2(_hooktype, _routine, _priority, line)
#define _DECLARE_HOOK_2(_hooktype, _routine, _priority, line)      \
	static struct zephyr_shim_hook_list _hook_lst_##line = {   \
		.routine = _routine,                               \
		.priority = _priority,                             \
		.type = _hooktype,                                 \
	};                                                         \
	SYS_INIT_ARG(zephyr_shim_setup_hook, (void *)&_hook_lst_##line, \
			APPLICATION, 1)
