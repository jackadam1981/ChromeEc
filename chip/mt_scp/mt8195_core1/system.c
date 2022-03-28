/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "registers.h"
#include "system.h"

void chip_pre_init(void)
{
	/* ask core 0 if I can run */
	/* SCP_GIPC_IN_SET = SCP_GIPC_IS_CORE0_OK; */

	/* wait core 0 init done */
	/* while ((SCP_GIPC_IN_SET & SCP_GIPC_IS_CORE0_OK) != 0); */
}
