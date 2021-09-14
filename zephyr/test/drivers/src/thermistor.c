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
#include <temp_sensor.h>

#include "common.h"
#include "../driver/temp_sensor/thermistor.h"
#include "temp_sensor/temp_sensor.h"


#define GPIO_PG_EC_DSW_PWROK_PATH DT_PATH(named_gpios, pg_ec_dsw_pwrok)
#define GPIO_PG_EC_DSW_PWROK_PORT DT_GPIO_PIN(GPIO_PG_EC_DSW_PWROK_PATH, gpios)

#define ADC_DEVICE_NODE		DT_NODELABEL(adc0)

#define _NAMED_TEMP_SENSOR_COUNTER(x) 1 +
#define NAMED_TEMP_SENSORS_SIZE DT_FOREACH_CHILD(DT_PATH(named_temp_sensors), \
	_NAMED_TEMP_SENSOR_COUNTER) 0

/* Conversion of temperature doesn't need to be 100% accurate */
#define TEMP_EPS	2

/**
 * Test if get temp function return expected error when ADC is not powered
 * (indicated as GPIO pin set to low) and return success after powering on ADC.
 */
static void test_thermistor_power_pin(void)
{
	const struct device *gpio_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_PG_EC_DSW_PWROK_PATH, gpios));
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);

	zassert_not_null(gpio_dev, "Cannot get GPIO device");
	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Make sure that ADC return any valid value */
	
#define A_VALID_VOLTAGE 1000
#define ASSERT_VALID_ADC_VALUE(ARG) printf("My label is %s\n", DT_PROP(ARG, label)); \
	zassert_ok(adc_emul_const_value_set(adc_dev,\
					    DT_PROP(DT_PHANDLE(ARG, adc), channel), \
					    A_VALID_VOLTAGE),\
		   "adc_emul_const_value_set() failed");

	DT_FOREACH_CHILD(DT_PATH(named_temp_sensors), ASSERT_VALID_ADC_VALUE)


	/* pg_ec_dsw_pwrok = 0 means ADC is not powered. */
	zassert_ok(gpio_emul_input_set(gpio_dev, GPIO_PG_EC_DSW_PWROK_PORT, 0),
		   NULL);


	int temp;
	int sensor_idx;

	for (sensor_idx = 0; sensor_idx < NAMED_TEMP_SENSORS_SIZE; sensor_idx++) {
		const struct temp_sensor_t *sensor = & temp_sensors[sensor_idx];
		zassert_equal(EC_ERROR_NOT_POWERED, sensor->read(sensor, &temp), NULL);
	}



/* pg_ec_dsw_pwrok = 1 means ADC is powered. */
	zassert_ok(gpio_emul_input_set(gpio_dev, GPIO_PG_EC_DSW_PWROK_PORT, 1),
		   NULL);

	for (sensor_idx = 0; sensor_idx < NAMED_TEMP_SENSORS_SIZE; sensor_idx++) {
		const struct temp_sensor_t *sensor = & temp_sensors[sensor_idx];
		zassert_equal(EC_SUCCESS, sensor->read(sensor, &temp), NULL);
	}
}

// Simple ADC emulator custom function which always return error
static int adc_error_func(const struct device *dev, unsigned int channel,
			  void *param, uint32_t *result)
{
	return -EINVAL;
}

/** Test if get temp function return expected error on ADC malfunction */
static void test_thermistor_adc_read_error(void)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Return error on all ADC channels */
#define ASSERT_READ_ERROR(node_id)				\
	zassert_ok(adc_emul_value_func_set(				\
			   adc_dev,					\
			   DT_PROP(DT_PHANDLE(node_id, adc), channel),	\
			   adc_error_func,				\
			   NULL),					\
		   "adc_emul_value_func_set() failed");

	DT_FOREACH_CHILD(DT_PATH(named_temp_sensors), ASSERT_READ_ERROR)

	int temp;
	int sensor_idx;

	for (sensor_idx = 0; sensor_idx < NAMED_TEMP_SENSORS_SIZE; sensor_idx++) {
		const struct temp_sensor_t *sensor = & temp_sensors[sensor_idx];
		printf("Name of Sensor %s\n", sensor->name);
		printf("sensor read value is %d\n", sensor->read(sensor, &temp));
		printf("ec not powered read value is %d\n", EC_ERROR_NOT_POWERED);
		printf("ec success read value is %d\n", EC_SUCCESS);
		printf("ec error value is %d\n", EC_ERROR_UNKNOWN);
		// TODO why doesnt below work?
		zassert_equal(EC_ERROR_UNKNOWN, sensor->read(sensor, &temp), NULL);
	}
}

