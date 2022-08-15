/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//#include "common.h"
#include "console.h"
//#include "ec_commands.h"
#include "gpio.h"
//#include "gpio_signal.h"
#include "i2c.h"
//#include "i8042_protocol.h"
//#include "keyboard_8042.h"
//#include "keyboard_8042_sharedlib.h"
//#include "keyboard_customization.h"
#if 0 //for Nav key using CONFIG_USB_HID_KEYBOARD_VIVALDI
//#include "keyboard_protocol.h"
//#include "keyboard_scan.h"
#endif
//#include "lpc.h"
#if 0 //for Nav key using CONFIG_BRIGHTNESS_BUTTONS and CONFIG_MKBP_INPUT_DEVICES
#include "keyboard_protocol.h"
//#include "mkbp_input_devices.h"
#endif
#if 1 //for Nav key using CONFIG_KEYBOARD_PROTOCOL_8042
#include "keyboard_protocol.h"
#endif
//#include "power_button.h"
//#include "system.h"
//#include "test_util.h"
#include "timer.h"
//#include "util.h"

#define SCALER_NAV_KEY_REG 0x0381
#define SCALER_VOL_UP 0x01
#define SCALER_VOL_DOWN 0x02
#define SCALER_BRIGHTNESS_UP 0x04
#define SCALER_BRIGHTNESS_DOWN 0x08

#if 0 //for Nav key using CONFIG_USB_HID_KEYBOARD_VIVALDI
#if 1
extern const struct key {
	uint8_t row;
	uint8_t col;
} vivaldi_keys[];
#else
__overridable const struct key {
	uint8_t row;
	uint8_t col;
} vivaldi_keys[] = {
	{ .row = 0, .col = 2 }, /* T1 */
	{ .row = 3, .col = 2 }, /* T2 */
	{ .row = 2, .col = 2 }, /* T3 */
	{ .row = 1, .col = 2 }, /* T4 */
	{ .row = 3, .col = 4 }, /* T5 */
	{ .row = 2, .col = 4 }, /* T6 */
	{ .row = 1, .col = 4 }, /* T7 */
	{ .row = 2, .col = 9 }, /* T8 */
	{ .row = 1, .col = 9 }, /* T9 */
	{ .row = 0, .col = 4 }, /* T10 */
	{ .row = 0, .col = 1 }, /* T11 */
	{ .row = 1, .col = 5 }, /* T12 */
	{ .row = 3, .col = 5 }, /* T13 */
	{ .row = 0, .col = 9 }, /* T14 */
	{ .row = 0, .col = 11 }, /* T15 */
};
#endif

#if 1
struct keyboard_scan_config keyscan_config;
#else
__overridable struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 50,
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x1c, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};
#endif
#endif

//int8_t scaler_en; //for scaler test //raymondchung: ???
int scaler_[3]; //r,c,p  //for scaler test //raymondchung: ???


#if 0 //for Nav key using CONFIG_USB_HID_KEYBOARD_VIVALDI
static void simulate_key(enum action_key key_) //for Navi key  //raymondchung: ???done
{
	const struct ec_response_keybd_config *keybd = board_vivaldi_keybd_config();
	uint8_t row = 0, col = 0;

	for (int i = 0; i < keybd->num_top_row_keys; i++)
		if (key_ == keybd->action_keys[i])
		{
			row = vivaldi_keys[i].row;
			col = vivaldi_keys[i].col;
		}

	keyboard_state_changed(row, col, 1);
	keyboard_state_changed(row, col, 0);
}

static void nav_key_event(void) //for Nav key  //raymondchung: ???done
{
	int val = 0;
	uint8_t scaler_action_key = 0;

	if (i2c_read16(I2C_PORT_SCALER, I2C_ADDR_SCALER_FLAGS, SCALER_NAV_KEY_REG,
		      &val) == EC_SUCCESS)
	{
		if (val == SCALER_VOL_UP)
			scaler_action_key = TK_VOL_UP;
		if (val == SCALER_VOL_DOWN)
			scaler_action_key = TK_VOL_DOWN;
		if (val == SCALER_BRIGHTNESS_UP)
			scaler_action_key = TK_BRIGHTNESS_UP;
		if (val == SCALER_BRIGHTNESS_DOWN)
			scaler_action_key = TK_BRIGHTNESS_DOWN;
	}

	if (val != 0)
		simulate_key(scaler_action_key);
}

