/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inline assembly for IT8380 */

#ifndef __CROS_EC_ASM_H
#define __CROS_EC_ASM_H

static inline void nop(void)
{
	asm volatile ("nop");
}

#endif /* __CROS_EC_ASM_H */
