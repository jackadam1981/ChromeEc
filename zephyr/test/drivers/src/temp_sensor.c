/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/adc.h>
#include <drivers/adc/adc_emul.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include <math.h>

#include "common.h"
#include "temp_sensor.h"
#include "temp_sensor/temp_sensor.h"

#define GPIO_PG_EC_DSW_PWROK_PATH DT_PATH(named_gpios, pg_ec_dsw_pwrok)
#define GPIO_PG_EC_DSW_PWROK_PORT DT_GPIO_PIN(GPIO_PG_EC_DSW_PWROK_PATH, gpios)

#define ADC_DEVICE_NAME		DT_LABEL(DT_NODELABEL(adc0))
#define ADC_CHANNELS_NUM	DT_PROP(DT_NODELABEL(adc0), nchannels)

static void test_temp_sensor_wrong_id(void)
{
	int temp;

	zassert_equal(EC_ERROR_INVAL, temp_sensor_read(TEMP_SENSOR_COUNT,
						       &temp),
		      NULL);
}

static void test_temp_sensor_adc_error(void)
{
	const struct device *gpio_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_PG_EC_DSW_PWROK_PATH, gpios));
	int temp;

	zassert_not_null(gpio_dev, "Cannot get GPIO device");

	/*
	 * pg_ec_dsw_pwrok = 0 means ADC is not powered.
	 * adc_read will return error
	 */
	zassert_ok(gpio_emul_input_set(gpio_dev, GPIO_PG_EC_DSW_PWROK_PORT, 0),
		   NULL);

	zassert_equal(EC_ERROR_NOT_POWERED,
		      temp_sensor_read(TEMP_SENSOR_CHARGER, &temp), NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      temp_sensor_read(TEMP_SENSOR_DDR_SOC, &temp), NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      temp_sensor_read(TEMP_SENSOR_FAN, &temp), NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      temp_sensor_read(TEMP_SENSOR_PP3300_REGULATOR, &temp),
		      NULL);

	/* power ADC */
	zassert_ok(gpio_emul_input_set(gpio_dev, GPIO_PG_EC_DSW_PWROK_PORT, 1),
		   NULL);
}

/* Simple ADC emulator custom function which always return error */
static int adc_error_func(const struct device *dev, unsigned int channel,
			  void *param, uint32_t *result)
{
	return -EINVAL;
}

static void test_temp_sensor_read(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	int temp, chan;

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Return error on all ADC channels */
	for (chan = 0; chan < ADC_CHANNELS_NUM; chan++) {
		zassert_ok(adc_emul_value_func_set(adc_dev, chan,
						    adc_error_func, NULL),
			   "channel %d adc_emul_value_func_set() failed", chan);
	}

#define TEST_SENSOR(sensor) do {					\
		/* ADC channel of tested sensor return valid value */	\
		zassert_ok(adc_emul_const_value_set(adc_dev,		\
				temp_sensors[sensor].idx, 1000),	\
			   "adc_emul_const_value_set() failed");	\
		zassert_equal(EC_SUCCESS,				\
			      temp_sensor_read(sensor, &temp), NULL);	\
		zassert_within(temp, 273 + 50, 51,			\
			"Expected temperature in 0*C-100*C, got %d*C",	\
			temp);						\
		/* Return error on ADC channel of tested sensor */	\
		zassert_ok(adc_emul_value_func_set(adc_dev,		\
					temp_sensors[sensor].idx,	\
					adc_error_func, NULL),		\
			   "adc_emul_value_func_set() failed");		\
	} while (0)

	TEST_SENSOR(TEMP_SENSOR_CHARGER);
	TEST_SENSOR(TEMP_SENSOR_DDR_SOC);
	TEST_SENSOR(TEMP_SENSOR_FAN);
	TEST_SENSOR(TEMP_SENSOR_PP3300_REGULATOR);
#undef TEST_SENSOR
}

void test_suite_temp_sensor(void)
{
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_PG_EC_DSW_PWROK_PATH, gpios));

	zassert_not_null(dev, NULL);
	/* Before tests make sure that power pin is set. */
	zassert_ok(gpio_emul_input_set(dev, GPIO_PG_EC_DSW_PWROK_PORT, 1),
		   NULL);

	ztest_test_suite(temp_sensor,
			 ztest_user_unit_test(test_temp_sensor_wrong_id),
			 ztest_user_unit_test(test_temp_sensor_adc_error),
			 ztest_user_unit_test(test_temp_sensor_read));
	ztest_run_test_suite(temp_sensor);
}
