/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Rohm BH1730 Ambient light sensor driver
 */

#include "driver/als_bh1730.h"
#include "i2c.h"
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_SENSE, format, ## args)

/* Sensor configuration */
/* Select Gain */
#define BH1730_CONF_GAIN BH1730_GAIN_GAIN_X64_GAIN
#define BH1730_GAIN_DIV 64

/* Select Itime, 0xDA is 102.6ms = 38*2.7ms */
#define BH1730_CONF_ITIME 0xDA
#define ITIME_MS_X_10 ((256 - BH1730_CONF_ITIME) * 27)
#define ITIME_MS_X_1K (ITIME_MS_X_10*100)

/* ALS CONSTANT will be changed according to project */
#ifndef CONFIG_ALS_BH1730_CONSTANTS
#define LUXTH1_1K		260
#define LUXTH1_D0_1K		1290
#define LUXTH1_D1_1K		2733
#define LUXTH2_1K		550
#define LUXTH2_D0_1K		797
#define LUXTH2_D1_1K		859
#define LUXTH3_1K		1090
#define LUXTH3_D0_1K		510
#define LUXTH3_D1_1K		345
#define LUXTH4_1K		2130
#define LUXTH4_D0_1K		276
#define LUXTH4_D1_1K		130
#endif

/**
 * Convert BH1730 data0, data1 to lux
 */
static int bh1730_convert_to_lux(uint32_t data0_1)
{
	int lux;
	uint16_t data0 = 0x0000ffff & data0_1;
	uint16_t data1 = (data0_1 >> 16) & 0x0000ffff;

	if ( data0 == 0 ) {
		return 2;
	}

	{
	uint32_t d0_1k = data0 * 1000;
	uint32_t d1_1k = data1 * 1000;
	uint32_t d_temp = d1_1k / d0_1k;
	uint32_t d_lux;

	if(d_temp < LUXTH1_1K) {
		d0_1k = LUXTH1_D0_1K * data0;
		d1_1k = LUXTH1_D1_1K * data1;
	}
	else if(d_temp < LUXTH2_1K) {
		d0_1k = LUXTH2_D0_1K * data0;
		d1_1k = LUXTH2_D1_1K * data1;
	}
	else if(d_temp < LUXTH3_1K) {
		d0_1k = LUXTH3_D0_1K * data0;
		d1_1k = LUXTH3_D1_1K * data1;
	}
	else if(d_temp < LUXTH4_1K) {
		d0_1k = LUXTH4_D0_1K * data0;
		d1_1k = LUXTH4_D1_1K * data1;
	}
	else {
		return 2;
	}

	d_lux = (d0_1k - d1_1k) / BH1730_GAIN_DIV;
	d_lux *= 100;
	lux = d_lux / ITIME_MS_X_1K;
	}

	CPRINTF("bh1730_convert_to_lux d0=%d, d1=%d, lux=%d\n", data0, data1, lux);

	return lux;
}

/**
 * Initialise BH1730 Ambient light sensor.
 */
static int bh1730_init_sensor(int port, int addr)
{
	int ret;

	CPRINTF("bh1730_init_sensor \n");

	/* power and measurement bit high */
	ret = i2c_write8(port, addr,
			BH1730_CONTROL,
			BH1730_CONTROL_POWER_ENABLE|BH1730_CONTROL_ADC_EN_ENABLE);

	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_init_sensor - enable fail %d\n", ret);
		return ret;
	}

	/* set timing */
	ret = i2c_write8(port, addr, BH1730_TIMING, BH1730_CONF_ITIME);

	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_init_sensor - time fail %d\n", ret);
		return ret;
	}

	/* set ADC gain */
	ret = i2c_write8(port, addr, BH1730_GAIN, BH1730_CONF_GAIN);

	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_init_sensor - gain fail %d\n", ret);
		return ret;
	}

	CPRINTF("bh1730_init_sensor ok\n");

	return EC_SUCCESS;
}

