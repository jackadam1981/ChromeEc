/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"

#include <power_signals.h>

__override void set_board_ready_before_imvp_update(void)
{
	ccprints("Initiating to G3 for IMVP update");
	chipset_force_shutdown(CHIPSET_SHUTDOWN_G3);

	ccprints("set Arail on");
	power_signal_set(PWR_EN_PP3300_A, 1);
}

__override void set_board_ready_after_imvp_update(void)
{
	power_signal_set(PWR_EN_PP3300_A, 0);
	ccprints("Press Powerbutton/use powerbtn command to progress");
}
