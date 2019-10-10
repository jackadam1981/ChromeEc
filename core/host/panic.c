/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "panic.h"

/*
 * Print panic data
 */
void panic_data_print(const struct panic_data *pdata)
{
	ccprintf("Host panic_data at 0x%pP\n", pdata);
}
