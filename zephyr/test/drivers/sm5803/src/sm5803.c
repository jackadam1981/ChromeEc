/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/charger/sm5803.h"
#include "test/drivers/charger_utils.h"
#include "test/drivers/test_state.h"

#include <zephyr/ztest.h>

#define CHARGER_NUM get_charger_num(&sm5803_drv)
#define SM5803_EMUL EMUL_DT_GET(DT_NODELABEL(sm5803_emul))

ZTEST_SUITE(sm5803, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(sm5803, test_sm5803_zero_fast_charge_current)
{
	int ma;

	zassert_ok(sm5803_drv.set_current(CHARGER_NUM, 0));
	zassert_ok(sm5803_drv.get_current(CHARGER_NUM, &ma));
	zassert_equal(ma, 100,
		      "Zero current limit should be converted to nonzero");
}