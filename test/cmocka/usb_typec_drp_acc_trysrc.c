/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB Type-C Dual Role Port, Audio Accessory, and Try.SRC Device module.
 */
#include "mock/tcpc_mock.h"
#include "mock/usb_mux_mock.h"
#include "task.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_tc_sm.h"
#include "battery.h"

#include "test.h"

#define PORT0 0
#define USBC_EVENT_TIMEOUT (5 * MSEC)

/* Fake task_id */
#define TASK_ID_PD_C0 0

/*
 * Amount of time to wait after a specified timeout. Allows for an extra loop
 * through statemachine plus 1000 calls to clock.
 */
#define FUDGE (6 * MSEC)

/*
 * ToDo: Below functions are needed only for linking, it may be beneficial
 * to place them in some common location if other tests may also make use out
 * of them.
 */
test_mockable enum battery_present battery_is_present(void)
{
	return 1;
}

uint8_t pd_get_src_cap_cnt(int port)
{
	return 0;
}

const uint32_t * const pd_get_src_caps(int port)
{
	return NULL;
}

__overridable void pd_request_power_swap(int port)
{
}

void pd_set_src_caps(int port, int cnt, uint32_t *src_caps)
{
}

__overridable void pe_invalidate_explicit_contract(int port)
{
}

void tc_start_event_loop(int port)
{
}

int tc_restart_tcpc(int port)
{
	return tcpm_init(port);
}

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.drv = &mock_tcpc_driver,
	},
};
const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.driver = &mock_usb_mux_driver,
	}
};

static void test_polarity_cc1_default(void **state)
{
	int i = 0;

	/* Update CC lines */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_RP_DEF;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.vbus_level = 1;

	/* Run in the context of PD task on Port0 */
	will_return_maybe(task_get_current, TASK_ID_PD_C0);

	/*
	 * In this test we are expecting value of polarity, which is set by
	 * default for tcpc mock. Initialize it with something else, in order
	 * to catch possible errors.
	 */
	mock_tcpc.last.polarity = POLARITY_COUNT;

	/*
	 * Run TypeC state machine on port0. After tCCDebounce time, we should
	 * SRC, so run state machine up until then.
	 */
	for (i = 0, tc_run(PORT0);
	     i < ((PD_T_CC_DEBOUNCE + FUDGE)/USBC_EVENT_TIMEOUT) + 1;
	     i++) {
		udelay(USBC_EVENT_TIMEOUT);
		tc_run(PORT0);
	}

	assert_int_equal(mock_tcpc.last.polarity, POLARITY_CC1);
}

static void test_try_src_disabled(void **state)
{
	int i = 0;

	/* Update CC lines */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_RP_3_0;
	mock_tcpc.vbus_level = 1;

	/* Run in the context of PD task on Port0 */
	will_return_maybe(task_get_current, TASK_ID_PD_C0);

	tc_try_src_override(TRY_SRC_OVERRIDE_OFF);

	/* Init TypeC state machine */
	tc_state_init(PORT0);

	/*
	 * Run TypeC state machine on port0. Give enough time for many
	 * potential transitions.
	 */
	for (i = 0, tc_run(PORT0);
	     i < ((10 * SECOND)/USBC_EVENT_TIMEOUT) + 1;
	     i++) {
		udelay(USBC_EVENT_TIMEOUT);
		tc_run(PORT0);
	}

	assert_int_equal(mock_tcpc.last.polarity, POLARITY_CC2);
	assert_int_equal(mock_tcpc.last.cc, TYPEC_CC_RD);
	assert_int_equal(mock_tcpc.last.power_role, PD_ROLE_SINK);
	assert_int_equal(mock_tcpc.last.data_role, PD_ROLE_UFP);
	assert_true(tc_is_attached_snk(PORT0));
}

static void test_mux_con_dis_as_src(void **state)
{
	int i;

	/* Update CC lines */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_RD;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.vbus_level = 0;
	mock_usb_mux.num_set_calls = 0;

	/* Run in the context of PD task on Port0 */
	will_return_maybe(task_get_current, TASK_ID_PD_C0);

	pd_set_dual_role(PORT0, PD_DRP_TOGGLE_ON);

	tc_event_check(PORT0, PD_EVENT_UPDATE_DUAL_ROLE);

	/*
	 * Run TypeC state machine on port0. This transition through
	 * AttachWait.SRC then to Attached.SRC
	 */
	for (i = 0, tc_run(PORT0); i < SECOND/USBC_EVENT_TIMEOUT + 1; i++) {
		udelay(USBC_EVENT_TIMEOUT);
		tc_run(PORT0);
	}

	/* We should be in Attached.SRC now */
	assert_int_equal(mock_usb_mux.state, USB_PD_MUX_USB_ENABLED);
	assert_int_equal(mock_usb_mux.num_set_calls, 1);

	mock_tcpc.cc1 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;

	/*
	 * Run TypeC state machine on port0. This transition through
	 * TryWait.SNK then to Unattached.SNK.
	 */
	for (i = 0, tc_run(PORT0);
	     i < 10 * SECOND/USBC_EVENT_TIMEOUT + 1;
	     i++) {
		udelay(USBC_EVENT_TIMEOUT);
		tc_run(PORT0);
	}

	/* We are in Unattached.SNK. The mux should have detached */
	assert_int_equal(mock_usb_mux.state, USB_PD_MUX_NONE);
	assert_int_equal(mock_usb_mux.num_set_calls, 2);
}

static int global_test_setup(void **state)
{
	mock_usb_mux_reset();
	mock_tcpc_reset();

	tc_try_src_override(TRY_SRC_NO_OVERRIDE);

	tc_restart_tcpc(PORT0);

	/* Run in the context of PD task on Port0 */
	will_return_maybe(task_get_current, TASK_ID_PD_C0);

	/* Init TypeC state machine */
	tc_state_init(PORT0);

	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_polarity_cc1_default),
		cmocka_unit_test(test_try_src_disabled),
		cmocka_unit_test(test_mux_con_dis_as_src)
	};

	return cmocka_run_group_tests(tests, global_test_setup, NULL);
}
