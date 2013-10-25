/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * N8 CPU initialization
 */

#include "config.h"

.section .text.vecttable

.macro vector name
.long \name\()_handler
.weak \name\()_handler
.set \name\()_handler, default_handler
.endm

.macro vector_irq number
.if \number < CONFIG_IRQ_COUNT
vector irq_\()\number
.endif
.endm

/* Exceptions vector */
vectors:
.long stack_end 
.long reset

.text

.global reset
reset:
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
	j	.

_bss_start:
.long __bss_start
_bss_end:
.long __bss_end
_data_start:
.long __data_start
_data_end:
.long __data_end
_ro_end:
.long __ro_end

/* Reserve space for system stack */
.section .bss.system_stack
stack_start:
.space CONFIG_STACK_SIZE, 0
stack_end:
.global stack_end

