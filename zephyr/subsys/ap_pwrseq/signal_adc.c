/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/atomic.h>
#include <logging/log.h>

#include <ap_power/ap_power.h>
#include <power_signals.h>
#include <signal_adc.h>
#include "drivers/sensor.h"

#define MY_COMPAT	intel_ap_pwrseq_adc

#if HAS_ADC_SIGNALS

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

struct adc_config {
	const struct device *dev_trig_high;
	const struct device *dev_trig_low;
	enum power_signal signal;
};

#define INIT_ADC_CONFIG(id)	\
{									\
	.dev_trig_high = DEVICE_DT_GET(DT_PHANDLE(id, trigger_high)),	\
	.dev_trig_low = DEVICE_DT_GET(DT_PHANDLE(id, trigger_low)),	\
	.signal = PWR_SIGNAL_ENUM(id),					\
},

static const struct adc_config config[] = {
DT_FOREACH_STATUS_OKAY(MY_COMPAT, INIT_ADC_CONFIG)
};

static atomic_t value[ARRAY_SIZE(config)];

static void trigger_high(enum pwr_sig_adc adc)
{
	struct sensor_value val;

	atomic_set_bit(&value[adc], 0);
	val.val1 = false;
	sensor_attr_set(config[adc].dev_trig_high,
			SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT,
			&val);
	val.val1 = true;
	sensor_attr_set(config[adc].dev_trig_low,
			SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT,
			&val);
	LOG_INF("power signal adc%d is HIGH", adc);
	power_signal_interrupt(config[adc].signal, 1);
}

static void trigger_low(enum pwr_sig_adc adc)
{
	struct sensor_value val;

	atomic_clear_bit(&value[adc], 0);
	val.val1 = false;
	sensor_attr_set(config[adc].dev_trig_low,
			SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT,
			&val);
	val.val1 = true;
	sensor_attr_set(config[adc].dev_trig_high,
			SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT,
			&val);
	LOG_INF("power signal adc%d is LOW", adc);
	power_signal_interrupt(config[adc].signal, 0);
}

int power_signal_adc_get(enum pwr_sig_adc adc)
{
	if (adc < 0 || adc >= ARRAY_SIZE(config)) {
		return -EINVAL;
	}
	return !!value[adc];
}

/*
 * Macros to create individual callbacks for
 * high and low triggers for each ADC.
 */

#define TAG_ADC(tag, name) DT_CAT(tag, name)

#define PWR_ADC_ENUM(id) TAG_ADC(PWR_SIG_TAG_ADC, PWR_SIGNAL_ENUM(id))

#define ADC_CB(id, lev)	cb_##lev##_##id

#define ADC_CB_DEFINE(id, lev)					\
static void ADC_CB(id, lev)(const struct device *dev,		\
		       const struct sensor_trigger *trigger)	\
{								\
	trigger_##lev(PWR_ADC_ENUM(id));			\
}

DT_FOREACH_STATUS_OKAY_VARGS(MY_COMPAT, ADC_CB_DEFINE, high)
DT_FOREACH_STATUS_OKAY_VARGS(MY_COMPAT, ADC_CB_DEFINE, low)

#define ADC_CB_COMMA(id, lev)	ADC_CB(id, lev),

static void power_signal_adc_power_change(struct ap_power_ev_callback *cb,
					  struct ap_power_ev_data data)
{
	const struct device *adc_dev;
	struct sensor_value val;
	int i;

	switch (data.event) {
	case AP_POWER_RESUME:
		val.val1 = true;
		break;

	case AP_POWER_S0IX:
		val.val1 = false;
		break;
	default:
		return;
	}

	for (i = 0; i < ARRAY_SIZE(config); i++) {
		adc_dev = !!value[i] ? config[i].dev_trig_low :
			config[i].dev_trig_high;
		sensor_attr_set(adc_dev, SENSOR_CHAN_VOLTAGE,
				SENSOR_ATTR_ALERT,
				&val);
		LOG_INF("adc%d trig-%s %sable", i, !!value[i] ? "low" : "high",
			val.val1 ? "en" : "dis");
	}
}

void power_signal_adc_init(void)
{
	static struct ap_power_ev_callback cb;
	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_THRESHOLD,
		.chan = SENSOR_CHAN_VOLTAGE
	};
	struct sensor_value val;
	sensor_trigger_handler_t low_cb[] = {
		DT_FOREACH_STATUS_OKAY_VARGS(MY_COMPAT, ADC_CB_COMMA, low)
	};
	sensor_trigger_handler_t high_cb[] = {
		DT_FOREACH_STATUS_OKAY_VARGS(MY_COMPAT, ADC_CB_COMMA, high)
	};
	int i;

	ap_power_ev_init_callback(&cb, power_signal_adc_power_change,
				  AP_POWER_S0IX | AP_POWER_RESUME);
	ap_power_ev_add_callback(&cb);

	for (i = 0; i < ARRAY_SIZE(low_cb); i++) {
		/* Set high and low trigger callbacks */
		sensor_trigger_set(config[i].dev_trig_high, &trig, high_cb[i]);
		sensor_trigger_set(config[i].dev_trig_low, &trig, low_cb[i]);

		/* Enable high trigger callback only */
		val.val1 = true;
		sensor_attr_set(config[i].dev_trig_high,
				SENSOR_CHAN_VOLTAGE,
				SENSOR_ATTR_ALERT,
				&val);
	}
}

#endif /*  HAS_ADC_SIGNALS */
