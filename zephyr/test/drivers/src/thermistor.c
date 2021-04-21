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
#include "../driver/temp_sensor/thermistor.h"


#define GPIO_PG_EC_DSW_PWROK_PATH DT_PATH(named_gpios, pg_ec_dsw_pwrok)
#define GPIO_PG_EC_DSW_PWROK_PORT DT_GPIO_PIN(GPIO_PG_EC_DSW_PWROK_PATH, gpios)

#define ADC_DEVICE_NAME		DT_LABEL(DT_NODELABEL(adc0))

#define TEMP_3V3_13K7_47K_4050B_INST	DT_INST(0, temp_3v3_13k7_47k_4050b)
#define ADC_CHANNEL_3V3_13K7_47K_4050B \
		DT_PROP(DT_PHANDLE(TEMP_3V3_13K7_47K_4050B_INST, adc), channel)

#define TEMP_3V3_30K9_47K_4050B_INST	DT_INST(0, temp_3v3_30k9_47k_4050b)
#define ADC_CHANNEL_3V3_30K9_47K_4050B \
		DT_PROP(DT_PHANDLE(TEMP_3V3_30K9_47K_4050B_INST, adc), channel)

#define TEMP_3V3_51K1_47K_4050B_INST	DT_INST(0, temp_3v3_51k1_47k_4050b)
#define ADC_CHANNEL_3V3_51K1_47K_4050B \
		DT_PROP(DT_PHANDLE(TEMP_3V3_51K1_47K_4050B_INST, adc), channel)

#define TEMP_3V0_22K6_47K_4050B_INST	DT_INST(0, temp_3v0_22k6_47k_4050b)
#define ADC_CHANNEL_3V0_22K6_47K_4050B \
		DT_PROP(DT_PHANDLE(TEMP_3V0_22K6_47K_4050B_INST, adc), channel)

/* Conversion of temperature doesn't need to be 100% accurate */
#define TEMP_EPS	2

