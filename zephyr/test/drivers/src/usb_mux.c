/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "common.h"
#include "ec_tasks.h"
#include "emul/emul_usb_mux_mock.h"
#include "hooks.h"
#include "i2c.h"
#include "stubs.h"
#include "usb_prl_sm.h"
#include "usb_tc_sm.h"

#include "usb_mux.h"

/** Test usb_mux init */
static void test_usb_mux_init(void)
{
	int i, calls;

	emul_usb_mux_mock_reset();

	usb_mux_init(USBC_PORT_C1);
	for (i = 0; i < 3; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
	}

	emul_usb_mux_mock_reset();
	usb_mux_mock_data[1].init_ret = EC_ERROR_NOT_POWERED;

	usb_mux_init(USBC_PORT_C1);
	/*
	 * Muxes that are in chain after the one which fails shouldn't be
	 * called
	 */
	for (i = 0; i < 2; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
	}
	calls = usb_mux_mock_data[2].num_init_calls;
	zassert_equal(0, calls, "usb_mux_mock 2 init called %d times (!= 0)",
		      calls);

	/* Test board init callback */
	emul_usb_mux_mock_reset();
	usbc1_usb_mux_mock1.board_init = usbc1_usb_mux_mock1.driver->init;

	usb_mux_init(USBC_PORT_C1);

	calls = usb_mux_mock_data[0].num_init_calls;
	zassert_equal(1, calls, "usb_mux_mock 0 init called %d times (!= 1)",
		      calls);
	/*
	 * board_init of second mux mock is set to init mock function, so it
	 * should be called two times.
	 */
	calls = usb_mux_mock_data[1].num_init_calls;
	zassert_equal(2, calls, "usb_mux_mock 1 init called %d times (!= 2)",
		      calls);
	calls = usb_mux_mock_data[2].num_init_calls;
	zassert_equal(1, calls, "usb_mux_mock 2 init called %d times (!= 1)",
		      calls);
	usbc1_usb_mux_mock1.board_init = NULL;
}

static int mock_board_set_calls;

static int mock_board_set(const struct usb_mux *me, mux_state_t mux_state)
{
	mock_board_set_calls++;

	return EC_SUCCESS;
}

/** Test usb_mux setting mux mode */
static void test_usb_mux_set(void)
{
	mux_state_t exp_mode, mode;
	int i, calls;

	/* usb mux mock 1 shouldn't be set with polarity mode */
	usbc1_usb_mux_mock1.flags = USB_MUX_FLAG_SET_WITHOUT_FLIP;

	/* Makes sure that usb muxes of port 1 are not init */
	usb_muxes[USBC_PORT_C1].flags = USB_MUX_FLAG_RESETS_IN_G3;
	hook_notify(HOOK_CHIPSET_HARD_OFF);

	/* Set AP to normal state to init BB retimer */
	set_mock_power_state(POWER_S0);
	k_msleep(1);

	/* Test setting mux mode without polarity inversion */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		/*
		 * Expect that init will be called when set is used on
		 * unitialised port
		 */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test setting mux mode with polarity inversion */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_TBT_COMPAT_ENABLED;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    1 /* = polarity */);
	for (i = 0; i < 3; i++) {
		/* init should be called by previous set */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
		/* usb mux mock 1 shouldn't have polarity set */
		if (i == 1) {
			exp_mode &= ~USB_PD_MUX_POLARITY_INVERTED;
		} else {
			exp_mode |= USB_PD_MUX_POLARITY_INVERTED;
		}
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test board set callback */
	mock_board_set_calls = 0;
	usbc1_usb_mux_mock1.board_set = &mock_board_set;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    1 /* = polarity */);
	zassert_equal(1, mock_board_set_calls,
		      "board set callback called %d times (!= 1)",
		      mock_board_set_calls);

	/* Test set function with error in usb_mux */
	emul_usb_mux_mock_reset();
	usb_mux_mock_data[1].set_ret = EC_ERROR_UNKNOWN;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    1 /* = polarity */);
	for (i = 0; i < 2; i++) {
		/* init should be called by previous get */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
	}
	/* Last usb mux mock shouldn't be called after previous returns error */
	calls = usb_mux_mock_data[2].num_init_calls;
	zassert_equal(0, calls, "usb_mux_mock 2 init called %d times (!= 0)",
		      calls);
	calls = usb_mux_mock_data[2].num_set_calls;
	zassert_equal(0, calls, "usb_mux_mock 2 set called %d times (!= 0)",
		      calls);
}

