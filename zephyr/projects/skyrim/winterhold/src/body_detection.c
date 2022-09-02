/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* winterhold-specific body detection function */

#include "body_detection.h"
#include "ec_commands.h"
#include "host_command.h"

__override void board_body_state_change(void)
{
	host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
}
