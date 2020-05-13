/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#ifndef __CROS_EC_CLOCK_CHIP_H
#define __CROS_EC_CLOCK_CHIP_H

#include "registers.h"

/* Default ULPOSC clock speed in MHz */
#ifndef ULPOSC1_CLOCK_MHZ
#define ULPOSC1_CLOCK_MHZ 240
#endif
#ifndef ULPOSC2_CLOCK_MHZ
#define ULPOSC2_CLOCK_MHZ 330
#endif

enum scp_clock_source {
	SCP_CLK_26M = CLK_SW_SEL_26M,
	SCP_CLK_32K = CLK_SW_SEL_32K,
	SCP_CLK_ULPOSC2 = CLK_SW_SEL_ULPOSC2,
	SCP_CLK_ULPOSC1 = CLK_SW_SEL_ULPOSC1,
};

/* Switches to use 'src' clock */
void clock_select_clock(enum scp_clock_source src);

#endif /* __CROS_EC_CLOCK_CHIP_H */
