/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test lid switch.
 */

#include "battery_smart.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "task.h"
#include "test_util.h"
#include "util.h"

#define WAIT_CHARGER_TASK 600
#define BATTERY_DETACH_DELAY 35000

static int mock_chipset_state = CHIPSET_STATE_ON;
static int is_shutdown;
static int is_force_discharge;
static int is_hibernated;

/* The simulation doesn't really hibernate, so we reset this ourselves */
extern timestamp_t shutdown_warning_time;

static void reset_mocks(void)
{
	mock_chipset_state = CHIPSET_STATE_ON;
	is_shutdown = is_force_discharge = is_hibernated = 0;
	shutdown_warning_time.val = 0ULL;
}

void chipset_force_shutdown(void)
{
	is_shutdown = 1;
	mock_chipset_state = CHIPSET_STATE_HARD_OFF;
}

int chipset_in_state(int state_mask)
{
	return state_mask & mock_chipset_state;
}

int board_discharge_on_ac(int enabled)
{
	is_force_discharge = enabled;
	return EC_SUCCESS;
}

void system_hibernate(int sec, int usec)
{
	is_hibernated = 1;
}

static int wait_charging_state(void)
{
	enum charge_state state;
	task_wake(TASK_ID_CHARGER);
	msleep(WAIT_CHARGER_TASK);
	state = charge_get_state();
	ccprintf("[CHARGING TEST] state = %d\n", state);
	return state;
}

/* Setup init condition */
static void test_setup(int on_ac)
{
	const struct battery_info *bat_info = battery_get_info();

	reset_mocks();

	/* 50% of charge */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 50);
	sb_write(SB_ABSOLUTE_STATE_OF_CHARGE, 50);
	sb_write(SB_FULL_CHARGE_CAPACITY, 0xf000);
	/* 25 degree Celsius */
	sb_write(SB_TEMPERATURE, CELSIUS_TO_DECI_KELVIN(25));
	/* Normal voltage */
	sb_write(SB_VOLTAGE, bat_info->voltage_normal);
	sb_write(SB_CHARGING_VOLTAGE, bat_info->voltage_max);
	sb_write(SB_CHARGING_CURRENT, 4000);

	if (on_ac) {
		sb_write(SB_CURRENT, 1000);
		gpio_set_level(GPIO_AC_PRESENT, 1);
	} else {
		sb_write(SB_CURRENT, -100);
		gpio_set_level(GPIO_AC_PRESENT, 0);
	}

	/* Let it tick once */
	wait_charging_state();
}

static int charge_control(enum ec_charge_control_mode mode)
{
	struct ec_params_charge_control params;
	params.mode = mode;
	return test_send_host_command(EC_CMD_CHARGE_CONTROL, 1, &params,
				      sizeof(params), NULL, 0);
}

/* Host Event helpers */
static int ev_is_set(int event)
{
	return host_get_events() & EC_HOST_EVENT_MASK(event);
}
static int ev_is_clear(int event)
{
	return !ev_is_set(event);
}
static void ev_clear(int event)
{
	host_clear_events(EC_HOST_EVENT_MASK(event));
}

