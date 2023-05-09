/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "button.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "nissa_sub_board.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

static void board_button_init(void)
{
	int ret;
	uint32_t val;
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	ret = cbi_get_board_version(&val);
	if (ret != EC_SUCCESS) {
		LOG_ERR("Error retrieving CBI BOARD_VER.");
		return;
	}
	/*
	 * The volume up/down button are exchanged on ver3 USB
	 * sub board.
	 *
	 * LTE:
	 *   volup -> gpioa2, voldn -> gpio93
	 * USB:
	 *   volup -> gpio93, voldn -> gpioa2
	 */
	if (val == 3 && sb == NISSA_SB_C_A) {
		LOG_INF("Volume up/down btn exchanged on ver3 USB sku");
		buttons[BUTTON_VOLUME_UP].gpio = GPIO_VOLUME_DOWN_L;
		buttons[BUTTON_VOLUME_DOWN].gpio = GPIO_VOLUME_UP_L;
	}
}
DECLARE_HOOK(HOOK_INIT, board_button_init, HOOK_PRIO_DEFAULT);
