/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/pinctrl.h>
#include <zephyr/dt-bindings/pinctrl/it8xxx2-pinctrl.h>
#include <zephyr/init.h>

#include "hooks.h"
#include "nissa_common.h"

#define I2C4_NODE DT_NODELABEL(i2c4)
#if DT_NODE_EXISTS(I2C4_NODE)
PINCTRL_DT_DEFINE(I2C4_NODE);
#endif

/*
 * This function must be called before nereid_subboard_config to ensure that
 * we do not overwrite the GPIO configuration of GPIOE0/E7.
 */
static void nereid_subboard_config_early(void)
{
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	switch (sb) {
	case NISSA_SB_HDMI_A:
#if DT_NODE_EXISTS(I2C4_NODE)
		const struct pinctrl_dev_config *pcfg =
			PINCTRL_DT_DEV_CONFIG_GET(I2C4_NODE);

		pinctrl_apply_state(pcfg, PINCTRL_STATE_SLEEP);
#endif
		break;
	default:
		break;
	}
}
DECLARE_HOOK(HOOK_INIT, nereid_subboard_config_early, HOOK_PRIO_FIRST);

static void board_system_suspend_hooks(void)
{
	/* CPU clock is 24MHz */
	IT8XXX2_ECPM_SCDCR0 = 1;
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_system_suspend_hooks, HOOK_PRIO_LAST);

static void board_system_resume_hooks(void)
{
	/* CPU clock is 48MHz */
	IT8XXX2_ECPM_SCDCR0 = 0;
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_system_resume_hooks, HOOK_PRIO_FIRST);

static void board_clock_init(void)
{
	/* Disable unused modules' clock */
	IT8XXX2_ECPM_CGCTRL2R = 0x10;
	IT8XXX2_ECPM_CGCTRL3R = 0x5b;
	IT8XXX2_ECPM_CGCTRL5R = 0x7a;
	IT8XXX2_ECPM_CGCTRL6R = 0xe;
}
DECLARE_HOOK(HOOK_INIT, board_clock_init, HOOK_PRIO_DEFAULT);
