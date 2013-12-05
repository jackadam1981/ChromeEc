/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Handy clever tricks */

#ifndef __CROS_EC_TRICKS_H
#define __CROS_EC_TRICKS_H

/* Test an important condition at compile time, not run time */
#define _BA1_(cond, line) \
	extern int __build_assertion_ ## line[1 - 2*!(cond)] \
	__attribute__ ((unused))
#define _BA0_(c, x) _BA1_(c, x)
#define BUILD_ASSERT(cond) _BA0_(cond, __LINE__)

/* Number of elements in an array */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Just in case - http://gcc.gnu.org/onlinedocs/gcc/Offsetof.html */
#ifndef offsetof
#define offsetof(type, member)  __builtin_offsetof(type, member)
#endif

/**
 * returns a pointer the containing struct of a field
 * @param ptr   the pointer to the member.
 * @param type  the type of the container struct.
 * @param field the name of the member within the struct.
 */
#define container_of(ptr, type, field) ({              \
        const typeof(((type *)0)->field) *__p = (ptr); \
	        (type *)((char *)__p - offsetof(type,field));})

#endif /* __CROS_EC_TRICKS_H */
