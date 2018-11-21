/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * IPI handlers declaration
 */

#include "common.h"
#include "ipi_chip.h"

#ifndef PASS
#define PASS 1
#endif

#define ipi_arguments int32_t id, void *data, uint32_t len

#if PASS == 1
void ipi_handler_undefined(ipi_arguments)
{
	return;
}

void ipi_wakeup_undefined(void)
{
	return;
}

#define table(type, name, x) x

#define ipi_x_func(suffix, args, number)                                       \
	extern void __attribute__(                                             \
		(used, weak, alias(STRINGIFY(ipi_ ## suffix ## _undefined))))  \
		ipi_ ## number ## _ ## suffix(args);

#endif /* PASS == 1 */

#if PASS == 2

#undef table
#undef ipi_x_func

#define table(type, name, x)                                                   \
	type name[] __attribute__(                                             \
		(aligned(4), section(".rodata.ipi, \"a\" @"))) = {x};

#define ipi_x_func(suffix, args, number)                                       \
	[number < IPI_COUNT ? number : -1] = ipi_##number##_##suffix,

#endif /* PASS == 2 */

/*
 * Table to hold all the IPI handler function pointer.
 */
table(ipi_handler_t, ipi_handler_table,
	ipi_x_func(handler, ipi_arguments, 0)
	ipi_x_func(handler, ipi_arguments, 1)
	ipi_x_func(handler, ipi_arguments, 2)
	ipi_x_func(handler, ipi_arguments, 3)
	ipi_x_func(handler, ipi_arguments, 4)
	ipi_x_func(handler, ipi_arguments, 5)
	ipi_x_func(handler, ipi_arguments, 6)
	ipi_x_func(handler, ipi_arguments, 7)
)

/*
 * Table to hold all the wake-up function pointer.
 */
table(ipi_wakeup_t, ipi_wakeup_table,
	ipi_x_func(wakeup, void, 0)
	ipi_x_func(wakeup, void, 1)
	ipi_x_func(wakeup, void, 2)
	ipi_x_func(wakeup, void, 3)
	ipi_x_func(wakeup, void, 4)
	ipi_x_func(wakeup, void, 5)
	ipi_x_func(wakeup, void, 6)
	ipi_x_func(wakeup, void, 7)
)

#if PASS == 1
#undef PASS
#define PASS 2
#include "ipi_table.c"
#endif
