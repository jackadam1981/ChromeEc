/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "util.h"

#ifndef HAVE_PRIVATE_MT8183
void private_hello(void) {}
#else
void private_hello(void);
#endif

static int command_test_private_hello(int argc, char **argv)
{
	ccprints("test command private hello 2");
	private_hello();
        return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(testprivatenl, command_test_private_hello,
                NULL, "Test command private hello");

