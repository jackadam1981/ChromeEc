/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ATOMIC_BIT_H
#define __CROS_EC_ATOMIC_BIT_H

#ifndef CONFIG_ZEPHYR
#include "atomic.h"
#include "compiler.h"

/*
 * TODO(b/254710459): clang does not have __atomic_load_4 for ARMv6-M, so
 * provide an implementation until it is fixed:
 * https://github.com/llvm/llvm-project/issues/58603.
 */
#if !__has_builtin(__atomic_load_4)
static atomic_val_t __atomic_load_4(const atomic_t *target, int memorder)
{
	atomic_val_t ret;

	asm volatile("dmb ish\n"
		     "ldr  %0, [%1]\n"
		     "dmb ish\n"
		     : "=r"(ret)
		     : "r"(target));

	return ret;
}

static inline atomic_val_t atomic_get(const atomic_t *target)
{
	DISABLE_CLANG_WARNING("-Watomic-alignment");
	return __atomic_load_n(target, __ATOMIC_SEQ_CST);
	ENABLE_CLANG_WARNING("-Watomic-alignment");
}
#else
static inline atomic_val_t atomic_get(const atomic_t *target)
{
	return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}
#endif

#include "third_party/zephyr/atomic.h"

#endif /* CONFIG_ZEPHYR */
#endif /* __CROS_EC_ATOMIC_BIT_H */