void osd_int_interrupt(enum gpio_signal signal)
{
	nav_key_event();
}
#endif

#if 1 //for Nav key using CONFIG_KEYBOARD_PROTOCOL_8042
static void simulate_key(enum keyboard_button_type key_) //for Nav key  //raymondchung: ???done
{
	keyboard_update_button(key_, 1);
	keyboard_update_button(key_, 0);
}

static void nav_key_event(void) //for Navi key  //raymondchung: ???done
{
	int val = 0;
	uint8_t scaler_action_key = 0;

	if (i2c_read16(I2C_PORT_SCALER, I2C_ADDR_SCALER_FLAGS, SCALER_NAV_KEY_REG,
		      &val) == EC_SUCCESS)
	{
		if (val == SCALER_VOL_UP)
			scaler_action_key = KEYBOARD_BUTTON_VOLUME_UP;
		if (val == SCALER_VOL_DOWN)
			scaler_action_key = KEYBOARD_BUTTON_VOLUME_DOWN;
		if (val == SCALER_BRIGHTNESS_UP)
			scaler_action_key = KEYBOARD_BUTTON_BRIGHTNESS_UP;
		if (val == SCALER_BRIGHTNESS_DOWN)
			scaler_action_key = KEYBOARD_BUTTON_BRIGHTNESS_DOWN;
	}

	if (val != 0)
		simulate_key(scaler_action_key);
}

/*
 * TODO: Need to change the long press function.
 * How to implementation?
 */
void osd_int_interrupt(enum gpio_signal signal)
{
	nav_key_event();
}
#endif

/*
 * TODO: Need to add the display mode function.
 * How to implementation?
 */
void disp_mode_interrupt(enum gpio_signal signal)
{
	//raymondchung: ???

}

void hdmi0_cable_det_interrupt(enum gpio_signal signal)
{
	gpio_set_level(GPIO_EC_OVERRIDE_SCLR_EN, 1);
	gpio_set_level(GPIO_EC_12VSC_EN, 1);
	gpio_set_level(GPIO_EC_AMP_SD, 1);
}

void scaler_test(void) //for scaler test //raymondchung: ???
{
#if 0 //for Nav key using CONFIG_USB_HID_KEYBOARD_VIVALDI
	//const struct key *scaler_key;
	//const struct ec_response_keybd_config *scaler_keybd = board_vivaldi_keybd_config();

	//uint8_t scaler_row = 0, scaler_col = 0;
	uint8_t scaler_action_key;

	//if (scaler_en == 1)
	//{
	//simulate_key(1, 1, 0);
	//keyboard_state_changed(1, 4, 1); //row, col, is_pressed
	//keyboard_state_changed(1, 4, 0); //row, col, is_pressed
	//}

	//keyboard_state_changed(scaler_[0], scaler_[1], scaler_[2]);

	/* scaler_[2] 0: V_u, 1: V_d, 2: B_u, B_d */
	if (scaler_[2] == 0)
		scaler_action_key = TK_VOL_UP;
	if (scaler_[2] == 1)
		scaler_action_key = TK_VOL_DOWN;
	if (scaler_[2] == 2)
		scaler_action_key = TK_BRIGHTNESS_UP;
	if (scaler_[2] == 3)
		scaler_action_key = TK_BRIGHTNESS_DOWN;

	simulate_key(scaler_action_key);
#endif
#if 0 //for Nav key using CONFIG_BRIGHTNESS_BUTTONS and CONFIG_MKBP_INPUT_DEVICES
	uint8_t scaler_action_key;

	/* scaler_[2] 0: V_u, 1: V_d, 2: B_u, B_d */
	if (scaler_[2] == 0)
		scaler_action_key = KEYBOARD_BUTTON_VOLUME_UP;
	if (scaler_[2] == 1)
		scaler_action_key = KEYBOARD_BUTTON_VOLUME_DOWN;
	if (scaler_[2] == 2)
		scaler_action_key = KEYBOARD_BUTTON_BRIGHTNESS_UP;
	if (scaler_[2] == 3)
		scaler_action_key = KEYBOARD_BUTTON_BRIGHTNESS_DOWN;

	//mkbp_button_update(scaler_action_key, 1);
	//msleep(500);
	//mkbp_button_update(scaler_action_key, 0);

	keyboard_update_button(scaler_action_key, 1);
	keyboard_update_button(scaler_action_key, 0);
#endif
}