/** Get resistance of thermistor for given temperature */
static int resistance(int t)
{
	/* Thermistor manufacturer resistance lookup table*/
	int r_table[] = {
		155700, 147900, 140600, 133700, 127200, /* 0*C  - 4*C */
		121000, 115100, 109600, 104300, 99310,  /* 5*C  - 9*C */
		94600,  90130,  85890,  81870,  78070,  /* 10*C - 14*C */
		74450,  71020,  67770,  64680,  61750,  /* 15*C - 19*C */
		58970,  56320,  53810,  51430,  49160,  /* 20*C - 24*C */
		47000,  44950,  42990,  41130,  39360,  /* 25*C - 29*C */
		37680,  36070,  34540,  33080,  31690,  /* 30*C - 34*C */
		30360,  29100,  27900,  26750,  25650,  /* 35*C - 39*C */
		24610,  23610,  22660,  21750,  20880,  /* 40*C - 44*C */
		20050,  19260,  18500,  17780,  17090,  /* 45*C - 49*C */
		16430,  15800,  15200,  14620,  14070,  /* 50*C - 54*C */
		13540,  13030,  12550,  12090,  11640,  /* 55*C - 59*C */
		11210,  10800,  10410,  10040,  9676,   /* 60*C - 64*C */
		9331,   8999,   8680,   8374,   8081,   /* 65*C - 69*C */
		7799,   7528,   7268,   7018,   6777,   /* 70*C - 74*C */
		6546,   6324,   6111,   5906,   5708,   /* 75*C - 79*C */
		5518,   5335,   5160,   4990,   4827,   /* 80*C - 84*C */
		4671,   4519,   4374,   4233,   4098,   /* 85*C - 89*C */
		3968,   3842,   3721,   3605,   3492,   /* 90*C - 94*C */
		3384,   3279,   3179,   3082,   2988,   /* 95*C - 99*C */
		2898                                    /* 100*C */
	};

	t -= 273;
	if (t < 0)
		return r_table[0] + 10000;

	if (t >= ARRAY_SIZE(r_table))
		return r_table[ARRAY_SIZE(r_table) - 1] - 100;

	return r_table[t];
}

/**
 * Calculate output voltage in voltage divider circuit using formula
 * Vout = Vs * r2 / (r1 + r2)
 */
static int volt_divider(int vs, int r1, int r2)
{
	return vs * r2 / (r1 + r2);
}

struct thermistor_state {
	const int v;
	const int r;
	int temp_expected;
};

/** ADC emulator function which calculate output voltage for given thermistor */
static int adc_temperature_func(const struct device *dev, unsigned int channel,
				void *param, uint32_t *result)
{
	struct thermistor_state *s = (struct thermistor_state *)param;

	*result = volt_divider(s->v, s->r, resistance(s->temp_expected));

	return 0;
}

/** Test conversion from ADC raw value to temperature */
static void do_thermistor_test(const struct temp_sensor_t *temp_sensor,
		    int reference_mv,
		    int reference_ohms)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);
	struct thermistor_state state = {
		.v = reference_mv,
		.r = reference_ohms,
	};
	int temp_expected;
	int temp;

	printf("\n NOW TESTING %s\n", temp_sensor->name);

	zassert_not_null(adc_dev, "Cannot get ADC device");

		/* Setup ADC channel */
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   temp_sensor->idx,
					   adc_temperature_func, &state),
		   "adc_emul_value_func_set() failed");

	/* Makes sure that reference voltage is correct for given thermistor */
	zassert_ok(adc_emul_ref_voltage_set(adc_dev, ADC_REF_INTERNAL, state.v),
		   "adc_emul_ref_voltage_set() failed");

	/* Test whole supported range from 0*C to 100*C (273*K to 373*K) */
	for (temp_expected = 273; temp_expected <= 373; temp_expected++) {
		state.temp_expected = temp_expected;
		zassert_equal(EC_SUCCESS,
			      temp_sensor->read(temp_sensor,
							&temp),
			      NULL);
		zassert_within(temp_expected, temp, TEMP_EPS,
			       "Expected %d*K, got %d*K", temp_expected, temp);
	}

	/* Temperatures below 0*C should be reported as 0*C */
	state.temp_expected = -15 + 273;
	zassert_equal(EC_SUCCESS,
		      temp_sensor->read(temp_sensor,
						&temp),
		      NULL);
	zassert_equal(273, temp, "Expected %d*K, got %d*K", 273, temp);

	/* Temperatures above 100*C should be reported as 100*C */
	state.temp_expected = 115 + 273;
	zassert_equal(EC_SUCCESS,
		      temp_sensor->read(temp_sensor,
						&temp),
		      NULL);
	zassert_equal(373, temp, "Expected %d*K, got %d*K", 373, temp);
}
static void test_thermistor_adc_temperature_conversion(void)
{

#define GET_THERMISTOR_REF_MV(node_id)             \
	[ZSHIM_TEMP_SENSOR_ID(node_id)] = DT_PROP( \
		DT_PHANDLE(node_id, thermistor), steinhart_reference_mv),

#define GET_THERMISTOR_REF_RES(node_id)            \
	[ZSHIM_TEMP_SENSOR_ID(node_id)] = DT_PROP( \
		DT_PHANDLE(node_id, thermistor), steinhart_reference_res),

	const static int reference_mv_arr[] = { DT_FOREACH_STATUS_OKAY(
		temp_sensor, GET_THERMISTOR_REF_MV) };
	const static int reference_res_arr[] = { DT_FOREACH_STATUS_OKAY(
		temp_sensor, GET_THERMISTOR_REF_RES) };

	int sensor_idx;
	for (sensor_idx = 0; sensor_idx < NAMED_TEMP_SENSORS_SIZE; sensor_idx++)
		do_thermistor_test(& temp_sensors[sensor_idx],
				   reference_mv_arr[sensor_idx],
				   reference_res_arr[sensor_idx]);
}

void test_suite_thermistor(void)
{
	const struct device *dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_PG_EC_DSW_PWROK_PATH, gpios));

	zassert_not_null(dev, NULL);
	/* Before tests make sure that power pin is set. */
	zassert_ok(gpio_emul_input_set(dev, GPIO_PG_EC_DSW_PWROK_PORT, 1),
		   NULL);
	
	ztest_test_suite(thermistor,
			 ztest_user_unit_test(test_thermistor_power_pin),
			 ztest_user_unit_test(test_thermistor_adc_read_error),
			 ztest_user_unit_test(test_thermistor_adc_temperature_conversion));

	ztest_run_test_suite(thermistor); 
}
