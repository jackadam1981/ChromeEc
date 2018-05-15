/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

/* TODO(b/78638238): Remove file if still unused after DVT */

void board_pd_execute_data_swap(int port, int data_role)
{
	/* On Octopus, only the first port can act as OTG*/
	if (port == 0)
		gpio_set_level(GPIO_USB2_OTG_ID,
			(data_role == PD_ROLE_UFP) ? 1 : 0);
}
