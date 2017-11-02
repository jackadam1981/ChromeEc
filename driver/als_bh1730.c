/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Rohm BH1730 Ambient light sensor driver
 */

#include "driver/als_bh1730.h"
#include "i2c.h"
#include "console.h"
#include "math_util.h"
#include "util.h"

#ifndef LOCAL
#define LOCAL static
#endif

#ifdef CONFIG_CMD_BH1730_TEST
/* Test data from console command */
uint16_t test_data_use = 0;
uint16_t test_data0 = 0;
uint16_t test_data1 = 0;
#endif

/* Sensor configuration */
/* Select Gain */
#define BH1730_CONF_GAIN BH1730_GAIN_GAIN_X1_GAIN
/* Select Itime */
#define BH1730_CONF_ITIME 0xDA

/* Map gain register value to gain fp */
fp_t gain_to_fp[4] = { INT_TO_FP(1), INT_TO_FP(2),
			INT_TO_FP(64), INT_TO_FP(128) };

/* Map itime register value to ms fp */
fp_t itime_to_ms_fp(uint8_t itime)
{
	/* Cycle is defined "256 - itime" * 2.7ms */
	#define CYCLE_TIME FLOAT_TO_FP(2.7)
	return fp_mul(INT_TO_FP(256-itime),CYCLE_TIME);
}

/* Threshold 1 and data0,1 multipliers */
#define LUXTH1 FLOAT_TO_FP(0.26)
#define LUXTH1_D0_M FLOAT_TO_FP(1.290)
#define LUXTH1_D1_M FLOAT_TO_FP(2.733)

/* Threshold 2 and data0,1 multipliers */
#define LUXTH2 FLOAT_TO_FP(0.55)
#define LUXTH2_D0_M FLOAT_TO_FP(0.797)
#define LUXTH2_D1_M FLOAT_TO_FP(0.859)

/* Threshold 3 and data0,1 multipliers */
#define LUXTH3 FLOAT_TO_FP(1.09)
#define LUXTH3_D0_M FLOAT_TO_FP(0.510)
#define LUXTH3_D1_M FLOAT_TO_FP(0.345)

/* Threshold 4 and data0,1 multipliers */
#define LUXTH4 FLOAT_TO_FP(2.13)
#define LUXTH4_D0_M FLOAT_TO_FP(0.276)
#define LUXTH4_D1_M FLOAT_TO_FP(0.130)

/**
 * Convert BH1730 data0, data1 to lux
 */
LOCAL int bh1730_convert_to_lux(int data0, int data1)
{
	int lux;

	if ( data0 == 0 ) {
		lux = 2;
		return lux;
	}

	{
	// NOTE: calculation fails if FPU not used and data 15bit high
	// 15 bit will shift to fp sign bit
	fp_t fp_d0  = INT_TO_FP(data0);
	fp_t fp_d1  = INT_TO_FP(data1);
	fp_t fp_temp = fp_div(fp_d1, fp_d0);
	fp_t fp_lux;

	/* following lux calculation formula split to few steps to avoid fp */
	/* overflow */
	/* "fp_lux = ((mul_d0*d0 - mul_d1*d1) / Gain) * (100 / ITIME)" */
	if ( fp_temp < LUXTH1 ) {
		fp_lux = fp_mul(LUXTH1_D0_M,fp_d0) - fp_mul(LUXTH1_D1_M,fp_d1);
	} else if ( fp_temp < LUXTH2 ) {
		fp_lux = fp_mul(LUXTH2_D0_M,fp_d0) - fp_mul(LUXTH2_D1_M,fp_d1);
	} else if ( fp_temp < LUXTH3 ) {
		fp_lux = fp_mul(LUXTH3_D0_M,fp_d0) - fp_mul(LUXTH3_D1_M,fp_d1);
	} else if ( fp_temp < LUXTH4 ) {
		fp_lux = fp_mul(LUXTH4_D0_M,fp_d0) - fp_mul(LUXTH4_D1_M,fp_d1);
	} else {
		lux = 2;
		return lux;
	}

	/* fp_lux = fp_lux / (GAIN) */
	fp_lux = fp_div(fp_lux, gain_to_fp[BH1730_CONF_GAIN]);

	/* fp_temp = 100ms / ITIME */
	fp_temp = itime_to_ms_fp(BH1730_CONF_ITIME);
	fp_temp = fp_div(INT_TO_FP(100), fp_temp);

	/* final part, fp_lux * fp_temp */
	fp_lux = fp_mul(fp_lux, fp_temp);

	/* fp result to int */
	lux = FP_TO_INT(fp_lux);
	}

	return lux;
}

