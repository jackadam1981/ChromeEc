/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "cros_board_info.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "power_button.h"
#include "power.h"
#include "switch.h"
#include "throttle_ap.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/******************************************************************************/
/* USB-A charging control */

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USBA,
};
BUILD_ASSERT(ARRAY_SIZE(usb_port_enable) == USB_PORT_COUNT);

/******************************************************************************/

int board_set_active_charge_port(int port)
{
	/* TODO(b/197514362): set either barreljack or typec port */
	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
				int max_ma, int charge_mv)
{
}

static uint16_t board_version;
static uint32_t sku_id;
static uint32_t fw_config;

static void cbi_init(void)
{
	/*
	 * Load board info from CBI to control per-device configuration.
	 *
	 * If unset it's safe to treat the board as a proto, just C10 gating
	 * won't be enabled.
	 */
	uint32_t val;

	if (cbi_get_board_version(&val) == EC_SUCCESS && val <= UINT16_MAX)
		board_version = val;
	if (cbi_get_sku_id(&val) == EC_SUCCESS)
		sku_id = val;
	if (cbi_get_fw_config(&val) == EC_SUCCESS)
		fw_config = val;
	CPRINTS("Board Version: %d, SKU ID: 0x%08x, F/W config: 0x%08x",
		board_version, sku_id, fw_config);
}
DECLARE_HOOK(HOOK_INIT, cbi_init, HOOK_PRIO_INIT_I2C + 1);
