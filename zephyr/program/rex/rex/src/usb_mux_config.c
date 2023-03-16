/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Rex board-specific USB-C mux configuration */

#include "console.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "ioexpander.h"
#include "usb_mux.h"
#include "usbc/tcpci.h"
#include "usbc/usb_muxes.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_ZTEST

#undef USB_MUX_ENABLE_ALTERNATIVE
#define USB_MUX_ENABLE_ALTERNATIVE(x)

#endif /* CONFIG_ZTEST */

LOG_MODULE_DECLARE(rex, CONFIG_REX_LOG_LEVEL);

static void setup_mux(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_IO_DB, &val);
	if (ret != 0) {
		LOG_ERR("Error finding FW_DB_IO in CBI FW_CONFIG, ret: %d",
			ret);
		return;
	}

	if (val == FW_IO_DB_NOT_CONNECTED) {
		LOG_INF("DB USB not connected");
	}
	if (val == FW_IO_DB_USB3) {
		LOG_INF("C1: Setting USB3 mux");
	}
	if (val == FW_IO_DB_USB4_ANX7452) {
		LOG_INF("C1: Setting ANX7452 mux");
		USB_MUX_ENABLE_ALTERNATIVE(usb_mux_chain_anx7452_port1);
		TCPC_ENABLE_ALTERNATE_BY_NODELABEL(1, rt1716_tcpc_port1);
	}
}
DECLARE_HOOK(HOOK_INIT, setup_mux, HOOK_PRIO_INIT_I2C);
