/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fff.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <dt-bindings/kbd.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, chipset_in_state, int);
FAKE_VALUE_FUNC(int, test_sensor_attr_set, const struct device *,
		enum sensor_channel, enum sensor_attribute,
		const struct sensor_value *);
FAKE_VALUE_FUNC(int, test_sensor_trigger_set, const struct device *,
		const struct sensor_trigger *, sensor_trigger_handler_t);

static const struct sensor_driver_api test_sensor_api = {
	.attr_set = test_sensor_attr_set,
	.trigger_set = test_sensor_trigger_set,
};

DEVICE_DT_DEFINE(DT_INST(0, test_sensor), NULL, NULL, NULL, NULL, PRE_KERNEL_1,
		 CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &test_sensor_api);

ZTEST(prochot_vcmp, test_prochot_vcmp)
{
}

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
}

ZTEST_SUITE(prochot_vcmp, NULL, NULL, reset, reset, NULL);
