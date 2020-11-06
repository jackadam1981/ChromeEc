/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "link_defs.h"
#include "host_command.h"

/*
 * TODO(b:172678200): Implement host commands shim.  This is a minimal
 * stub implementation for now just to get some things compiling.
 */

const struct host_command *__hcmds = NULL;
const struct host_command *__hcmds_end = NULL;
