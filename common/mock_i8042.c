/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Mock EC i8042 interface code.
 */

#include "i8042.h"
#include "timer.h"


void i8042_receives_data(int data)
{
	/* Not implemented */
	return;
}


void i8042_receives_command(int cmd)
{
	/* Not implemented */
	return;
}


void i8042_command_task(void)
{
	/* Do nothing */
	while (1)
		usleep(5000000);
}