/** Test usb_mux reset in g3 when required flag is set */
static void test_usb_mux_reset_in_g3(void)
{
	int i, calls;

	/* Should deinit usb muxes of port 1 */
	usb_muxes[USBC_PORT_C1].flags = USB_MUX_FLAG_RESETS_IN_G3;
	hook_notify(HOOK_CHIPSET_HARD_OFF);

	/* Test that init is called */
	emul_usb_mux_mock_reset();
	usb_mux_set(USBC_PORT_C1, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		/*
		 * Expect that init will be called when set is used on
		 * unitialised port
		 */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
	}

	/* Usb muxes of port 1 should stay initialised */
	usb_muxes[USBC_PORT_C1].flags = 0;
	hook_notify(HOOK_CHIPSET_HARD_OFF);

	/* Test that init is not called */
	emul_usb_mux_mock_reset();
	usb_mux_set(USBC_PORT_C1, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
	}
}

/** Test usb_mux getting mux mode */
static void test_usb_mux_get(void)
{
	mux_state_t exp_mode, mode;
	int i, calls;

	/* Makes sure that usb muxes of port 1 are not init */
	usb_muxes[USBC_PORT_C1].flags = USB_MUX_FLAG_RESETS_IN_G3;
	hook_notify(HOOK_CHIPSET_HARD_OFF);

	/* Test getting mux mode */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	/* Include result of virtual usb_mux */
	usbc1_virtual_usb_mux.driver->get(&usbc1_virtual_usb_mux, &mode);
	exp_mode |= mode;

	mode = usb_mux_get(USBC_PORT_C1);
	zassert_equal(exp_mode, mode, "mode is 0x%x (!= 0x%x)", mode, exp_mode);
	for (i = 0; i < 3; i++) {
		/*
		 * Expect that init will be called when get is used on
		 * unitialised port
		 */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_get_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d get called %d times (!= 1)",
			      i, calls);
	}

	/* Test getting mux mode with one inverted polarisation */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_TBT_COMPAT_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	usb_mux_mock_data[1].state |= USB_PD_MUX_POLARITY_INVERTED;
	exp_mode |= USB_PD_MUX_POLARITY_INVERTED;
	/* Include result of virtual usb_mux */
	usbc1_virtual_usb_mux.driver->get(&usbc1_virtual_usb_mux, &mode);
	exp_mode |= mode;

	mode = usb_mux_get(USBC_PORT_C1);
	zassert_equal(exp_mode, mode, "mode is 0x%x (!= 0x%x)", mode, exp_mode);
	for (i = 0; i < 3; i++) {
		/* init should be called by previous get */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_get_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d get called %d times (!= 1)",
			      i, calls);
	}

	/* Test get function with error in usb_mux */
	emul_usb_mux_mock_reset();
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = USB_PD_MUX_TBT_COMPAT_ENABLED;
	}
	usb_mux_mock_data[1].get_ret = EC_ERROR_UNKNOWN;
	exp_mode = USB_PD_MUX_NONE;

	mode = usb_mux_get(USBC_PORT_C1);
	zassert_equal(exp_mode, mode, "mode is 0x%x (!= 0x%x)", mode, exp_mode);
	for (i = 0; i < 2; i++) {
		/* init should be called by previous get */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_get_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d get called %d times (!= 1)",
			      i, calls);
	}
	/* Last usb mux mock shouldn't be called after previous returns error */
	calls = usb_mux_mock_data[2].num_init_calls;
	zassert_equal(0, calls, "usb_mux_mock 2 init called %d times (!= 0)",
		      calls);
	calls = usb_mux_mock_data[2].num_get_calls;
	zassert_equal(0, calls, "usb_mux_mock 2 get called %d times (!= 0)",
		      calls);
}

