/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "registers.h"
#include "system.h"

void chip_pre_init(void)
{
	/* wait core 0 init done */
	while (SCP_CORE0_GPR(0) != SCP_CORE0_INIT_DONE);

	/* let core 0 clear this bit when it's init done */
	SCP_CORE0_GPR(0) |= SCP_CORE1_INIT_DONE;
}
