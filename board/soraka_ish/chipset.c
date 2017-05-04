/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 *  * Use of this source code is governed by a BSD-style license that can be
 *   * found in the LICENSE file.
 *    */

#include <chipset.h>
#include <task.h>
#include <timer.h>

test_mockable void chipset_task(void)
{
	/* Nothing to do */
	for (;;)
		usleep(10000000);
}


int chipset_in_state(int state_mask)
{
	if (state_mask == CHIPSET_STATE_ON)
		return 1;
	else
		return 0;
}
