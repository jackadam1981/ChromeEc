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
#include "cros_version.h"

/**
 * The internal data structure stored for a deferred function.
 */
struct deferred_data {
#if IS_ZEPHYR_VERSION(2, 6)
	struct k_work_delayable *work;
#else
	struct k_delayed_work *work;
#endif
};

/**
 * See include/hooks.h for documentation.
 */
int hook_call_deferred(const struct deferred_data *data, int us);

#if IS_ZEPHYR_VERSION(2, 6)
#define DECLARE_DEFERRED(routine)                                    \
	K_WORK_DELAYABLE_DEFINE(routine##_work_data,                 \
				(void (*)(struct k_work *))routine); \
	__maybe_unused const struct deferred_data routine##_data = { \
		.work = &routine##_work_data,                \
	}
#else
#define DECLARE_DEFERRED(routine)                                    \
	K_DELAYED_WORK_DEFINE(routine##_work_data,                   \
			      (void (*)(struct k_work *))routine);   \
	__maybe_unused const struct deferred_data routine##_data = { \
		.work = &routine##_work_data,                \
	}
#endif

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
 * @param entry  The statically allocated hook list entry.
 */
int zephyr_shim_setup_hook(struct zephyr_shim_hook_list *entry);

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
	SYS_INIT_ARG(zephyr_shim_setup_hook, &_hook_lst_##line, \
			APPLICATION, 1)
