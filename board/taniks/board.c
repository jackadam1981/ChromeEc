/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_ramp.h"
#include "charge_state_v2.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "driver/accel_lis2dw12.h"
#include "driver/accelgyro_lsm6dso.h"
#include "driver/als_tcs3400.h"
#include "fw_config.h"
#include "hooks.h"
#include "keyboard_raw.h"
#include "lid_switch.h"
#include "power_button.h"
#include "power.h"
#include "registers.h"
#include "switch.h"
#include "tablet_mode.h"
#include "throttle_ap.h"
#include "usbc_config.h"
#include "aw20198.h"
#include "rgb_keyboard.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/******************************************************************************/
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

void board_init(void)
{

	if (ec_cfg_has_tabletmode()) {

	} else {
		/* only clamshell todo */
		gpio_set_flags(GPIO_EC_VOLUP_BTN_ODL, GPIO_INPUT | GPIO_PULL_DOWN);
		gpio_set_flags(GPIO_EC_VOLDN_BTN_ODL, GPIO_INPUT | GPIO_PULL_DOWN);
		button_disable_gpio(BUTTON_VOLUME_UP);
		button_disable_gpio(BUTTON_VOLUME_DOWN);
	}
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

__override void board_kblight_shutdown(void)
{
	gpio_set_level(GPIO_EC_KB_BL_EN_L, 1);
}

__override void board_kblight_init(void)
{
	gpio_set_level(GPIO_RGBKBD_SDB_L, 1);
	gpio_set_level(GPIO_EC_KB_BL_EN_L, 0);
	msleep(10);
}

#ifdef CONFIG_CHARGE_RAMP_SW

/*
 * TODO: tune this threshold
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
	/* The GPIO is low when the battery is physically present */
	return gpio_get_level(GPIO_EC_BATT_PRES_ODL) ? BP_NO : BP_YES;
}

__override void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	/*
	 * Follow OEM request to limit the input current to
	 * 95% negotiated limit.
	 */
	charge_ma = charge_ma * 95 / 100;

	charge_set_input_current_limit(MAX(charge_ma,
					CONFIG_CHARGER_INPUT_CURRENT),
					charge_mv);
}

static int aw20198_read(struct rgbkbd *ctx, uint8_t addr, uint8_t *value)
{
	return i2c_xfer(ctx->cfg->i2c, AW20198_I2C_ADDR_FLAG,
			&addr, sizeof(addr), value, sizeof(*value));
}

static int aw20198_write(struct rgbkbd *ctx, uint8_t addr, uint8_t value)
{
	uint8_t buf[2] = {
		[0] = addr,
		[1] = value,
	};

	return i2c_xfer(ctx->cfg->i2c, AW20198_I2C_ADDR_FLAG,
			buf, sizeof(buf), NULL, 0);
}

static int aw20198_set_page(struct rgbkbd *ctx, uint8_t page)
{
	return aw20198_write(ctx, AW20198_REG_PAGE, page);
}

static int aw20198_get_config(struct rgbkbd *ctx, uint8_t addr, uint8_t *value)
{
	int rv = aw20198_set_page(ctx, AW20198_PAGE_FUNC);
	if (rv) {
		return rv;
	}

	return aw20198_read(ctx, addr, value);
}

static int aw20198_set_swsel(uint8_t num)
{
	uint8_t cfg;
	int rv = EC_SUCCESS;

	for (int i = 0; i < rgbkbd_count; i++) {
		struct rgbkbd *ctx = &rgbkbds[i];
		rv = aw20198_get_config(ctx, AW20198_REG_GCR, &cfg);

		cfg &= ~AW20198_REG_GCR_SWSEL_MASK;
		cfg |= (num << AW20198_REG_GCR_SWSEL_SHIFT);
		rv = aw20198_write(ctx, AW20198_REG_GCR, cfg);
	}
	return rv;
}

__override void board_aw20198_init(void)
{
	CPRINTS("taniks AW20198 init");

	/*
	 * Modify SWSEL bit4-7 from SW11 to SW8 to fulfill the LED
	 * module design layout
	 */
	aw20198_set_swsel(AW20198_REG_GCR_SW1_TO_SW8_ACTIVE);
}

__override void board_aw20198_set_scale(uint8_t *buf)
{
	CPRINTS("taniks AW20198 set scale");

	for (int i = 0; i < AW20198_REG_NUM_PAG2 / 3; i++) {
		/* Modify RED SLn to 190(60mA) for HW change the total Sink current to 80mA */
		*(buf + i*3) = 190;
	}
}
