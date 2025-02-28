/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_prochot_vcmp

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

/* Include the right definition for the custom sensor threshold attributes, the
 * enum name happen to be the same for both it8xxx2 and npcx so we just have to
 * include the right file.
 */
#if defined(CONFIG_VCMP_IT8XXX2)
#include <zephyr/drivers/sensor/it8xxx2_vcmp.h>
#elif defined(CONFIG_ADC_CMP_NPCX)
#include <zephyr/drivers/sensor/adc_cmp_npcx.h>
#elif defined(CONFIG_TEST)
#include <test_vcmp_sensor.h>
#else
#error Unsupported platform
#endif

#include <chipset.h>

LOG_MODULE_REGISTER(prochot_vcmp, LOG_LEVEL_INF);

#if (DT_INST_CHILD_NUM_STATUS_OKAY(0) == 1)
#define PROCHOT_NODE DT_INST_CHILD(0, prochot)
#define HAS_PROCHOT_CHILD

BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(PROCHOT_NODE),
	     "The 'cros-ec,prochot-vcmp' child-node must be called 'prochot'");
#endif

#define TH_HIGH_PERCENT 80
#define TH_LOW_PERCENT 50

#define PINCTRL_STATE_GPIO PINCTRL_STATE_PRIV_START

struct prochot_vcmp_config {
	const struct device *vcmp_dev;
	uint16_t high_level_mv;

#ifdef HAS_PROCHOT_CHILD
	const struct gpio_dt_spec prochot_gpio;
	const struct pinctrl_dev_config *prochot_pcfg;
#endif
};

struct prochot_vcmp_data {
	bool last_state;
	bool prochot_is_gpio;
};

static void prochot_vcmp_configure(const struct device *dev, bool state)
{
	const struct prochot_vcmp_config *cfg = dev->config;
	struct sensor_value val;
	enum sensor_attribute attr;
	int ret;

	memset(&val, 0, sizeof(val));

	val.val1 = 0;
	ret = sensor_attr_set(cfg->vcmp_dev, SENSOR_CHAN_VOLTAGE,
			      SENSOR_ATTR_ALERT, &val);
	if (ret < 0) {
		LOG_ERR("vcmp attr set failed: %d", ret);
		return;
	}

	if (state) {
		val.val1 = cfg->high_level_mv * TH_HIGH_PERCENT / 100;
		attr = (enum sensor_attribute)SENSOR_ATTR_UPPER_VOLTAGE_THRESH;
	} else {
		val.val1 = cfg->high_level_mv * TH_LOW_PERCENT / 100;
		attr = (enum sensor_attribute)SENSOR_ATTR_LOWER_VOLTAGE_THRESH;
	}

	ret = sensor_attr_set(cfg->vcmp_dev, SENSOR_CHAN_VOLTAGE, attr, &val);
	if (ret < 0) {
		LOG_ERR("vcmp attr set failed: %d", ret);
		return;
	}

	val.val1 = 1;
	ret = sensor_attr_set(cfg->vcmp_dev, SENSOR_CHAN_VOLTAGE,
			      SENSOR_ATTR_ALERT, &val);
	if (ret < 0) {
		LOG_ERR("vcmp attr set failed: %d", ret);
		return;
	}
}

static void prochot_vcmp_handler(const struct device *sensor_dev,
				 const struct sensor_trigger *trigger)
{
	const struct device *dev = DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT));
	struct prochot_vcmp_data *data = dev->data;

	data->last_state = !data->last_state;

	prochot_vcmp_configure(dev, data->last_state);

	if (!chipset_in_state(CHIPSET_STATE_ON)) {
		return;
	}

	if (data->last_state) {
		LOG_INF("PROCHOT state: asserted");
	} else {
		LOG_INF("PROCHOT state: deasserted");
	}
}

