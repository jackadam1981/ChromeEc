/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Registers map and defintions for Andes cores
 */

#ifndef __CPU_H
#define __CPU_H

#include <stdint.h>

/* Macro to access 32-bit registers */
#define CPUREG(addr) (*(volatile uint32_t*)(addr))

/* Set up the cpu to detect faults */
void cpu_init(void);

#endif /* __CPU_H */
