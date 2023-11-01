/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Anraggar sub-board hardware configuration */

#include "cros_cbi.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "anraggar_sub_board.h"
#include "usbc/usb_muxes.h"

#include "task.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <ap_power/ap_power.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

static uint8_t cached_usb_pd_port_count;

__override uint8_t board_get_usb_pd_port_count(void)
{
	__ASSERT(cached_usb_pd_port_count != 0,
		 "sub-board detection did not run before a port count request");
	/* LCOV_EXCL_START - will not happen due to board_init is
	 * HOOK_PRIO_DEFAULT
	 */
	if (cached_usb_pd_port_count == 0)
		LOG_WRN("USB PD Port count not initialized!");
	/* LCOV_EXCL_STOP */
	return cached_usb_pd_port_count;
}

test_export_static enum anraggar_sub_board_type anraggar_cached_sub_board =
	ANRAGGAR_SB_UNKNOWN;
/*
 * Retrieve sub-board type from FW_CONFIG.
 */
enum anraggar_sub_board_type anraggar_get_sb_type(void)
{
	int ret;
	uint32_t val;

	/*
	 * Return cached value.
	 */
	if (anraggar_cached_sub_board != ANRAGGAR_SB_UNKNOWN)
		return anraggar_cached_sub_board;

	anraggar_cached_sub_board = ANRAGGAR_SB_NONE; /* Defaults to none */
	ret = cros_cbi_get_fw_config(FW_SUB_BOARD, &val);
	if (ret != 0) {
		LOG_WRN("Error retrieving CBI FW_CONFIG field %d",
			FW_SUB_BOARD);
		return anraggar_cached_sub_board;
	}

	switch (val) {
	case FW_SUB_BOARD_1:
		anraggar_cached_sub_board = ANRAGGAR_SB_C;
		LOG_INF("SB: USB type C");
		break;
	default:
		break;
	}
	return anraggar_cached_sub_board;
}

/*
 * Initialise the USB PD port count, which
 * depends on which sub-board is attached.
 */
test_export_static void board_usb_pd_count_init(void)
{
	switch (anraggar_get_sb_type()) {
	case ANRAGGAR_SB_NONE:
		cached_usb_pd_port_count = 1;
		break;
	default:
		cached_usb_pd_port_count = 2;
		break;
	}
}
/*
 * Make sure setup is done after EEPROM is readable.
 */
DECLARE_HOOK(HOOK_INIT, board_usb_pd_count_init, HOOK_PRIO_INIT_I2C);

// static void sub_board_power_handler(struct ap_power_ev_callback *cb,
// 				    struct ap_power_ev_data data)
// {
// 	/* Enable rails for S5 */
// 	const struct gpio_dt_spec *s5_rail =
// 		GPIO_DT_FROM_ALIAS(gpio_en_sub_s5_rails);
// 	switch (data.event) {
// 	case AP_POWER_PRE_INIT:
// 		LOG_DBG("Enabling sub-board power rails");
// 		gpio_pin_set_dt(s5_rail, 1);
// 		break;
// 	case AP_POWER_HARD_OFF:
// 		LOG_DBG("Disabling sub-board power rails");
// 		gpio_pin_set_dt(s5_rail, 0);
// 		break;
// 	default:
// 		LOG_ERR("Unhandled power event %d", data.event);
// 		break;
// 	}
// }

/**
 * Configure GPIOs (and other pin functions) that vary with present sub-board.
 */
static void anraggar_subboard_config(void)
{
	enum anraggar_sub_board_type sb = anraggar_get_sb_type();
	// static struct ap_power_ev_callback power_cb;

	// gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_en_sub_s5_rails),
	// 		      GPIO_DISCONNECTED);

#if CONFIG_USB_PD_PORT_MAX_COUNT > 1
	if (sb == ANRAGGAR_SB_C) {
		/* Configure interrupt input */
		// gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl),
		// 		      GPIO_INPUT | GPIO_PULL_UP);
	} else {
		/* Port doesn't exist, doesn't need muxing */
		USB_MUX_ENABLE_ALTERNATIVE(usb_mux_chain_1_no_mux);
	}
#endif

	switch (sb) {
	case ANRAGGAR_SB_C:
		/*
		 * LTE: Set up callbacks for enabling/disabling
		 * sub-board power on S5 state.
		 */
		// gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_en_sub_s5_rails),
		// 		      GPIO_OUTPUT_INACTIVE);
		/* Control sub_board power when CPU entering or
		 * exiting S5 state.
		 */
		// ap_power_ev_init_callback(&power_cb, sub_board_power_handler,
		// 			  AP_POWER_HARD_OFF |
		// 				  AP_POWER_PRE_INIT);
		// ap_power_ev_add_callback(&power_cb);
		break;

	default:
		break;
	}
}
DECLARE_HOOK(HOOK_INIT, anraggar_subboard_config, HOOK_PRIO_POST_FIRST);

/*
 * Enable interrupts
 */
static void board_init(void)
{
	/*
	 * Enable USB-C interrupts.
	 */
	// gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0));
// #if CONFIG_USB_PD_PORT_MAX_COUNT > 1
// 	if (board_get_usb_pd_port_count() == 2)
// 		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c1));
// #endif
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
