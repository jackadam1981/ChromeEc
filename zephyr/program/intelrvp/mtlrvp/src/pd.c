/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#if 0
void board_dc_jack_interrupt(enum gpio_signal signal)
{
}
#endif

void board_charging_enable(int port, int enable)
{
}

int board_vbus_source_enabled(int port)
{
	return 0;
}