#ifdef HAS_TASK_ALS
/**
 * Initialise BH1730 Ambient light sensor.
 */
int bh1730_init(void)
{
	int ret;

	CPRINTF("bh1730_init \n");

	ret = bh1730_init_sensor(I2C_PORT_ALS, BH1730_I2C_ADDR);

	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_init_sensor - fail %d\n", ret);
		return ret;
	}

	CPRINTF("sensor_init_done()\n");

	return EC_SUCCESS;
}

/**
 * Read BH1730 Ambient light sensor data.
 */
int bh1730_read_lux(int *lux, int af)
{
	int ret;
	int data0_1;

	/* Read data0 and data1 from sensor */
	ret = i2c_read32(I2C_PORT_ALS, BH1730_I2C_ADDR,
			BH1730_DATA0LOW, &data0_1);

	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_read_lux - fail %d\n", ret);
		return ret;
	}

	/* convert sensor data0 and data1 to lux */
	*lux = bh1730_convert_to_lux(data0_1);
	*lux = *lux * af;

	return EC_SUCCESS;
}

#else    /* HAS_TASK_ALS */

#include "accelgyro.h"

/**
 * Read BH1730 light sensor data.
 */
int bh1730_read_lux(const struct motion_sensor_t *s, vector_3_t v)
{
	struct bh1730_drv_data_t *drv_data = BH1730_GET_DATA(s);
	int ret;
	int data0_1;

	/* read data0 and data1 from sensor */
	ret = i2c_read32(s->port, s->addr, BH1730_DATA0LOW, &data0_1);
	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_read_lux - fail %d\n", ret);
		return ret;
	}

	/* convert sensor data0 and data1 to lux */
	v[0] = bh1730_convert_to_lux(data0_1);
	v[1] = 0;
	v[2] = 0;

	/*
	 * Return an error when nothing change to prevent filling the
	 * fifo with useless data.
	 */
	if (v[0] == drv_data->last_value)
		return EC_ERROR_UNCHANGED;
	else
		return EC_SUCCESS;
}

static int bh1730_set_range(const struct motion_sensor_t *s, int range,
			     int rnd)
{
	return EC_SUCCESS;
}

static int bh1730_get_range(const struct motion_sensor_t *s)
{
	return 1;
}

/* default Itime is about 10Hz */
#define BH1730_10000_mHz (10*1000)

static int bh1730_set_data_rate(const struct motion_sensor_t *s,
				int rate, int roundup)
{
	struct bh1730_drv_data_t *drv_data = BH1730_GET_DATA(s);

	/* now only one rate supported */
	drv_data->rate = BH1730_10000_mHz;

	return EC_SUCCESS;
}

static int bh1730_get_data_rate(const struct motion_sensor_t *s)
{
	struct bh1730_drv_data_t *drv_data = BH1730_GET_DATA(s);

	return drv_data->rate;
}

static int bh1730_set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	return EC_SUCCESS;
}

static int bh1730_get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	*offset = 0;

	return EC_SUCCESS;
}

/**
 * Initialise BH1730 Ambient light sensor.
 */
static int bh1730_init(const struct motion_sensor_t *s)
{
	int ret;

	CPRINTF("bh1730_init \n");

	ret = bh1730_init_sensor(s->port, s->addr);

	if (ret != EC_SUCCESS) {
		CPRINTF("bh1730_init_sensor - fail %d\n", ret);
		return ret;
	}

	CPRINTF("sensor_init_done()\n");

	return EC_SUCCESS;
}

const struct accelgyro_drv bh1730_drv = {
	.init = bh1730_init,
	.read = bh1730_read_lux,
	.set_range = bh1730_set_range,
	.get_range = bh1730_get_range,
	.set_offset = bh1730_set_offset,
	.get_offset = bh1730_get_offset,
	.set_data_rate = bh1730_set_data_rate,
	.get_data_rate = bh1730_get_data_rate,
};
#endif   /* HAS_TASK_ALS */