static void test_thermistor_power_pin(void)
{
	const struct device *gpio_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_PG_EC_DSW_PWROK_PATH, gpios));
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	int temp;

	zassert_not_null(gpio_dev, "Cannot get GPIO device");
	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Make sure that ADC return any valid value */
	zassert_ok(adc_emul_const_value_set(adc_dev,
					    ADC_CHANNEL_3V3_13K7_47K_4050B,
					    1000),
		   "adc_emul_const_value_set() failed");
	zassert_ok(adc_emul_const_value_set(adc_dev,
					    ADC_CHANNEL_3V3_30K9_47K_4050B,
					    1000),
		   "adc_emul_const_value_set() failed");
	zassert_ok(adc_emul_const_value_set(adc_dev,
					    ADC_CHANNEL_3V3_51K1_47K_4050B,
					    1000),
		   "adc_emul_const_value_set() failed");
	zassert_ok(adc_emul_const_value_set(adc_dev,
					    ADC_CHANNEL_3V0_22K6_47K_4050B,
					    1000),
		   "adc_emul_const_value_set() failed");

	/* pg_ec_dsw_pwrok = 0 means ADC is not powered. */
	zassert_ok(gpio_emul_input_set(gpio_dev, GPIO_PG_EC_DSW_PWROK_PORT, 0),
		   NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      get_temp_3v3_13k7_47k_4050b(
					ADC_CHANNEL_3V3_13K7_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      get_temp_3v3_30k9_47k_4050b(
					ADC_CHANNEL_3V3_30K9_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      get_temp_3v3_51k1_47k_4050b(
					ADC_CHANNEL_3V3_51K1_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      get_temp_3v0_22k6_47k_4050b(
					ADC_CHANNEL_3V0_22K6_47K_4050B, &temp),
		      NULL);

	/* pg_ec_dsw_pwrok = 1 means ADC is powered. */
	zassert_ok(gpio_emul_input_set(gpio_dev, GPIO_PG_EC_DSW_PWROK_PORT, 1),
		   NULL);
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_13k7_47k_4050b(
					ADC_CHANNEL_3V3_13K7_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_30k9_47k_4050b(
					ADC_CHANNEL_3V3_30K9_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_51k1_47k_4050b(
					ADC_CHANNEL_3V3_51K1_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      get_temp_3v0_22k6_47k_4050b(
					ADC_CHANNEL_3V0_22K6_47K_4050B, &temp),
		      NULL);

}

/* Simple ADC emulator custom function which always return error */
static int adc_error_func(const struct device *dev, unsigned int channel,
			  void *param, uint32_t *result)
{
	return -EINVAL;
}

static void test_thermistor_adc_read_error(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	int temp;

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Return error on all ADC channels */
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V3_13K7_47K_4050B,
					   adc_error_func, NULL),
		   "adc_emul_value_func_set() failed");
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V3_30K9_47K_4050B,
					   adc_error_func, NULL),
		   "adc_emul_value_func_set() failed");
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V3_51K1_47K_4050B,
					   adc_error_func, NULL),
		   "adc_emul_value_func_set() failed");
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V0_22K6_47K_4050B,
					   adc_error_func, NULL),
		   "adc_emul_value_func_set() failed");

	zassert_equal(EC_ERROR_UNKNOWN,
		      get_temp_3v3_13k7_47k_4050b(
					ADC_CHANNEL_3V3_13K7_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_ERROR_UNKNOWN,
		      get_temp_3v3_30k9_47k_4050b(
					ADC_CHANNEL_3V3_30K9_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_ERROR_UNKNOWN,
		      get_temp_3v3_51k1_47k_4050b(
					ADC_CHANNEL_3V3_51K1_47K_4050B, &temp),
		      NULL);
	zassert_equal(EC_ERROR_UNKNOWN,
		      get_temp_3v0_22k6_47k_4050b(
					ADC_CHANNEL_3V0_22K6_47K_4050B, &temp),
		      NULL);
}

/*
 * Calculate thermistor resistance in temperature t2 when B and resistance in
 * temperature t1 is given. Use formula r2 = r1 / exp(B * (1 / t1 - 1 / t2)
 */
static int resistance(int b, int r1, int t1, int t2)
{
	double B = (double)b;
	double R1 = (double)r1;
	double T1 = (double)t1;
	double T2 = (double)t2;

	return R1 / exp(B * (1 / T1 - 1 / T2));
}

/*
 * Calculate output voltage in voltage divider circuit using formula
 * Vout = Vs * r2 / (r1 + r2)
 */
static int volt_divider(int vs, int r1, int r2)
{
	return vs * r2 / (r1 + r2);
}

struct thermistor_state {
	const int b;
	const int v;
	const int rt; /* thermistor resistance at temperature 't' */
	const int t;
	const int r;
	int temp_expected;
};

/* ADC emulator function which calculate output voltage for given thermistor */
static int adc_temperature_func(const struct device *dev, unsigned int channel,
				void *param, uint32_t *result)
{
	struct thermistor_state *s = (struct thermistor_state *)param;

	*result = volt_divider(s->v, s->r, resistance(s->b, s->rt, s->t,
						      s->temp_expected));

	return 0;
}

static void test_thermistor_3v3_13k7_47k_4050b(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	struct thermistor_state state = {
		.b = 4050,
		.v = 3300,
		.rt = 47000,
		.t = 25 + 273,
		.r = 13700,
	};
	int temp;

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Setup ADC channel */
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V3_13K7_47K_4050B,
					   adc_temperature_func, &state),
		   "adc_emul_value_func_set() failed");

	/* Makes sure that reference voltage is correct for given thermistor */
	zassert_ok(adc_emul_ref_voltage_set(adc_dev, ADC_REF_INTERNAL, state.v),
		   "adc_emul_ref_voltage_set() failed");

#define TEST_THERMISTOR(t)						\
	state.temp_expected = t + 273;					\
	zassert_equal(EC_SUCCESS,					\
		      get_temp_3v3_13k7_47k_4050b(			\
				ADC_CHANNEL_3V3_13K7_47K_4050B, &temp),	\
		      NULL);						\
	zassert_within(state.temp_expected, temp, TEMP_EPS,		\
		       "Expected %d*K, got %d*K", state.temp_expected, temp)

	TEST_THERMISTOR(0);
	TEST_THERMISTOR(1);
	TEST_THERMISTOR(2);
	TEST_THERMISTOR(3);
	TEST_THERMISTOR(4);
	TEST_THERMISTOR(5);
	TEST_THERMISTOR(10);
	TEST_THERMISTOR(23);
	TEST_THERMISTOR(27);
	TEST_THERMISTOR(40);
	TEST_THERMISTOR(51);
	TEST_THERMISTOR(54);
	TEST_THERMISTOR(58);
	TEST_THERMISTOR(59);
	TEST_THERMISTOR(60);
	TEST_THERMISTOR(68);
	TEST_THERMISTOR(72);
	TEST_THERMISTOR(77);
	TEST_THERMISTOR(80);
	TEST_THERMISTOR(93);
	TEST_THERMISTOR(97);
	TEST_THERMISTOR(98);
	TEST_THERMISTOR(99);
	TEST_THERMISTOR(100);
