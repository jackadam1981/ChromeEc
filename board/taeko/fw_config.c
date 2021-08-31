/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "cros_board_info.h"
#include "driver/tcpm/ps8xxx_public.h"
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

#ifdef CONFIG_SYSTEM_UNLOCKED
/* DB should be plugged as default */
static bool db_is_plugged = 1;

/*
 * b/197585292
 * If DB isn't plugged into dut, it may cause TCPC1 initialization abnormal.
 * This is used to detect if DB is plugged or not.
 */
static bool board_detect_db_is_plugged(void)
{
	int val, rv;

	gpio_set_level(GPIO_USB_C1_RT_RST_R_ODL, 1);
	msleep(GENERIC_MAX(PS8XXX_RESET_DELAY_MS,
			PS8815_PWR_H_RST_H_DELAY_MS));

	rv = i2c_read8(I2C_PORT_USB_C1_TCPC,
		PS8751_I2C_ADDR1_FLAGS, 0x00, &val);

	gpio_set_level(GPIO_USB_C1_RT_RST_R_ODL, 0);
	msleep(PS8815_FW_INIT_DELAY_MS);

	return (rv == EC_SUCCESS);
}
#endif /* CONFIG_SYSTEM_UNLOCKED */

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
		 * Early boards doesn't have correct FW_CONFIG, so replace
		 * it with a sensible default value.
		 */
		CPRINTS("CBI: Using board defaults for early board");
		fw_config = fw_config_defaults;

	#ifdef CONFIG_SYSTEM_UNLOCKED
		db_is_plugged = board_detect_db_is_plugged();
	#endif /* CONFIG_SYSTEM_UNLOCKED */

	}
}

union taeko_cbi_fw_config get_fw_config(void)
{
	return fw_config;
}

enum ec_cfg_usb_db_type ec_cfg_usb_db_type(void)
{
#ifdef CONFIG_SYSTEM_UNLOCKED
	if (!db_is_plugged)
		return DB_USB_ABSENT;
#endif /* CONFIG_SYSTEM_UNLOCKED */
	return fw_config.usb_db;
}

bool ec_cfg_has_keyboard_backlight(void)
{
	return (fw_config.kb_bl == KEYBOARD_BACKLIGHT_ENABLED);
}
