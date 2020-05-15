/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdlib.h>
#include <stdio.h>

void panic_assert_fail(const char *msg, const char *func, const char *fname,
			int linenum)
{
	printf("ASSERTION FAIL: %s:%d:%s - %s\n",
	       fname, linenum, func, msg);

	exit(1);
}