/** Test usb_mux entering and exiting low power mode */
static void test_usb_mux_low_power_mode(void)
{
	mux_state_t exp_mode, mode;
	int i, calls, exp_calls;

	/*
	 * Virtual mux return ack_required in some cases, but this requires to
	 * run usb_mux_set in TASK_PD_C1 context. Remove virtual mux from chain
	 * for this test.
	 *
	 * TODO: Find way to setup PD stack in such state that notifing PD task
	 *       results in required usb_mux_set call.
	 */
	usbc1_bb_retimer.next_mux = NULL;

	/* Makes sure that usb muxes of port 1 are init */
	usb_mux_init(USBC_PORT_C1);

	/* Test enter to low power mode */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_NONE;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_DISCONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
		calls = usb_mux_mock_data[i].num_enter_lpm_calls;
		zassert_equal(1, calls,
			     "usb_mux_mock %d enter_lpm called %d times (!= 1)",
			     i, calls);
	}

	/* Test that nothing is changed when already in low power mode */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_NONE;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_DISCONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d set called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_enter_lpm_calls;
		zassert_equal(0, calls,
			     "usb_mux_mock %d enter_lpm called %d times (!= 0)",
			     i, calls);
	}

	/* Test that get return USB_PD_MUX_NONE in low poer mode */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_NONE;
	mode = usb_mux_get(USBC_PORT_C1);
	zassert_equal(exp_mode, mode, "mode is 0x%x (!= 0x%x)", mode, exp_mode);
	for (i = 0; i < 3; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d set called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_enter_lpm_calls;
		zassert_equal(0, calls,
			     "usb_mux_mock %d enter_lpm called %d times (!= 0)",
			     i, calls);
	}

	/* Test exiting from low power mode */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		/* Expect that init will be called on low power mode exit */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test exiting from lpm, when init end with EC_ERROR_NOT_POWERED */ 
	emul_usb_mux_mock_reset();
	usb_mux_mock_data[1].init_ret = EC_ERROR_NOT_POWERED;
	usb_mux_init(USBC_PORT_C1);
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		/* Expect that init will be called on low power mode exit */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test enter to low power mode with polarity */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_NONE;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_DISCONNECT,
		    1 /* = polarity */);
	for (i = 0; i < 3; i++) {
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d set called %d times (!= 1)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
		calls = usb_mux_mock_data[i].num_enter_lpm_calls;
		zassert_equal(1, calls,
			     "usb_mux_mock %d enter_lpm called %d times (!= 1)",
			     i, calls);
	}

	/* Test that nothing is changed on lpm exit error */
	emul_usb_mux_mock_reset();
	usb_mux_mock_data[1].init_ret = EC_ERROR_NOT_POWERED;
	exp_mode = USB_PD_MUX_USB_ENABLED;
	usb_mux_set(USBC_PORT_C1, exp_mode, USB_SWITCH_CONNECT,
		    0 /* = polarity */);
	for (i = 0; i < 3; i++) {
		if (i == 2) {
			exp_calls = 0;
		} else {
			/* Trying to exit low power mode */
			exp_calls = 1;
		}
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(exp_calls, calls,
			      "usb_mux_mock %d init called %d times (!= %d)",
			      i, calls, exp_calls);
		calls = usb_mux_mock_data[i].num_set_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d set called %d times (!= 0)",
			      i, calls);
		calls = usb_mux_mock_data[i].num_enter_lpm_calls;
		zassert_equal(0, calls,
			     "usb_mux_mock %d enter_lpm called %d times (!= 0)",
			     i, calls);
	}

	/* Restore usb muxes chain */
	usbc1_bb_retimer.next_mux = &usbc1_virtual_usb_mux;
}

