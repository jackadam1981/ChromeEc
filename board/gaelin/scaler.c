/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//#include "common.h"
#include "console.h"
//#include "ec_commands.h"
//#include "gpio.h"
//#include "i8042_protocol.h"
//#include "keyboard_8042.h"
//#include "keyboard_8042_sharedlib.h"
//#include "keyboard_customization.h"
#include "keyboard_protocol.h"
//#include "keyboard_scan.h"
//#include "lpc.h"
//#include "power_button.h"
//#include "system.h"
//#include "test_util.h"
//#include "timer.h"
//#include "util.h"

extern const struct key {
	uint8_t row;
	uint8_t col;
} vivaldi_keys[];

static void simulate_key(enum action_key key);

//int8_t scaler_en; //for scaler test //raymondchung: ???
int scaler_[3]; //r,c,p  //for scaler test //raymondchung: ???


static void simulate_key(enum action_key key) //for Navi key  //raymondchung: ???done
{
	const struct ec_response_keybd_config *keybd = board_vivaldi_keybd_config();
	uint8_t row = 0, col = 0;

	for(int i = 0; i < keybd->num_top_row_keys; i++)
		if(key == keybd->action_keys[i])
		{
			row = vivaldi_keys[i].row;
			col = vivaldi_keys[i].col;
		}

	keyboard_state_changed(row, col, 1);
	keyboard_state_changed(row, col, 0);
}





void scaler_test(void) //for scaler test //raymondchung: ???
{
#if 1 //for Navi key
	//const struct key *scaler_key;
	//const struct ec_response_keybd_config *scaler_keybd = board_vivaldi_keybd_config();

	//uint8_t scaler_row = 0, scaler_col = 0;
	uint8_t scaler_action_keys;

	//if(scaler_en == 1)
	//{
	//simulate_key(1, 1, 0);
	//keyboard_state_changed(1, 4, 1); //row, col, is_pressed
	//keyboard_state_changed(1, 4, 0); //row, col, is_pressed
	//}

	//keyboard_state_changed(scaler_[0], scaler_[1], scaler_[2]);

	/* scaler_[2] 0: B_d, 1: B_u, 2: V_d, V_u */
	if(scaler_[2] == 0)
		scaler_action_keys = TK_BRIGHTNESS_DOWN;
	if(scaler_[2] == 1)
		scaler_action_keys = TK_BRIGHTNESS_UP;
	if(scaler_[2] == 2)
		scaler_action_keys = TK_VOL_DOWN;
	if(scaler_[2] == 3)
		scaler_action_keys = TK_VOL_UP;

	simulate_key(scaler_action_keys);
#endif


}
