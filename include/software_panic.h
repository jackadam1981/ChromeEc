/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Software panic constants. This file must be parsable by the assembler.
 */

#ifndef __CROS_EC_SOFTWARE_PANIC_H
#define __CROS_EC_SOFTWARE_PANIC_H

/* Holds software panic reason *_PANIC */
#define SOFTWARE_PANIC_REASON_REG	r4
/* Holds custom data specific to panic reason */
#define SOFTWARE_PANIC_INFO_REG		r5

#define SOFTWARE_PANIC_BASE		0xDEAD6660

/* Software panic reasons */
#define DIV_ZERO_PANIC			(SOFTWARE_PANIC_BASE + 0)
#define STACK_OVERFLOW_PANIC		(SOFTWARE_PANIC_BASE + 1)

/* For macro expansion inside __asm__ macro */
#define STR(x) #x
#define EXP(x) STR(x)

#endif  /* __CROS_EC_SOFTWARE_PANIC_H */
