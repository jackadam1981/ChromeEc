/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "util.h"

static int command_test_private_nolib(int argc, char **argv)
{
	ccprints("private test command (no lib)");
        return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(testprivatenl, command_test_private_nolib,
                NULL, "Test command (nolib)");
