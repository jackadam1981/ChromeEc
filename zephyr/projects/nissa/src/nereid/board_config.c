/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nereid sub-board hardware configuration */

#include <init.h>
#include <kernel.h>
#include <sys/printk.h>

#include "gpios.h"
#include "hooks.h"
#include "task.h"

#include "sub_board.h"

static void nereid_subboard_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, nereid_subboard_init, HOOK_PRIO_FIRST+1);
