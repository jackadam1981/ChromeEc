/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "button.h"
#include "board.h"
#include "charge_ramp.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "driver/accel_bma2x2_public.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/charger/isl9241.h"
#include "fw_config.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power_button.h"
#include "power.h"
#include "registers.h"
#include "switch.h"
#include "tablet_mode.h"
#include "throttle_ap.h"
#include "usbc_config.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/******************************************************************************/
static int fake_state_of_charge = -1;

/* USB-A charging control */

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USBA_R,
};
BUILD_ASSERT(ARRAY_SIZE(usb_port_enable) == USB_PORT_COUNT);

/******************************************************************************/

__override void board_cbi_init(void)
{
	config_usb_db_type();
}

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	/* Allow keyboard backlight to be enabled */

	/* TODO(b/190783131)
	 * Need to implement specific keyboard backlight control method.
	 */
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	/* Turn off the keyboard backlight if it's on. */

	/* TODO(b/190783131)
	 * Need to implement specific keyboard backlight control method.
	 */
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_CHARGE_RAMP_SW

/*
 * TODO(b/181508008): tune this threshold
 */

#define BC12_MIN_VOLTAGE 4400

/**
 * Return true if VBUS is too low
 */
int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	int voltage;

	if (charger_get_vbus_voltage(port, &voltage))
		voltage = 0;

	if (voltage == 0) {
		CPRINTS("%s: must be disconnected", __func__);
		return 1;
	}

	if (voltage < BC12_MIN_VOLTAGE) {
		CPRINTS("%s: port %d: vbus %d lower than %d", __func__,
			port, voltage, BC12_MIN_VOLTAGE);
		return 1;
	}

	return 0;
}

#endif /* CONFIG_CHARGE_RAMP_SW */

enum battery_present battery_hw_present(void)
{
	enum gpio_signal batt_pres;

	batt_pres = GPIO_EC_BATT_PRES_ODL;

	/* The GPIO is low when the battery is physically present */
	return gpio_get_level(batt_pres) ? BP_NO : BP_YES;
}

static enum ec_status
host_command_get_chg_info(struct host_cmd_handler_args *args)
//static void host_command_get_chg_info(void)
{
	const struct ec_params_get_chg_info *p = args->params;
	struct ec_response_get_chg_info *r1 = args->response;
	struct batt_params batt_new = {0};
	int voltage=0;
	int current=0;
	int cycle_count=0;


	if(p->index !=0)
		return EC_RES_SUCCESS;

	/* RSOC */
	if (sb_read(SB_RELATIVE_STATE_OF_CHARGE, &batt_new.state_of_charge)
	    && fake_state_of_charge < 0)
		batt_new.flags |= BATT_FLAG_BAD_STATE_OF_CHARGE;
	r1->RSOC = batt_new.state_of_charge;

	/*Get battery current and voltage*/
	if (sb_read(SB_VOLTAGE, &batt_new.voltage))
		batt_new.flags |= BATT_FLAG_BAD_VOLTAGE;
	r1->charge_voltage = batt_new.voltage;

	if (sb_read(SB_CURRENT, &batt_new.current))
		batt_new.flags |= BATT_FLAG_BAD_CURRENT;
	else
		batt_new.current = (int16_t)batt_new.current;
	r1->charge_current = batt_new.current;

	i2c_read16(I2C_PORT_CHARGER,
			  ISL9241_ADDR_FLAGS,
			  ISL9241_REG_CHG_CURRENT_LIMIT, &current);
	r1->ChargingCurrent = current;

	i2c_read16(I2C_PORT_CHARGER,
			  ISL9241_ADDR_FLAGS,
			  ISL9241_REG_MAX_SYSTEM_VOLTAGE, &voltage);
	r1->ChargingVoltage = voltage;

	sb_read(SB_REMAINING_CAPACITY, &batt_new.remaining_capacity);
	r1->remaining_capacity = batt_new.remaining_capacity;

	sb_read(SB_FULL_CHARGE_CAPACITY, &batt_new.full_capacity);
	r1->full_capacity = batt_new.full_capacity;

	sb_read(SB_CYCLE_COUNT, &cycle_count);
	r1->cycle_count = cycle_count;

	if (sb_read(SB_TEMPERATURE, &batt_new.temperature))
		batt_new.flags |= BATT_FLAG_BAD_TEMPERATURE;
	r1->temp = batt_new.temperature;

	args->response_size = sizeof(*r1);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CHARGER_INFO, host_command_get_chg_info, EC_VER_MASK(0));