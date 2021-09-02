/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "pathsel.h"
#include "board.h"

void dut_to_host(void)
{
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_SEL, 0);
	gpio_set_level(GPIO_FASTBOOT_DUTHUB_MUX_EN_L, 0);

    /* Set USERVO_FASTBOOT_MUX_SEL. Hub connected to DUT */
    write_ioexpander(1, 0, 1);
}

void uservo_to_host(void)
{
    /* Clear USERVO_FASTBOOT_MUX_SEL. Hub connected to uservo */
	write_ioexpander(1, 0, 0);
}
