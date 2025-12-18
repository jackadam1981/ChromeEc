/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system_chip.h"

#include <soc.h>

int system_get_hibernate_wake_source(void)
{
	struct glue_reg *inst_glue = (struct glue_reg *)(NPCX_GLUE_REG_ADDR);

	return inst_glue->PSL_CTS & 0xf;
}