/** Test usb_mux flip */
static void test_usb_mux_flip(void)
{
	mux_state_t exp_mode, mode;
	int i, calls;

	/* usb mux mock 1 shouldn't be set with polarity mode */
	usbc1_usb_mux_mock1.flags = USB_MUX_FLAG_SET_WITHOUT_FLIP;

	/* Makes sure that usb muxes of port 1 are not init */
	usb_muxes[USBC_PORT_C1].flags = USB_MUX_FLAG_RESETS_IN_G3;
	hook_notify(HOOK_CHIPSET_HARD_OFF);

	/* Test flip port without polarity inverted */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	/* Include result of virtual usb_mux */
	usbc1_virtual_usb_mux.driver->get(&usbc1_virtual_usb_mux, &mode);
	exp_mode |= mode;
	usb_mux_flip(USBC_PORT_C1);
	for (i = 0; i < 3; i++) {
		/*
		 * Expect that init will be called when flip is used on
		 * unitialised port
		 */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
		/* usb mux mock 1 shouldn't have polarity set because of flag */
		if (i == 1) {
			exp_mode &= ~USB_PD_MUX_POLARITY_INVERTED;
		} else {
			exp_mode |= USB_PD_MUX_POLARITY_INVERTED;
		}
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test flip port with polarity inverted */
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].num_init_calls = 0;
	}
	exp_mode &= ~USB_PD_MUX_POLARITY_INVERTED;
	usb_mux_flip(USBC_PORT_C1);
	for (i = 0; i < 3; i++) {
		/* init should be called by previous flip */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}
}

void test_usb_mux_hpd_update(void)
{
	mux_state_t exp_mode, mode, virt_mode;
	int i, calls;

	/* Makes sure that usb muxes of port 1 are not init */
	usb_muxes[USBC_PORT_C1].flags = USB_MUX_FLAG_RESETS_IN_G3;
	hook_notify(HOOK_CHIPSET_HARD_OFF);

	usbc1_virtual_usb_mux.driver->get(&usbc1_virtual_usb_mux, &virt_mode);

	/* Test no hpd level and no irq */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	/* Include result of virtual usb_mux */
	exp_mode |= virt_mode;
	usb_mux_hpd_update(USBC_PORT_C1, 0, 0);
	for (i = 0; i < 3; i++) {
		/*
		 * Expect that init will be called when flip is used on
		 * unitialised port
		 */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(1, calls,
			      "usb_mux_mock %d init called %d times (!= 1)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test hpd level and irq */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	exp_mode |= USB_PD_MUX_HPD_LVL | USB_PD_MUX_HPD_IRQ;
	/* Include result of virtual usb_mux */
	exp_mode |= virt_mode;
	usb_mux_hpd_update(USBC_PORT_C1, 1, 1);
	for (i = 0; i < 3; i++) {
		/* init should be called by previous hpd update */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test no hpd level and irq */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	exp_mode |= USB_PD_MUX_HPD_IRQ;
	/* Include result of virtual usb_mux */
	exp_mode |= virt_mode;
	usb_mux_hpd_update(USBC_PORT_C1, 0, 1);
	for (i = 0; i < 3; i++) {
		/* init should be called by previous hpd update */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}

	/* Test hpd level and no irq */
	emul_usb_mux_mock_reset();
	exp_mode = USB_PD_MUX_USB_ENABLED;
	for (i = 0; i < 3; i++) {
		usb_mux_mock_data[i].state = exp_mode;
	}
	exp_mode |= USB_PD_MUX_HPD_LVL;
	/* Include result of virtual usb_mux */
	exp_mode |= virt_mode;
	usb_mux_hpd_update(USBC_PORT_C1, 1, 0);
	for (i = 0; i < 3; i++) {
		/* init should be called by previous hpd update */
		calls = usb_mux_mock_data[i].num_init_calls;
		zassert_equal(0, calls,
			      "usb_mux_mock %d init called %d times (!= 0)",
			      i, calls);
		mode = usb_mux_mock_data[i].state;
		zassert_equal(exp_mode, mode,
			      "usb_mux_mock %d mode is 0x%x (!= 0x%x)",
			      i, mode, exp_mode);
	}
}

void test_suite_usb_mux(void)
{
	ztest_test_suite(usb_mux,
			 ztest_user_unit_test(test_usb_mux_init),
			 ztest_user_unit_test(test_usb_mux_set),
			 ztest_user_unit_test(test_usb_mux_reset_in_g3),
			 ztest_user_unit_test(test_usb_mux_get),
			 ztest_user_unit_test(test_usb_mux_low_power_mode),
			 ztest_user_unit_test(test_usb_mux_flip),
			 ztest_user_unit_test(test_usb_mux_hpd_update));
	ztest_run_test_suite(usb_mux);
}
