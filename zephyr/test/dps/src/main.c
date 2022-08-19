/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest_assert.h>
#include <zephyr/ztest_test_new.h>

#include "battery.h"
#include "dps.h"
#include "usb_pd.h"

struct batt_params mock_batt = {
	.current = 3400,
	.voltage = 10000,
	.desired_voltage = 13045,
	.desired_current = 3210,
};

uint8_t mock_port_count = 2;
int mock_active_charge_port;
int mock_input_current = 1333;
int mock_vbus_voltage = 8988;
int mock_charger_voltage = 12222;
uint32_t mock_requested_mv = 9000;
uint32_t mock_requested_ma = 3000;

uint8_t board_get_usb_pd_port_count(void)
{
	return mock_port_count;
}

const struct batt_params *charger_current_battery_params(void)
{
	return &mock_batt;
}

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

const uint32_t mock_pd_src_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int mock_pd_src_pdo_cnt = ARRAY_SIZE(mock_pd_src_pdo);

const uint32_t *const pd_get_src_caps(int port)
{
	return mock_pd_src_pdo;
}

uint8_t pd_get_src_cap_cnt(int port)
{
	return mock_pd_src_pdo_cnt;
}

int charge_manager_get_active_charge_port(void)
{
	return mock_active_charge_port;
}

int charge_get_active_chg_chip(void)
{
	return 0;
}

enum ec_error_list charger_get_input_current(int chgnum, int *input_current)
{
	*input_current = mock_input_current;
	return EC_SUCCESS;
}

int charge_manager_get_vbus_voltage(int port)
{
	return mock_vbus_voltage;
}

enum ec_error_list charger_get_voltage(int chgnum, int *voltage)
{
	return mock_charger_voltage;
}

int battery_design_voltage(int *voltage)
{
	*voltage = mock_batt.desired_voltage;
	return EC_SUCCESS;
}

uint32_t pd_get_requested_voltage(int port)
{
	return mock_requested_mv;
}
uint32_t pd_get_requested_current(int port)
{
	return mock_requested_ma;
}

ZTEST_SUITE(dps, NULL, NULL, NULL, NULL, NULL);

ZTEST(dps, test_enable)
{
	zassert_true(dps_is_enabled(), NULL);
	dps_enable(false);
	zassert_false(dps_is_enabled(), NULL);
	dps_enable(true);
	zassert_true(dps_is_enabled(), NULL);
}

ZTEST(dps, test_config)
{
	struct dps_config_t *config = dps_get_config();
	const struct dps_config_t old_config = *config;

	zassert_true(config->k_less_pwr <= config->k_more_pwr, NULL);
	zassert_true(config->k_less_pwr > 0 && config->k_less_pwr < 100, NULL);
	zassert_true(config->k_more_pwr > 0 && config->k_more_pwr < 100, NULL);

	zassert_equal(dps_init(), EC_SUCCESS, NULL);

	config->k_less_pwr = config->k_more_pwr + 1;
	zassert_equal(dps_init(), EC_ERROR_INVALID_CONFIG, NULL);

	/* restore config */
	*config = old_config;
}