#undef TEST_THERMISTOR

	/* Temperatures below 0*C should be reported as 0*C */
	state.temp_expected = -15 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_13k7_47k_4050b(
				ADC_CHANNEL_3V3_13K7_47K_4050B, &temp),
		      NULL);
	zassert_equal(273, temp, "Expected %d*K, got %d*K", 273, temp);

	/* Temperatures above 100*C should be reported as 100*C */
	state.temp_expected = 115 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_13k7_47k_4050b(
				ADC_CHANNEL_3V3_13K7_47K_4050B, &temp),
		      NULL);
	zassert_equal(373, temp, "Expected %d*K, got %d*K", 373, temp);
}

static void test_thermistor_3v3_30k9_47k_4050b(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	struct thermistor_state state = {
		.b = 4050,
		.v = 3300,
		.rt = 47000,
		.t = 25 + 273,
		.r = 30900,
	};
	int temp;

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Setup ADC channel */
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V3_30K9_47K_4050B,
					   adc_temperature_func, &state),
		   "adc_emul_value_func_set() failed");

	/* Makes sure that reference voltage is correct for given thermistor */
	zassert_ok(adc_emul_ref_voltage_set(adc_dev, ADC_REF_INTERNAL, state.v),
		   "adc_emul_ref_voltage_set() failed");

#define TEST_THERMISTOR(t)						\
	state.temp_expected = t + 273;					\
	zassert_equal(EC_SUCCESS,					\
		      get_temp_3v3_30k9_47k_4050b(			\
				ADC_CHANNEL_3V3_30K9_47K_4050B, &temp),	\
		      NULL);						\
	zassert_within(state.temp_expected, temp, TEMP_EPS,		\
		       "Expected %d*K, got %d*K", state.temp_expected, temp)

	TEST_THERMISTOR(0);
	TEST_THERMISTOR(1);
	TEST_THERMISTOR(2);
	TEST_THERMISTOR(3);
	TEST_THERMISTOR(4);
	TEST_THERMISTOR(5);
	TEST_THERMISTOR(10);
	TEST_THERMISTOR(23);
	TEST_THERMISTOR(27);
	TEST_THERMISTOR(40);
	TEST_THERMISTOR(51);
	TEST_THERMISTOR(54);
	TEST_THERMISTOR(58);
	TEST_THERMISTOR(59);
	TEST_THERMISTOR(60);
	TEST_THERMISTOR(68);
	TEST_THERMISTOR(72);
	TEST_THERMISTOR(77);
	TEST_THERMISTOR(80);
	TEST_THERMISTOR(93);
	TEST_THERMISTOR(97);
	TEST_THERMISTOR(98);
	TEST_THERMISTOR(99);
	TEST_THERMISTOR(100);
#undef TEST_THERMISTOR

	/* Temperatures below 0*C should be reported as 0*C */
	state.temp_expected = -15 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_30k9_47k_4050b(
				ADC_CHANNEL_3V3_30K9_47K_4050B, &temp),
		      NULL);
	zassert_equal(273, temp, "Expected %d*K, got %d*K", 273, temp);

	/* Temperatures above 100*C should be reported as 100*C */
	state.temp_expected = 115 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_30k9_47k_4050b(
				ADC_CHANNEL_3V3_30K9_47K_4050B, &temp),
		      NULL);
	zassert_equal(373, temp, "Expected %d*K, got %d*K", 373, temp);
}

static void test_thermistor_3v3_51k1_47k_4050b(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	struct thermistor_state state = {
		.b = 4050,
		.v = 3300,
		.rt = 47000,
		.t = 25 + 273,
		.r = 51100,
	};
	int temp;

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Setup ADC channel */
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V3_51K1_47K_4050B,
					   adc_temperature_func, &state),
		   "adc_emul_value_func_set() failed");

	/* Makes sure that reference voltage is correct for given thermistor */
	zassert_ok(adc_emul_ref_voltage_set(adc_dev, ADC_REF_INTERNAL, state.v),
		   "adc_emul_ref_voltage_set() failed");

#define TEST_THERMISTOR(t)						\
	state.temp_expected = t + 273;					\
	zassert_equal(EC_SUCCESS,					\
		      get_temp_3v3_51k1_47k_4050b(			\
				ADC_CHANNEL_3V3_51K1_47K_4050B, &temp),	\
		      NULL);						\
	zassert_within(state.temp_expected, temp, TEMP_EPS,		\
		       "Expected %d*K, got %d*K", state.temp_expected, temp)

	TEST_THERMISTOR(0);
	TEST_THERMISTOR(1);
	TEST_THERMISTOR(2);
	TEST_THERMISTOR(3);
	TEST_THERMISTOR(4);
	TEST_THERMISTOR(5);
	TEST_THERMISTOR(10);
	TEST_THERMISTOR(23);
	TEST_THERMISTOR(27);
	TEST_THERMISTOR(40);
	TEST_THERMISTOR(51);
	TEST_THERMISTOR(54);
	TEST_THERMISTOR(58);
	TEST_THERMISTOR(59);
	TEST_THERMISTOR(60);
	TEST_THERMISTOR(68);
	TEST_THERMISTOR(72);
	TEST_THERMISTOR(77);
	TEST_THERMISTOR(80);
	TEST_THERMISTOR(93);
	TEST_THERMISTOR(97);
	TEST_THERMISTOR(98);
	TEST_THERMISTOR(99);
	TEST_THERMISTOR(100);