static int test_charge_state(void)
{
	enum charge_state state;
	uint32_t flags;

	/* On AC */
	test_setup(1);

	ccprintf("[CHARGING TEST] AC on\n");

	/* Detach battery, charging error */
	ccprintf("[CHARGING TEST] Detach battery\n");
	TEST_ASSERT(test_detach_i2c(I2C_PORT_BATTERY, BATTERY_ADDR) ==
		    EC_SUCCESS);
	msleep(BATTERY_DETACH_DELAY);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_ERROR);

	/* Attach battery again, charging */
	ccprintf("[CHARGING TEST] Attach battery\n");
	test_attach_i2c(I2C_PORT_BATTERY, BATTERY_ADDR);
	/* And changing full capacity should trigger a host event */
	ev_clear(EC_HOST_EVENT_BATTERY);
	sb_write(SB_FULL_CHARGE_CAPACITY, 0xeff0);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY));

	/* Unplug AC, discharging at 1000mAh */
	ccprintf("[CHARGING TEST] AC off\n");
	gpio_set_level(GPIO_AC_PRESENT, 0);
	sb_write(SB_CURRENT, -1000);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(!(flags & CHARGE_FLAG_EXTERNAL_POWER));
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	/* Discharging waaaay overtemp is ignored */
	ccprintf("[CHARGING TEST] AC off, batt temp = 0xffff\n");
	gpio_set_level(GPIO_AC_PRESENT, 0);
	sb_write(SB_CURRENT, -1000);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	sb_write(SB_TEMPERATURE, 0xffff);
	state = wait_charging_state();
	TEST_ASSERT(!is_shutdown);
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	sb_write(SB_TEMPERATURE, CELSIUS_TO_DECI_KELVIN(40));

	/* Discharging overtemp */
	ccprintf("[CHARGING TEST] AC off, batt temp = 90 C\n");
	gpio_set_level(GPIO_AC_PRESENT, 0);
	sb_write(SB_CURRENT, -1000);

	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	sb_write(SB_TEMPERATURE, CELSIUS_TO_DECI_KELVIN(90));
	state = wait_charging_state();
	TEST_ASSERT(is_shutdown);
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	sb_write(SB_TEMPERATURE, CELSIUS_TO_DECI_KELVIN(40));

	/* Force idle */
	ccprintf("[CHARGING TEST] AC on, force idle\n");
	gpio_set_level(GPIO_AC_PRESENT, 1);
	sb_write(SB_CURRENT, 1000);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));
	charge_control(CHARGE_CONTROL_IDLE);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_IDLE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(flags & CHARGE_FLAG_FORCE_IDLE);
	charge_control(CHARGE_CONTROL_NORMAL);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);

	/* Force discharge */
	ccprintf("[CHARGING TEST] AC on, force discharge\n");
	gpio_set_level(GPIO_AC_PRESENT, 1);
	sb_write(SB_CURRENT, 1000);
	charge_control(CHARGE_CONTROL_DISCHARGE);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_IDLE);
	TEST_ASSERT(is_force_discharge);
	charge_control(CHARGE_CONTROL_NORMAL);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	TEST_ASSERT(!is_force_discharge);

	return EC_SUCCESS;
}

static int test_low_battery(void)
{
	test_setup(1);

	ccprintf("[CHARGING TEST] Low battery with AC\n");

	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 2);
	wait_charging_state();
	mock_chipset_state = CHIPSET_STATE_SOFT_OFF;
	hook_notify(HOOK_CHIPSET_SHUTDOWN);
	TEST_ASSERT(!is_hibernated);

	ccprintf("[CHARGING TEST] Low battery shutdown S0->S5\n");
	mock_chipset_state = CHIPSET_STATE_ON;
	hook_notify(HOOK_CHIPSET_PRE_INIT);
	hook_notify(HOOK_CHIPSET_STARTUP);
	gpio_set_level(GPIO_AC_PRESENT, 0);
	is_hibernated = 0;
	sb_write(SB_CURRENT, -1000);
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 2);
	wait_charging_state();
	mock_chipset_state = CHIPSET_STATE_SOFT_OFF;
	hook_notify(HOOK_CHIPSET_SHUTDOWN);
	wait_charging_state();
	TEST_ASSERT(is_hibernated);

	ccprintf("[CHARGING TEST] Low battery shutdown S5\n");
	is_hibernated = 0;
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 10);
	wait_charging_state();
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 2);
	wait_charging_state();
	TEST_ASSERT(is_hibernated);

	ccprintf("[CHARGING TEST] Low battery AP shutdown\n");
	is_shutdown = 0;
	mock_chipset_state = CHIPSET_STATE_ON;
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 10);
	gpio_set_level(GPIO_AC_PRESENT, 1);
	sb_write(SB_CURRENT, 1000);
	wait_charging_state();
	gpio_set_level(GPIO_AC_PRESENT, 0);
	sb_write(SB_CURRENT, -1000);
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, 2);
	wait_charging_state();
	usleep(32 * SECOND);
	wait_charging_state();
	TEST_ASSERT(is_shutdown);

	return EC_SUCCESS;
}

