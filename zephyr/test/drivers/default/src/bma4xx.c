/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "common.h"
#include "driver/accel_bma4xx.h"
#include "emul/emul_bma4xx.h"
#include "emul/emul_common_i2c.h"
#include "i2c.h"
#include "motion_sense.h"
#include "test/drivers/test_state.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define EMUL EMUL_DT_GET(DT_NODELABEL(bma4xx_emul))
#define SENSOR emul_bma4xx_get_sensor_data(EMUL)

ZTEST_SUITE(bma4xx, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST_USER(bma4xx, test_something)
{
	zassert_ok(bma4_accel_drv.get_data_rate(SENSOR));
}