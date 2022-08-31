/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "fan.h"
#include "gpio/gpio.h"
#include "hooks.h"

/*
 * Rex fan support
 */
static void fan_init(void)
{
	/*
	 * TODO(b/244870433): Currently this function needs to exist but, no
	 * functionality is required. Once CBI FW config is being used and there
	 * are variants without fans, then this function will be used to
	 * determine if fan control is required or not.
	 */
}
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_POST_FIRST);
