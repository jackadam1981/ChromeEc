/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "task.h"
#include <kernel.h>

uint32_t interrupt_disable(void)
{
	return irq_lock();
}

void interrupt_enable(uint32_t key)
{
	irq_unlock(key);
}