#ifdef HAS_PROCHOT_CHILD
void chipset_throttle_cpu(int throttle)
{
	const struct device *dev = DEVICE_DT_GET(DT_DRV_INST(0));
	const struct prochot_vcmp_config *cfg = dev->config;
	struct prochot_vcmp_data *data = dev->data;
	int ret;

	if (!chipset_in_state(CHIPSET_STATE_ON)) {
		return;
	}

	LOG_INF("PROCHOT: set CPU throttle %d", throttle);

	if (throttle) {
		if (!data->prochot_is_gpio) {
			ret = pinctrl_apply_state(cfg->prochot_pcfg,
						  PINCTRL_STATE_GPIO);
			if (ret < 0) {
				LOG_ERR("PROCHOT: failed to configure pin as GPIO");
			}
			data->prochot_is_gpio = true;
		}

		ret = gpio_pin_configure_dt(&cfg->prochot_gpio,
					    GPIO_OUTPUT_ACTIVE);
		if (ret < 0) {
			LOG_ERR("PROCHOT: failed to assert GPIO");
		}

		return;
	}

	if (data->prochot_is_gpio) {
		ret = gpio_pin_configure_dt(&cfg->prochot_gpio, GPIO_INPUT);
		if (ret < 0) {
			LOG_ERR("PROCHOT: failed to deassert GPIO");
		}

		ret = pinctrl_apply_state(cfg->prochot_pcfg,
					  PINCTRL_STATE_DEFAULT);
		if (ret < 0) {
			LOG_ERR("PROCHOT: failed to configure pin as ADC");
		}
		data->prochot_is_gpio = false;
	}
}
#endif

static const struct sensor_trigger prochot_trig = {
	.type = SENSOR_TRIG_THRESHOLD,
	.chan = SENSOR_CHAN_VOLTAGE,
};

static int prochot_vcmp_init(const struct device *dev)
{
	const struct prochot_vcmp_config *cfg = dev->config;
	struct prochot_vcmp_data *data = dev->data;
	int ret;

#ifdef HAS_PROCHOT_CHILD
	/* Ensure PROCHOT output is not asserted by default. */
	ret = pinctrl_apply_state(cfg->prochot_pcfg, PINCTRL_STATE_GPIO);
	if (ret < 0) {
		LOG_ERR("PROCHOT: failed to configure pin as GPIO");
		return ret;
	}
	ret = gpio_pin_configure_dt(&cfg->prochot_gpio, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("PROCHOT: failed to deassert GPIO");
		return ret;
	}
	ret = pinctrl_apply_state(cfg->prochot_pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		LOG_ERR("PROCHOT: failed to configure pin as ADC");
		return ret;
	}
	data->prochot_is_gpio = false;
#endif

	ret = sensor_trigger_set(cfg->vcmp_dev, &prochot_trig,
				 prochot_vcmp_handler);
	if (ret < 0) {
		LOG_ERR("trigger set failed: %d", ret);
		return ret;
	}

	/* Initialize for detecting a high transition */
	data->last_state = true;
	prochot_vcmp_configure(dev, true);

	return 0;
}

#ifdef HAS_PROCHOT_CHILD
PINCTRL_DT_DEFINE(PROCHOT_NODE);
#endif

static const struct prochot_vcmp_config prochot_vcmp_cfg = {
	.vcmp_dev = DEVICE_DT_GET(DT_INST_PHANDLE(0, vcmp)),
	.high_level_mv = DT_INST_PROP(0, high_level_mv),
#ifdef HAS_PROCHOT_CHILD
	.prochot_gpio = GPIO_DT_SPEC_GET(PROCHOT_NODE, gpios),
	.prochot_pcfg = PINCTRL_DT_DEV_CONFIG_GET(PROCHOT_NODE),
#endif
};

static struct prochot_vcmp_data prochot_vcmp_data;

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1);
BUILD_ASSERT(DT_CHILD_NUM_STATUS_OKAY(DT_DRV_INST(0)) <= 1);
DEVICE_DT_INST_DEFINE(0, prochot_vcmp_init, NULL, &prochot_vcmp_data,
		      &prochot_vcmp_cfg, POST_KERNEL,
		      CONFIG_SENSOR_INIT_PRIORITY, NULL);

#if CONFIG_TEST
int test_reinit(void)
{
	return prochot_vcmp_init(DEVICE_DT_GET(DT_INST(0, DT_DRV_COMPAT)));
}
#endif
