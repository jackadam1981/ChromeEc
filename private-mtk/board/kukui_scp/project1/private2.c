/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "util.h"

static int command_test_private2(int argc, char **argv)
{
	ccprints("private2 test command 2");
        return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(testprivate2, command_test_private2,
                NULL, "Test command 2");
