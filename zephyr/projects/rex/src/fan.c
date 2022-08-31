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

/* LOG_MODULE_DECLARE(rex, CONFIG_REX_LOG_LEVEL); */

/*
 * Rex fan support
 */
static void fan_init(void)
{
	/* Assume FAN exists, nothing else required */
}
DECLARE_HOOK(HOOK_INIT, fan_init, HOOK_PRIO_POST_FIRST);
