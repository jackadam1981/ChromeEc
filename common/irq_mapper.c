/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IRQ mapper for boards without common runtime */

#include "common.h"
#include "task.h"

#ifdef CONFIG_COMMON_RUNTIME
#error irq_mapper.c must only be used without common runtime!
#endif

#define ENABLE_IRQ(irq) \
	extern void IRQ_HANDLER_OPT(irq)(void); \
	void IRQ_HANDLER(irq)(void) \
	{ \
		IRQ_HANDLER_OPT(irq)(); \
	}
#include "ec.irqlist"
