/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CPU_H
#define __CROS_EC_CPU_H

/* Return to specified function from exception handler context. */
int cpu_return_from_exception(void (*func)(void));

/* Do nothing for Zephyr */
static inline void cpu_init(void)
{
}

#endif /* __CROS_EC_CPU_H */
