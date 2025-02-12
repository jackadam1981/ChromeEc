/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Uldrenite hardware configuration */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "driver/tcpm/tcpci.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "task.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc/usb_muxes.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#include <ap_power/ap_power.h>

LOG_MODULE_DECLARE(trulo, LOG_LEVEL_INF);

enum uldrenite_typec_type {
	ULDRENITE_TYPE_UNKNOWN = -1, /* Uninitialised */
	ULDRENITE_TWO_C = 0, /* two type C ports */
	ULDRENITE_ONE_C = 1, /* one type C port */
};

static uint8_t cached_usb_pd_port_count;

__override uint8_t pdc_power_mgmt_get_usb_pd_port_count(void)
{
	if (cached_usb_pd_port_count == 0) {
		LOG_ERR("USB PD Port count not initialized!");
		cached_usb_pd_port_count = ULDRENITE_TWO_C;
		return cached_usb_pd_port_count;
	}

	LOG_INF("cached_usb_pd_port_count = %d ", cached_usb_pd_port_count);
	return cached_usb_pd_port_count;
}

test_export_static enum uldrenite_typec_type uldrenite_cached_typec_type =
	ULDRENITE_TYPE_UNKNOWN;
/*
 * Retrieve type-c type from FW_CONFIG.
 */
enum uldrenite_typec_type uldrenite_get_typec_type(void)
{
	int ret;
	uint32_t val;

	/*
	 * Return cached value.
	 */
	if (uldrenite_cached_typec_type != ULDRENITE_TYPE_UNKNOWN)
		return uldrenite_cached_typec_type;

	uldrenite_cached_typec_type = ULDRENITE_TWO_C; /* Defaults to two typec
							  ports */
	ret = cros_cbi_get_fw_config(USBC_PORTS, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", USBC_PORTS);
		return uldrenite_cached_typec_type;
	}
	switch (val) {
	default:
		LOG_INF("No typec port defined, default two type-c ports");
		break;
	case TWO_PORTS:
		uldrenite_cached_typec_type = ULDRENITE_TWO_C;
		LOG_INF("USB two type-C ");
		break;

	case ONE_PORT:
		uldrenite_cached_typec_type = ULDRENITE_ONE_C;
		LOG_INF("USB type C only");
		break;
	}
	return uldrenite_cached_typec_type;
}

/*
 * Initialise the USB PD port count, which
 * depends on FW_CONFIG.
 */
test_export_static void board_usb_pd_count_init(void)
{
	switch (uldrenite_get_typec_type()) {
	default:
		cached_usb_pd_port_count = 2;
		break;
	case ULDRENITE_TWO_C:
		cached_usb_pd_port_count = 2;
		break;
	case ULDRENITE_ONE_C:
		cached_usb_pd_port_count = 1;
		break;
	}
}
/*
 * Make sure setup is done after EEPROM is readable.
 */
DECLARE_HOOK(HOOK_INIT, board_usb_pd_count_init, HOOK_PRIO_INIT_I2C);
