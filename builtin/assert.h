/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ASSERT_H__
#define __CROS_EC_ASSERT_H__

/* Include CONFIG definitions for EC sources. */
#ifndef THIRD_PARTY
#include "common.h"
#endif

#ifdef CONFIG_DEBUG_ASSERT
#ifdef CONFIG_DEBUG_ASSERT_REBOOTS

#ifdef CONFIG_DEBUG_ASSERT_BRIEF
extern void panic_assert_fail(const char *fname, int linenum)
	__attribute__((noreturn));
#define ASSERT(cond) do {					\
		if (!(cond))					\
			panic_assert_fail(__FILE__, __LINE__);	\
	} while (0)
#else
void panic_printf(const char *format, ...);
#define ASSERT(cond) do {					     \
		if (!(cond)) {						      \
			panic_printf("\nASSERT FAIL '%s' in %s() at %s:%d\n", \
				     #cond, __func__, __FILE__, __LINE__);    \
			/* Trigger WD for exception frame and reset. */	      \
			while (1);					      \
		}							      \
	} while (0)
#endif
#else
#define ASSERT(cond) do {			\
		if (!(cond))			\
			__asm("bkpt");		\
			__builtin_unreachable();\
	} while (0)
#endif
#else
#define ASSERT(cond)
#endif

#define assert(x...) ASSERT(x)

#endif /* __CROS_EC_ASSERT_H__ */
