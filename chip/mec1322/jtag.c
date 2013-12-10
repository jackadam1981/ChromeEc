/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Settings to enable JTAG debugging */

#include "jtag.h"
#include "registers.h"

void jtag_pre_init(void)
{
	/*
	 * Disable JTAG. JTAG_nRST pin also need to be tied to ground by
	 * hardware in order for JTAG pins to be functional as GPIOs or
	 * keyboard pins.
	 */
	MEC1322_EC_JTAG_EN &= ~1;
}
