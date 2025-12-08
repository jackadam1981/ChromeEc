/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "extpower.h"
#include "hooks.h"
#include "host_command.h"

__overridable void board_check_extpower(void)
{
}

test_mockable void extpower_handle_update(int is_present)
{
	hook_notify(HOOK_AC_CHANGE);

	if (!IS_ENABLED(HAS_TASK_HOSTCMD)) {
		return;
	}

	extpower_update_host_events(is_present);
}
