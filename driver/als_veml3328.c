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

#define VEML3328_DRV_DATA(_s) ((struct als_drv_data_t *)(_s)->drv_data)
#define VEML3328_RGB_DRV_DATA(_s) \
	((struct veml3328_rgb_drv_data_t *)(_s)->drv_data)

/*
 * Read data from VEML3328 light sensor, and transfer unit into lux.
 */
static int veml3328_read(const struct motion_sensor_t *s, intv3_t v)
{
	struct als_drv_data_t *als_data = VEML3328_DRV_DATA(s);
	struct veml3328_rgb_drv_data_t *drv_data = VEML3328_RGB_DRV_DATA(s + 1);
	struct veml3328_calib *calib = &(drv_data->calib);
	int addr = s->i2c_spi_addr_flags;
	int port = s->port;
	fp_t CCTi, CCTi0;
	int c, r, g, b, ir;
	fp_t Ch, Cl, ir_div_c, tmp;

	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_C, &c));
	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_R, &r));
	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_G, &g));
	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_B, &b));
	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_IR, &ir));

	c = MAX(c, 1);
	r = MAX(r, 1);
	g = MAX(g, 1);
	b = MAX(b, 1);
	ir = MAX(ir, 1);

	ir_div_c = INT_TO_FP(ir) / INT_TO_FP(c);
	CCTi0 = fp_div(fp_mul(calib->per_model.Lccti0, INT_TO_FP(r + g - b)),
		       INT_TO_FP(c)) +
		fp_div(fp_mul(calib->per_model.Lccti1, INT_TO_FP(r + g)),
		       INT_TO_FP(b));

	if ((calib->per_model.Lccti0 > 0) && ((r + g - b) <= 0))
		CCTi = FLOAT_TO_FP(0.1);
	else if (CCTi0 > calib->per_model.X1)
		CCTi = fp_mul(CCTi0, calib->per_model.Y1);
	else if (CCTi0 < calib->per_model.X2)
		CCTi = fp_mul(CCTi0, calib->per_system.C_ccti);
	else {
		CCTi = fp_mul(CCTi0, (calib->per_system.C_ccti +
				      fp_mul((CCTi0 - calib->per_model.X2),
					     (fp_div((calib->per_model.Y1 -
						      calib->per_system.C_ccti),
						     (calib->per_model.X1 -
						      calib->per_model.X2))))));
	}

	if ((r == VEML3328_CH_MAX) || (g == VEML3328_CH_MAX) ||
	    (b == VEML3328_CH_MAX) || (c == VEML3328_CH_MAX) ||
	    (ir == VEML3328_CH_MAX)) {
		Ch = INT_TO_FP(1);
		Cl = INT_TO_FP(1);
	} else {
		if ((r < 10) || (g < 10) || (c < 10))
			Ch = INT_TO_FP(1);
		else {
			Ch = fp_mul(calib->per_model.Lh1, ir_div_c) +
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
			Cl = fp_mul(calib->per_model.Ll1,
				    CCTi + calib->per_model.Ll0);
			if (ir_div_c >= calib->per_model.Jl)
				Cl = INT_TO_FP(1);
			if (Cl >= calib->per_model.Cl_max)
				Cl = calib->per_model.Cl_max;
			if (Cl <= calib->per_model.Cl_min)
				Cl = calib->per_model.Cl_min;
		}
	}

	/*
	 * lux = calib->per_system.C_lux * Ch * Cl *
	 *      (calib->per_model.LG * g + calib->per_model.LC * c) / Lux_gain
	 */
	tmp = fp_mul(calib->per_system.C_lux, Ch);
	tmp = fp_mul(tmp, Cl);
	tmp = fp_mul(tmp, fp_mul(calib->per_model.LG, INT_TO_FP(g)) +
				  fp_mul(calib->per_model.LC, INT_TO_FP(c)));

	v[0] = FP_TO_INT(fp_div(tmp, FLOAT_TO_FP(VEML3328_DEFAULT_GAIN)));
	v[1] = 0;
	v[2] = 0;

	/*
	 * Return an error when value didn't change to prevent filling the
	 * fifo with useless data.
	 */
	if (v[0] == als_data->last_value)
		return EC_ERROR_UNCHANGED;

	als_data->last_value = v[0];
	return EC_SUCCESS;
}

static int veml3328_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	VEML3328_DRV_DATA(s)->als_cal.scale = range >> 16;
	VEML3328_DRV_DATA(s)->als_cal.uscale = range & 0xffff;
	s->current_range = range;
	return EC_SUCCESS;
}

static int veml3328_set_data_rate(const struct motion_sensor_t *s, int rate,
				  int roundup)
{
	/* TODO: validate rate is valid */
	VEML3328_DRV_DATA(s)->rate = rate;
	return EC_SUCCESS;
}

static int veml3328_get_data_rate(const struct motion_sensor_t *s)
{
	return VEML3328_DRV_DATA(s)->rate;
}

