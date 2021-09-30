/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>
#include <drivers/emul.h>

#include "battery.h"
#include "charger_utils.h"
#include "driver/charger/isl923x_public.h"
#include "emul/emul_isl923x.h"

BUILD_ASSERT(CONFIG_CHARGER_SENSE_RESISTOR == 10 ||
	     CONFIG_CHARGER_SENSE_RESISTOR == 5);

#if CONFIG_CHARGER_SENSE_RESISTOR == 10
#define EXPECTED_CURRENT_MA(n) (n)
#else
#define EXPECTED_CURRENT_MA(n) (n * 2)
#endif

#define CHARGER_NUM get_charger_num(&isl923x_drv)

void test_isl923x_set_current(void)
{
	int expected_current_mA[] = {
		EXPECTED_CURRENT_MA(0),	   EXPECTED_CURRENT_MA(4),
		EXPECTED_CURRENT_MA(8),	   EXPECTED_CURRENT_MA(16),
		EXPECTED_CURRENT_MA(32),   EXPECTED_CURRENT_MA(64),
		EXPECTED_CURRENT_MA(128),  EXPECTED_CURRENT_MA(256),
		EXPECTED_CURRENT_MA(512),  EXPECTED_CURRENT_MA(1024),
		EXPECTED_CURRENT_MA(2048), EXPECTED_CURRENT_MA(4096)
	};
	int current_mA;

	for (int i = 0; i < ARRAY_SIZE(expected_current_mA); ++i) {
		zassert_ok(isl923x_drv.set_current(CHARGER_NUM,
						   expected_current_mA[i]),
			   "Failed to set the current to %dmA",
			   expected_current_mA[i]);
		zassert_ok(isl923x_drv.get_current(CHARGER_NUM, &current_mA),
			   "Failed to get current");
		zassert_equal(expected_current_mA[i], current_mA,
			      "Expected current %dmA but got %dmA",
			      expected_current_mA[i], current_mA);
	}
}

void test_isl923x_set_voltage(void)
{
	int expected_voltage_mV[] = { 0,   8,	 16,   32,   64,   128,	 256,
				      512, 1024, 2048, 4096, 8192, 16384 };
	int voltage_mV;

	for (int i = 0; i < ARRAY_SIZE(expected_voltage_mV); ++i) {
		zassert_ok(isl923x_drv.set_voltage(CHARGER_NUM,
						   expected_voltage_mV[i]),
			   "Failed to set the voltage to %dmV",
			   expected_voltage_mV[i]);
		zassert_ok(isl923x_drv.get_voltage(CHARGER_NUM, &voltage_mV),
			   "Failed to get voltage");
		zassert_equal(i == 0 ? battery_get_info()->voltage_min :
					     expected_voltage_mV[i],
			      voltage_mV, "Expected voltage %dmV but got %dmV",
			      expected_voltage_mV[i], voltage_mV);
	}
}

void test_suite_isl923x(void)
{
	ztest_test_suite(isl923x,
			 ztest_unit_test(test_isl923x_set_current),
			 ztest_unit_test(test_isl923x_set_voltage));
	ztest_run_test_suite(isl923x);
}