LOCAL int bh1730_gain_control(uint16_t *data0, uint16_t *data1)
{
	return EC_SUCCESS;
}

/**
 * Read BH1730 data0 and data1
 */
LOCAL int bh1730_read_data(uint16_t *data0, uint16_t *data1)
{

	int ret;
	int data;

#ifdef CONFIG_CMD_BH1730_TEST
	if (test_data_use) {
		*data0 = test_data0;
		*data1 = test_data1;
		test_data_use = 0;
		return EC_SUCCESS;
	}
#endif

	ret = i2c_read32(I2C_PORT_ALS, BH1730_I2C_ADDR,
			 BH1730_DATA0LOW, &data);

	if (ret != EC_SUCCESS)
		return ret;

	*data0 = 0x0000ffff & data;
	*data1 = (data >> 16) & 0x0000ffff;

	return EC_SUCCESS;
}

/**
 * Initialise BH1730 Ambient light sensor.
 */
int bh1730_init(void)
{
	int ret;

	ccprintf("bh1730_init \n");

	/* power and measurement bit high */
	ret = i2c_write8(I2C_PORT_ALS, BH1730_I2C_ADDR,
			 BH1730_CONTROL,
			 BH1730_CONTROL_POWER_ENABLE|BH1730_CONTROL_ADC_EN_ENABLE);

	if (ret != EC_SUCCESS) {
		ccprintf("bh1730_init - enable fail %d\n", ret);
		return ret;
	}

	/* set timing */
	ret = i2c_write8(I2C_PORT_ALS, BH1730_I2C_ADDR,
			 BH1730_TIMING, BH1730_CONF_ITIME);

	if (ret != EC_SUCCESS) {
		ccprintf("bh1730_init - time fail %d\n", ret);
		return ret;
	}

	/* set ADC gain */
	ret = i2c_write8(I2C_PORT_ALS, BH1730_I2C_ADDR,
			 BH1730_GAIN, BH1730_CONF_GAIN);

	if (ret != EC_SUCCESS) {
		ccprintf("bh1730_init - gain fail %d\n", ret);
		return ret;
	}

	ccprintf("bh1730_init ok\n");

	return EC_SUCCESS;
}

/**
 * Read BH1730 Ambient light sensor data.
 */
int bh1730_read_lux(int *lux, int af)
{
	int ret;
	uint16_t data0;
	uint16_t data1;

	ccprintf("bh1730_read_lux \n");

	/* read data0 and data1 from sensor */
	ret = bh1730_read_data(&data0, &data1);
	if (ret != EC_SUCCESS) {
		ccprintf("bh1730_read_lux - fail %d\n", ret);
		return ret;
	}

	/* do sensor gain control */
	ret = bh1730_gain_control(&data0, &data1);
	if (ret != EC_SUCCESS)
		return ret;

	/* convert sensor data0 and data1 to lux */
	*lux = bh1730_convert_to_lux(data0, data1);

	ccprintf("bh1730_read_lux d0=%d, d1=%d, lux=%d\n", data0, data1, *lux);

	*lux = *lux * af;

	return EC_SUCCESS;
}

#ifdef CONFIG_CMD_BH1730_TEST
/* BH1730 console test command */
/* "bh1730 init" = initialize sensor */
/* "bh1730" = read data from sensor and convert to lux */
/* "bh1730 539 82" = use command line test data and convert to lux */
static int command_bh1730_test(int argc, char **argv)
{
	int rv, lux, af;

	/* init sensor */
	if (argc == 2 && !strcasecmp(argv[1], "init")) {
		rv = bh1730_init();
		ccprintf("command_bh1730_test - rv %d, init\n", rv);
		return EC_SUCCESS;
	}

	/* use data0 and data1 from command line */
	if (argc == 3) {
		char *e;

		/* use test data0 and data1 from command line */
		test_data_use = 1;
		test_data0 = strtoi(argv[1], &e, 0);
		test_data1 = strtoi(argv[2], &e, 0);
	}

	af = 1;
	rv = bh1730_read_lux(&lux, af);

	ccprintf("command_bh1730_test - rv %d, lux = %d\n", rv, lux);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(bh1730, command_bh1730_test,
			NULL,
			"BH1730 test", "als test");
#endif