static int test_external_funcs(void)
{
	int rv, temp;
	uint32_t flags;
	int state;

	/* Connect the AC */
	test_setup(1);

	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	/* Invalid or do-nothing commands first */
	UART_INJECT("chg\n");
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	UART_INJECT("chg blahblah\n");
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	UART_INJECT("chg idle\n");
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	UART_INJECT("chg idle blargh\n");
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	/* Now let's force idle on and off */
	UART_INJECT("chg idle on\n");
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_IDLE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(flags & CHARGE_FLAG_FORCE_IDLE);

	UART_INJECT("chg idle off\n");
	wait_charging_state();
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_CHARGE);
	flags = charge_get_flags();
	TEST_ASSERT(flags & CHARGE_FLAG_EXTERNAL_POWER);
	TEST_ASSERT(!(flags & CHARGE_FLAG_FORCE_IDLE));

	/* and the rest */
	TEST_ASSERT(charge_get_state() == PWR_STATE_CHARGE);
	TEST_ASSERT(!charge_want_shutdown());
	TEST_ASSERT(charge_get_percent() == 50);
	temp = 0;
	rv = charge_temp_sensor_get_val(0, &temp);
	TEST_ASSERT(rv == EC_SUCCESS);
	TEST_ASSERT(K_TO_C(temp) == 25);

	return EC_SUCCESS;
}

#define CHG_OPT1 0x2000
#define CHG_OPT2 0x4000
static int test_hc_charge_state(void)
{
	enum charge_state state;
	int i, rv, tmp;
	struct ec_params_charge_state params;
	struct ec_response_charge_state resp;

	/* Let's connect the AC again. */
	test_setup(1);

	/* Initialize the charger options with some nonzero value */
	TEST_ASSERT(charger_set_option(CHG_OPT1) == EC_SUCCESS);

	/* Get the state */
	memset(&resp, 0, sizeof(resp));
	params.cmd = CHARGE_STATE_CMD_GET_STATE;
	rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
				    &params, sizeof(params),
				    &resp, sizeof(resp));
	TEST_ASSERT(rv == EC_RES_SUCCESS);
	TEST_ASSERT(resp.get_state.ac);
	TEST_ASSERT(resp.get_state.chg_voltage);
	TEST_ASSERT(resp.get_state.chg_current);
	TEST_ASSERT(resp.get_state.chg_input_current);
	TEST_ASSERT(resp.get_state.batt_state_of_charge);

	/* Check all the params */
	for (i = 0; i < CS_NUM_BASE_PARAMS; i++) {

		/* Read it */
		memset(&resp, 0, sizeof(resp));
		params.cmd = CHARGE_STATE_CMD_GET_PARAM;
		params.get_param.param = i;
		rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
					    &params, sizeof(params),
					    &resp, sizeof(resp));
		TEST_ASSERT(rv == EC_RES_SUCCESS);
		TEST_ASSERT(resp.get_param.value);

		/* Bump it up a bit */
		tmp = resp.get_param.value;
		switch (i) {
		case CS_PARAM_CHG_VOLTAGE:
		case CS_PARAM_CHG_CURRENT:
		case CS_PARAM_CHG_INPUT_CURRENT:
			tmp -= 128;		/* Should be valid delta */
			break;
		case CS_PARAM_CHG_STATUS:
			/* This one can't be set */
			break;
		case CS_PARAM_CHG_OPTION:
			tmp = CHG_OPT2;
			break;
		}
		params.cmd = CHARGE_STATE_CMD_SET_PARAM;
		params.set_param.param = i;
		params.set_param.value = tmp;
		rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
					    &params, sizeof(params),
					    &resp, sizeof(resp));
		if (i == CS_PARAM_CHG_STATUS)
			TEST_ASSERT(rv == EC_RES_ACCESS_DENIED);
		else
			TEST_ASSERT(rv == EC_RES_SUCCESS);
		/* Allow the change to take effect */
		state = wait_charging_state();
		TEST_ASSERT(state == PWR_STATE_CHARGE);

		/* Read it back again*/
		memset(&resp, 0, sizeof(resp));
		params.cmd = CHARGE_STATE_CMD_GET_PARAM;
		params.get_param.param = i;
		rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
					    &params, sizeof(params),
					    &resp, sizeof(resp));
		TEST_ASSERT(rv == EC_RES_SUCCESS);
		TEST_ASSERT(resp.get_param.value == tmp);
	}

	/* param out of range */
	params.cmd = CHARGE_STATE_CMD_GET_PARAM;
	params.get_param.param = CS_NUM_BASE_PARAMS;
	rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
				    &params, sizeof(params),
				    &resp, sizeof(resp));
	TEST_ASSERT(rv == EC_RES_INVALID_PARAM);
	params.cmd = CHARGE_STATE_CMD_SET_PARAM;
	params.set_param.param = CS_NUM_BASE_PARAMS;
	params.set_param.value = 0x1000;	/* random value */
	rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
				    &params, sizeof(params),
				    &resp, sizeof(resp));
	TEST_ASSERT(rv == EC_RES_INVALID_PARAM);

	/* command out of range */
	params.cmd = CHARGE_STATE_NUM_CMDS;
	rv = test_send_host_command(EC_CMD_CHARGE_STATE, 0,
				    &params, sizeof(params),
				    &resp, sizeof(resp));
	TEST_ASSERT(rv == EC_RES_INVALID_PARAM);

	return EC_SUCCESS;
}


