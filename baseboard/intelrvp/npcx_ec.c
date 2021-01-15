/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel BASEBOARD-RVP NPCX EC specific configuration */

#include "common.h"
#include "fan_chip.h"
#include "keyboard_scan.h"
#include "npcx_ec.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "time.h"

/* Reset PD MCU */
void board_reset_pd_mcu(void)
{
	/* Not applicable for ITE TCPC */
}

/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 35,
	.debounce_down_us = 5 * MSEC,
	.debounce_up_us = 40 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

/* PWM configuration */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_FAN] = {
		.channel = PWN_FAN_CHANNEL,
		.flags = PWM_CONFIG_HAS_RPM_MODE,
		.freq = 30000,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

const struct mft_t mft_channels[] = {
	[MFT_CH_0] = {
		.module = NPCX_MFT_MODULE_1,
		.clk_src = TCKC_LFCLK,
		.pwm_id = PWM_CH_FAN,
	},
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);
