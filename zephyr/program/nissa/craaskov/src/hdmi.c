/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nissa_hdmi.h"

#include <cros_board_info.h>

__override void nissa_configure_hdmi_power_gpios(void)
{
	/*
	 * Craaskov board version before 1 needs hdmi-en-odl to be
	 * pulled down to enable VCC on the HDMI port, but later
	 * versions HDMI 5V output is ON by default when PP5000_S5
	 * is on.
	 */
	uint32_t board_version = 0;

	/* CBI errors ignored, will configure the pin */
	cbi_get_board_version(&board_version);
	if (board_version < 1) {
		nissa_configure_hdmi_vcc();
	}
}