static int test_low_battery_hostevents(void)
{
	int state;

	test_setup(0);

	ccprintf("[CHARGING TEST] Low battery host events\n");

	/* okay */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, BATTERY_LEVEL_LOW + 1);
	ev_clear(EC_HOST_EVENT_BATTERY_LOW);
	ev_clear(EC_HOST_EVENT_BATTERY_CRITICAL);
	ev_clear(EC_HOST_EVENT_BATTERY_SHUTDOWN);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_LOW));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_CRITICAL));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_SHUTDOWN));

	/* a little bit lower now */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, BATTERY_LEVEL_LOW - 1);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_LOW));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_CRITICAL));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_SHUTDOWN));
	TEST_ASSERT(!is_shutdown);

	/* a little bit lower now */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, BATTERY_LEVEL_CRITICAL + 1);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_LOW));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_CRITICAL));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_SHUTDOWN));
	TEST_ASSERT(!is_shutdown);

	/* a little bit lower now */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, BATTERY_LEVEL_CRITICAL - 1);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_LOW));
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_CRITICAL));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_SHUTDOWN));
	TEST_ASSERT(!is_shutdown);

	/* a little bit lower now */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, BATTERY_LEVEL_SHUTDOWN + 1);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_LOW));
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_CRITICAL));
	TEST_ASSERT(ev_is_clear(EC_HOST_EVENT_BATTERY_SHUTDOWN));
	TEST_ASSERT(!is_shutdown);

	/* a little bit lower now */
	sb_write(SB_RELATIVE_STATE_OF_CHARGE, BATTERY_LEVEL_SHUTDOWN - 1);
	state = wait_charging_state();
	TEST_ASSERT(state == PWR_STATE_DISCHARGE);
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_LOW));
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_CRITICAL));
	/* hey-hey-HEY-hey. Doesn't immediately shut down */
	TEST_ASSERT(ev_is_set(EC_HOST_EVENT_BATTERY_SHUTDOWN));
	TEST_ASSERT(!is_shutdown);
	/* after a while, the AP should shut down */
	usleep(LOW_BATTERY_SHUTDOWN_TIMEOUT_US);
	TEST_ASSERT(is_shutdown);

	return EC_SUCCESS;
}



void run_test(void)
{
	RUN_TEST(test_charge_state);
	RUN_TEST(test_low_battery);
	RUN_TEST(test_external_funcs);
	RUN_TEST(test_hc_charge_state);
	RUN_TEST(test_low_battery_hostevents);

	test_print_result();
}
