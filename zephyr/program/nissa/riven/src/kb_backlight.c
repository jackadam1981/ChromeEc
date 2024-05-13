/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board_config.h"
#include "common.h"
#include "cros_board_info.h"
#include "cros_cbi.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

__override uint32_t board_override_feature_flags0(uint32_t flags0)
{
	/*
	 * Remove keyboard backlight feature for devices don't support it.
	 */
	return (flags0 & ~EC_FEATURE_MASK_0(EC_FEATURE_PWM_KEYB));
}
