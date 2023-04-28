/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Myst board-specific USB-C mux configuration */

#include "console.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "usb_mux.h"
#include "usb_mux_config.h"
#include "usbc/ppc.h"
#include "usbc/tcpci.h"
#include "usbc/usb_muxes.h"
#include "usbc_config.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(myst, CONFIG_MYST_LOG_LEVEL);

uint32_t io_db_type;

__override uint8_t board_get_usb_pd_port_count(void)
{
	if (io_db_type == FW_IO_DB_NONE)
		return CONFIG_USB_PD_PORT_MAX_COUNT - 1;
	else
		return CONFIG_USB_PD_PORT_MAX_COUNT;
}

static void setup_mux(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_IO_DB, &io_db_type);
	if (ret != 0) {
		io_db_type = FW_IO_DB_NONE;
		LOG_ERR("Failed to get IO_DB value: %d", ret);
		return;
	}

	if (val == FW_IO_DB_NONE) {
		LOG_INF("USB DB: not connected");
		memset(&tcpc_config[USBC_PORT_C1], 0,
		       sizeof(struct tcpc_config_t));
		memset(&ppc_chips[USBC_PORT_C1], 0,
		       sizeof(struct ppc_config_t));
	}
	if (val == FW_IO_DB_SKU_A) {
		LOG_INF("USB DB: Setting SKU_A DB");
		/* configured by Default */
	}
	if (val == FW_IO_DB_SKU_B) {
		LOG_INF("USB DB: Setting SKU_B DB");
		TCPC_ENABLE_ALTERNATE_BY_NODELABEL(USBC_PORT_C1,
						   ps8815_port1_alt);
		PPC_ENABLE_ALTERNATE_BY_NODELABEL(USBC_PORT_C1, ppc_port1_alt);
	}
}
DECLARE_HOOK(HOOK_INIT, setup_mux, HOOK_PRIO_INIT_I2C);
