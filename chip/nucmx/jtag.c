/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "gpio.h"
#include "jtag.h"
#include "registers.h"
#include "system.h"

void jtag_pre_init(void)
{
	/* Enable automatic freeze mode */
	CLEAR_BIT(NUCMX_DBGFRZEN3, NUCMX_DBGFRZEN3_GLBL_FRZ_DIS);
}

