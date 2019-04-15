/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
);

/*
 * Table to hold all the wake-up bool address.
 */
table(int*, ipi_wakeup_table,
	ipi_x_var(wakeup, 0)
	ipi_x_var(wakeup, 1)
	ipi_x_var(wakeup, 2)
	ipi_x_var(wakeup, 3)
	ipi_x_var(wakeup, 4)
	ipi_x_var(wakeup, 5)
	ipi_x_var(wakeup, 6)
	ipi_x_var(wakeup, 7)
);