static int veml3328_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
			      int16_t *temp)
{
	scale[X] = VEML3328_DRV_DATA(s)->als_cal.channel_scale.k_channel_scale;
	scale[Y] = 0;
	scale[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int veml3328_set_scale(const struct motion_sensor_t *s,
			      const uint16_t *scale, int16_t temp)
{
	if (scale[X] == 0)
		return EC_ERROR_INVAL;
	VEML3328_DRV_DATA(s)->als_cal.channel_scale.k_channel_scale = scale[X];
	return EC_SUCCESS;
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
	offset[X] = VEML3328_DRV_DATA(s)->als_cal.offset;
	offset[Y] = 0;
	offset[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;

	return EC_SUCCESS;
}

static int veml3328_perform_calib(struct motion_sensor_t *s, int enable)
{
	VEML3328_RGB_DRV_DATA(s + 1)->calibration_mode = enable;
	return EC_SUCCESS;
}

/**
 * Initialise VEML3328 light sensor.
 */
static int veml3328_init(struct motion_sensor_t *s)
{
	int ret;
	int id;

	CPRINTS("veml3328 ALS init start");

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

	/* TODO - what should be the reset timing ?? */
	msleep(1);

	/* Power on, write default config */
	ret = i2c_write16(s->port, s->i2c_spi_addr_flags, VEML3328_REG_CONF,
			  VEML3328_CONF_DEFAULT);
	if (ret != EC_SUCCESS) {
		CPRINTS("veml3328 error writing to CONF reg %d", ret);
		return ret;
	}

	/* TODO - what should be the reset timing ?? */
	msleep(1);

	/* Check chip ID */
	ret = i2c_read16(s->port, s->i2c_spi_addr_flags, VEML3328_REG_ID, &id);
	if (ret != EC_SUCCESS) {
		CPRINTS("veml3328 failed reading ID reg ret=%d", ret);
		return ret;
	}

	id &= VEML3328_DEV_ID_MASK;
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
	.set_scale = veml3328_set_scale,
	.get_scale = veml3328_get_scale,
	.set_data_rate = veml3328_set_data_rate,
	.get_data_rate = veml3328_get_data_rate,
	.perform_calib = veml3328_perform_calib,
};

/*
 * ============== The RGB portion of the driver ==============
 */

static int veml3328_rgb_read(const struct motion_sensor_t *s, intv3_t v)
{
	struct veml3328_rgb_drv_data_t *drv_data = VEML3328_RGB_DRV_DATA(s);
	int r, g, b;
	int addr = s->i2c_spi_addr_flags;
	int port = s->port;

	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_R, &r));
	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_G, &g));
	RETURN_ERROR(i2c_read16(port, addr, VEML3328_REG_B, &b));

	if (drv_data->calibration_mode) {
		v[0] = r;
		v[1] = g;
		v[2] = b;
	} else {
		/* TODO: convert to lux */
	}

	return EC_SUCCESS;
}

static int veml3328_rgb_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	return EC_SUCCESS;
}

static int veml3328_rgb_set_offset(const struct motion_sensor_t *s,
				   const int16_t *offset, int16_t temp)
{
	/* Do not allow offset to be changed, it's predetermined */
	return EC_SUCCESS;
}

static int veml3328_rgb_get_offset(const struct motion_sensor_t *s,
				   int16_t *offset, int16_t *temp)
{
	offset[X] = VEML3328_RGB_DRV_DATA(s)->calibration.rgb_cal[X].offset;
	offset[Y] = VEML3328_RGB_DRV_DATA(s)->calibration.rgb_cal[Y].offset;
	offset[Z] = VEML3328_RGB_DRV_DATA(s)->calibration.rgb_cal[Z].offset;

	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int veml3328_rgb_set_scale(const struct motion_sensor_t *s,
				  const uint16_t *scale, int16_t temp)
{
	struct rgb_channel_calibration_t *rgb_cal =
		VEML3328_RGB_DRV_DATA(s)->calibration.rgb_cal;

	if (scale[X] == 0 || scale[Y] == 0 || scale[Z] == 0)
		return EC_ERROR_INVAL;

	rgb_cal[RED_RGB_IDX].scale.k_channel_scale = scale[X];
	rgb_cal[GREEN_RGB_IDX].scale.k_channel_scale = scale[Y];
	rgb_cal[BLUE_RGB_IDX].scale.k_channel_scale = scale[Z];

	return EC_SUCCESS;
}

static int veml3328_rgb_get_scale(const struct motion_sensor_t *s,
				  uint16_t *scale, int16_t *temp)
{
	struct rgb_channel_calibration_t *rgb_cal =
		VEML3328_RGB_DRV_DATA(s)->calibration.rgb_cal;

	scale[X] = rgb_cal[RED_RGB_IDX].scale.k_channel_scale;
	scale[Y] = rgb_cal[GREEN_RGB_IDX].scale.k_channel_scale;
	scale[Z] = rgb_cal[BLUE_RGB_IDX].scale.k_channel_scale;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;

	return EC_SUCCESS;
}

static int veml3328_rgb_set_data_rate(const struct motion_sensor_t *s, int rate,
				      int roundup)
{
	return EC_SUCCESS;
}

static int veml3328_rgb_get_data_rate(const struct motion_sensor_t *s)
{
	/* Clear ALS should be defined before RGB sensor */
	return veml3328_get_data_rate(s - 1);
}

static int veml3328_rgb_init(struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

const struct accelgyro_drv veml3328_rgb_drv = {
	.init = veml3328_rgb_init,
	.read = veml3328_rgb_read,
	.set_range = veml3328_rgb_set_range,
	.set_offset = veml3328_rgb_set_offset,
	.get_offset = veml3328_rgb_get_offset,
	.set_scale = veml3328_rgb_set_scale,
	.get_scale = veml3328_rgb_get_scale,
	.set_data_rate = veml3328_rgb_set_data_rate,
	.get_data_rate = veml3328_rgb_get_data_rate,
};
