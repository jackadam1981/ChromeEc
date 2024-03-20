/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Vishay VEML3328 light sensor driver
 */
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/als_veml3328.h"
#include "i2c.h"
#include "math_util.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ##args)
#define VEML3328_CH_MAX (65535)

#define VEML3328_GET_DATA(_s) ((struct veml3328_drv_data_t *)(_s)->drv_data)

/*
 * Read VEML3328 light sensor data.
 */
static int veml3328_read_lux(const struct motion_sensor_t *s,
			     const struct veml3328_calib *calib, int *lux)
{
	int ret;
	fp_t CCTi, CCTi0;
	uint16_t raw_data[5];
	int c, r, g, b, ir;
	fp_t Ch, Cl, ir_div_c;

	for (int i = 0; i < 5; i++) {
		ret = i2c_read16(s->port, s->i2c_spi_addr_flags,
				 VEML3328_REG_C_DATA + i, &(raw_data[i]));
		if (ret != EC_SUCCESS)
			return ret;
	}

	c = MAX(raw_data[0], 1);
	r = MAX(raw_data[1], 1);
	g = MAX(raw_data[2], 1);
	b = MAX(raw_data[3], 1);
	ir = MAX(raw_data[4], 1);

	ir_div_c = fp_div(INT_TO_FP(ir), INT_TO_FP(c));
	CCTi0 = fp_div(calib->per_model.Lccti0 * INT_TO_FP(r + g - b),
		       INT_TO_FP(c)) +
		fp_div(calib->per_model.Lccti1 * INT_TO_FP(r + g),
		       INT_TO_FP(b));

	if ((calib->per_model.Lccti0 > 0) && ((r + g - b) <= 0))
		CCTi = FLOAT_TO_FP(0.1);
	else if (CCTi0 > calib->per_model.X1)
		CCTi = CCTi0 * calib->per_model.Y1;
	else if (CCTi0 < calib->per_model.X2)
		CCTi = CCTi0 * calib->per_system.C_ccti;
	else {
		CCTi = CCTi0 *
		       (calib->per_system.C_ccti +
			(CCTi0 - calib->per_model.X2) *
				((calib->per_model.Y1 -
				  calib->per_system.C_ccti) /
				 (calib->per_model.X1 - calib->per_model.X2)));
	}

	if ((r == CH_MAX) || (g == CH_MAX) || (b == CH_MAX) || (c == CH_MAX) ||
	    (ir == CH_MAX)) {
		Ch = INT_TO_FP(1);
		Cl = INT_TO_FP(1);
	} else {
		if ((r < 10) || (g < 10) || (c < 10))
			Ch = INT_TO_FP(1);
		else {
			Ch = calib->per_model.Lh1 * ir_div_c +
			     calib->per_model.Lh0;
			if (ir_div_c <= calib->per_model.Jh)
				Ch = INT_TO_FP(1);
			if (Ch >= calib->per_model.Ch_max)
				Ch = calib->per_model.Ch_max;
			if (Ch <= calib->per_model.Ch_min)
				Ch = calib->per_model.Ch_min;
		}
		if ((r < 10) || (g < 10) || (b < 10) || (c < 10))
			Cl = INT_TO_FP(1);
		else {
			Cl = calib->per_model.Ll1 * CCTi + calib->per_model.Ll0;
			if (ir_div_c >= calib->per_model.Jl)
				Cl = INT_TO_FP(1);
			if (Cl >= calib->per_model.Cl_max)
				Cl = calib->per_model.Cl_max;
			if (Cl <= calib->per_model.Cl_min)
				Cl = calib->per_model.Cl_min;
		}
	}

	*lux = FP_TO_INT(calib->per_system.C_lux * Ch * Cl *
			 fp_div(calib->per_model.LG * INT_TO_FP(g) +
					calib->per_model.LC * INT_TO_FP(c),
				FLOAT_TO_FP(VEML3328_DEFAULT_GAIN)));

	return EC_SUCCESS;
}

/*
 * Read data from VEML3328 light sensor, and transfer unit into lux.
 */
static int veml3328_read(const struct motion_sensor_t *s, intv3_t v)
{
	struct veml3328_drv_data_t *drv_data = VEML3328_GET_DATA(s);
	int ret;
	int lux;
	static int last_value;

	ret = veml3328_read_lux(s, &drv_data->calib, &lux);
	if (ret != EC_SUCCESS)
		return ret;

	v[0] = lux;
	v[1] = 0;
	v[2] = 0;
	/*
	 * Return an error when value didn't change to prevent filling the
	 * fifo with useless data.
	 */
	if (v[0] == last_value)
		return EC_ERROR_UNCHANGED;

	last_value = v[0];
	return EC_SUCCESS;
}

static int veml3328_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	return EC_SUCCESS;
}

static int veml3328_set_data_rate(const struct motion_sensor_t *s, int rate,
				  int roundup)
{
	/* TODO: validate rate is valid */
	VEML3328_GET_DATA(s)->rate = rate;
	return EC_SUCCESS;
}

static int veml3328_get_data_rate(const struct motion_sensor_t *s)
{
	return VEML3328_GET_DATA(s)->rate;
}

static int veml3328_set_offset(const struct motion_sensor_t *s,
			       const int16_t *offset, int16_t temp)
{
	/* TODO: check calibration method */
	return EC_SUCCESS;
}

static int veml3328_get_offset(const struct motion_sensor_t *s, int16_t *offset,
			       int16_t *temp)
{
	/* TODO: check calibration method */
	return EC_SUCCESS;
}

/**
 * Initialise VEML3328 light sensor.
 */
static int veml3328_init(struct motion_sensor_t *s)
{
	int ret;
	int id;

	if (s->i2c_spi_addr_flags != VEML3328_I2C_ADDR) {
		CPRINTS("veml3328 address has to be %d", VEML3328_I2C_ADDR);
		return EC_ERROR_INVAL;
	}

	/* Shutdown */
	ret = i2c_write16(s->port, s->i2c_spi_addr_flags, VEML3328_REG_CONF,
			  VEML3328_SD);
	if (ret != EC_SUCCESS) {
		CPRINTS("veml3328 error writing to CONF reg %d", ret);
		return ret;
	}

	/* Power on, write default config */
	ret = i2c_write16(s->port, s->i2c_spi_addr_flags, VEML3328_REG_CONF,
			  VEML3328_CONF_DEFAULT);
	if (ret != EC_SUCCESS) {
		CPRINTS("veml3328 error writing to CONF reg %d", ret);
		return ret;
	}

	/* Check chip ID */
	ret = i2c_read16(s->port, s->i2c_spi_addr_flags, VEML3328_REG_ID, &id);
	if (ret != EC_SUCCESS)
		CPRINTS("veml3328 failed reading ID reg ret=%d", ret);
	return ret;

	if (id != VEML3328_DEV_ID) {
		CPRINTS("veml3328 wrong chip ID=%d", id);
		return EC_ERROR_INVAL;
	}

	CPRINTS("veml3328 ALS init successful");

	return sensor_init_done(s);
}

const struct accelgyro_drv veml3328_drv = {
	.init = veml3328_init,
	.read = veml3328_read,
	.set_range = veml3328_set_range,
	.set_offset = veml3328_set_offset,
	.get_offset = veml3328_get_offset,
	.set_data_rate = veml3328_set_data_rate,
	.get_data_rate = veml3328_get_data_rate,
};
