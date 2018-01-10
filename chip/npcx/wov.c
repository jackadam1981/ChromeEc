/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#ifdef CONFIG_WOV
#ifndef NPCX_WOV_SUPPORT
#error "Do not enable CONFIG_WOV if npcx ec doesn't support WOV !"
#endif
#endif