#undef TEST_THERMISTOR

	/* Temperatures below 0*C should be reported as 0*C */
	state.temp_expected = -15 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_51k1_47k_4050b(
				ADC_CHANNEL_3V3_51K1_47K_4050B, &temp),
		      NULL);
	zassert_equal(273, temp, "Expected %d*K, got %d*K", 273, temp);

	/* Temperatures above 100*C should be reported as 100*C */
	state.temp_expected = 115 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v3_51k1_47k_4050b(
				ADC_CHANNEL_3V3_51K1_47K_4050B, &temp),
		      NULL);
	zassert_equal(373, temp, "Expected %d*K, got %d*K", 373, temp);
}

static void test_thermistor_3v0_22k6_47k_4050b(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);
	struct thermistor_state state = {
		.b = 4050,
		.v = 3000,
		.rt = 47000,
		.t = 25 + 273,
		.r = 22600,
	};
	int temp;

	zassert_not_null(adc_dev, "Cannot get ADC device");

	/* Setup ADC channel */
	zassert_ok(adc_emul_value_func_set(adc_dev,
					   ADC_CHANNEL_3V0_22K6_47K_4050B,
					   adc_temperature_func, &state),
		   "adc_emul_value_func_set() failed");

	/* Makes sure that reference voltage is correct for given thermistor */
	zassert_ok(adc_emul_ref_voltage_set(adc_dev, ADC_REF_INTERNAL, state.v),
		   "adc_emul_ref_voltage_set() failed");

#define TEST_THERMISTOR(t)						\
	state.temp_expected = t + 273;					\
	zassert_equal(EC_SUCCESS,					\
		      get_temp_3v0_22k6_47k_4050b(			\
				ADC_CHANNEL_3V0_22K6_47K_4050B, &temp),	\
		      NULL);						\
	zassert_within(state.temp_expected, temp, TEMP_EPS,		\
		       "Expected %d*K, got %d*K", state.temp_expected, temp)

	TEST_THERMISTOR(0);
	TEST_THERMISTOR(1);
	TEST_THERMISTOR(2);
	TEST_THERMISTOR(3);
	TEST_THERMISTOR(4);
	TEST_THERMISTOR(5);
	TEST_THERMISTOR(10);
	TEST_THERMISTOR(23);
	TEST_THERMISTOR(27);
	TEST_THERMISTOR(40);
	TEST_THERMISTOR(51);
	TEST_THERMISTOR(54);
	TEST_THERMISTOR(58);
	TEST_THERMISTOR(59);
	TEST_THERMISTOR(60);
	TEST_THERMISTOR(68);
	TEST_THERMISTOR(72);
	TEST_THERMISTOR(77);
	TEST_THERMISTOR(80);
	TEST_THERMISTOR(93);
	TEST_THERMISTOR(97);
	TEST_THERMISTOR(98);
	TEST_THERMISTOR(99);
	TEST_THERMISTOR(100);
#undef TEST_THERMISTOR

	/* Temperatures below 0*C should be reported as 0*C */
	state.temp_expected = -15 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v0_22k6_47k_4050b(
				ADC_CHANNEL_3V0_22K6_47K_4050B, &temp),
		      NULL);
	zassert_equal(273, temp, "Expected %d*K, got %d*K", 273, temp);

	/* Temperatures above 100*C should be reported as 100*C */
	state.temp_expected = 115 + 273;
	zassert_equal(EC_SUCCESS,
		      get_temp_3v0_22k6_47k_4050b(
				ADC_CHANNEL_3V0_22K6_47K_4050B, &temp),
		      NULL);
	zassert_equal(373, temp, "Expected %d*K, got %d*K", 373, temp);
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
			 ztest_user_unit_test(
					test_thermistor_3v3_13k7_47k_4050b),
			 ztest_user_unit_test(
					test_thermistor_3v3_30k9_47k_4050b),
			 ztest_user_unit_test(
					test_thermistor_3v3_51k1_47k_4050b),
			 ztest_user_unit_test(
					test_thermistor_3v0_22k6_47k_4050b));
	ztest_run_test_suite(thermistor);
}
