/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_common_pwrseq_sm_handler.h>

#include "hooks.h"

DECLARE_HOOK(HOOK_INIT, ap_pwrseq_task_start, HOOK_PRIO_DEFAULT);
