/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "cros_board_info.h"
#include "fw_config.h"
#include "gpio.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

static union taeko_cbi_fw_config fw_config;
BUILD_ASSERT(sizeof(fw_config) == sizeof(uint32_t));

/*
 * FW_CONFIG defaults for Taeko if the CBI.FW_CONFIG data is not
 * initialized.
 */
static const union taeko_cbi_fw_config fw_config_defaults = {
	.usb_db = DB_USB3_PS8815,
	.kb_bl = KEYBOARD_BACKLIGHT_ENABLED,
};
static bool db_is_plugged;
/****************************************************************************
 * Taeko FW_CONFIG access
 */
void board_init_fw_config(void)
{
	if (cbi_get_fw_config(&fw_config.raw_value)) {
		CPRINTS("CBI: Read FW_CONFIG failed, using board defaults");
		fw_config = fw_config_defaults;
	}

	if (get_board_id() == 0) {
		/*
		 * Early boards have a zero'd out FW_CONFIG, so replace
		 * it with a sensible default value. If DB_USB_ABSENT2
		 * was used as an alternate encoding of DB_USB_ABSENT to
		 * avoid the zero check, then fix it.
		 */
		if (fw_config.raw_value == 0) {
			CPRINTS("CBI: FW_CONFIG is zero, using board defaults");
			fw_config = fw_config_defaults;
		} else if (fw_config.usb_db == DB_USB_ABSENT2) {
			fw_config.usb_db = DB_USB_ABSENT;
		}
	}

	/*
	 * b/197585292
	 * If DB isn't plugged into dut, it may cause TCPC1 initialization
	 * abnormal.
	 * This is used to detect if DB is plugged into dut. If not, set it as
	 * DB_USB_ABSENT.
	 * DB connector still have NC pin, maybe we can select one as DB
	 * detection gpio next phase instead of USB_C1_RT_RST_R_ODL.
	 */

	gpio_set_level(GPIO_USB_C1_RT_RST_R_ODL, 1);
	msleep(5);
	if (gpio_get_level(GPIO_USB_C1_RT_RST_R_ODL) == 0)
		db_is_plugged = 0;
	else
		db_is_plugged = 1;
	gpio_set_level(GPIO_USB_C1_RT_RST_R_ODL, 0);
}

union taeko_cbi_fw_config get_fw_config(void)
{
	return fw_config;
}

enum ec_cfg_usb_db_type ec_cfg_usb_db_type(void)
{
	if (db_is_plugged)
		return fw_config.usb_db;
	else
		return DB_USB_ABSENT;
}
