/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_prochot_vcmp

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <chipset.h>

LOG_MODULE_REGISTER(prochot_vcmp, LOG_LEVEL_INF);

struct prochot_vcmp_config {
	const struct device *vcmp_high_dev;
	const struct device *vcmp_low_dev;
};

static void prochot_vcmp_enable(const struct device *dev, bool state)
{
	struct sensor_value val;
	int ret;

	val.val1 = state;
	val.val2 = 0;
	ret = sensor_attr_set(dev, SENSOR_CHAN_VOLTAGE, SENSOR_ATTR_ALERT,
			      &val);
	if (ret < 0) {
		LOG_ERR("vcmp attr set failed: %d", ret);
		return;
	}
}

static void prochot_vcmp_handler(const struct device *dev, bool high)
{
	const struct prochot_vcmp_config *cfg = dev->config;

	/* Enable the comparator for the opposite level */
	prochot_vcmp_enable(cfg->vcmp_high_dev, !high);
	prochot_vcmp_enable(cfg->vcmp_low_dev, high);

	if (!chipset_in_state(CHIPSET_STATE_ON)) {
		return;
	}

	LOG_INF("PROCHOT state: %d", high);
}

static void prochot_vcmp_high_handler(const struct device *dev,
				      const struct sensor_trigger *trigger)
{
	prochot_vcmp_handler(DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT)), true);
}

static void prochot_vcmp_low_handler(const struct device *dev,
				     const struct sensor_trigger *trigger)
{
	prochot_vcmp_handler(DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT)), false);
}

static const struct sensor_trigger prochot_trig = {
	.type = SENSOR_TRIG_THRESHOLD,
	.chan = SENSOR_CHAN_VOLTAGE,
};

static int prochot_vcmp_init(const struct device *dev)
{
	const struct prochot_vcmp_config *cfg = dev->config;
	int ret;

	ret = sensor_trigger_set(cfg->vcmp_high_dev, &prochot_trig,
				 prochot_vcmp_high_handler);
	if (ret < 0) {
		LOG_ERR("trigger set failed: %d", ret);
	}

	ret = sensor_trigger_set(cfg->vcmp_low_dev, &prochot_trig,
				 prochot_vcmp_low_handler);
	if (ret < 0) {
		LOG_ERR("trigger set failed: %d", ret);
	}

	/* Enable both as a start, one of the two shoudl trigger and the state
	 * settle immediately.
	 */
	prochot_vcmp_enable(cfg->vcmp_high_dev, true);
	prochot_vcmp_enable(cfg->vcmp_low_dev, true);

	LOG_DBG("prochot vcmp ready");

	return 0;
}

static const struct prochot_vcmp_config prochot_vcmp_cfg = {
	.vcmp_high_dev = DEVICE_DT_GET(DT_INST_PHANDLE(0, vcmp_high)),
	.vcmp_low_dev = DEVICE_DT_GET(DT_INST_PHANDLE(0, vcmp_low)),
};

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1);
DEVICE_DT_INST_DEFINE(0, prochot_vcmp_init, NULL, NULL, &prochot_vcmp_cfg,
		      POST_KERNEL, 99, NULL);
