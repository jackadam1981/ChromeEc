/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "base_state.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_LID, format, ## args)

/* 1: base attached, 0: otherwise */
static int base_state;

int base_get_state(void)
{
	return base_state;
}

void base_set_state(int state)
{
	if (base_state == !!state)
		return;

	base_state = !!state;
	CPRINTS("base state: %stached", state ? "at" : "de");
	hook_notify(HOOK_BASE_ATTACHED_CHANGE);
}
