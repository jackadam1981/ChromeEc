/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMS TCS3408 light sensor driver
 */
#include "accelgyro.h"
#include "als_tcs3408.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "hwtimer.h"
#include "i2c.h"
#include "math_util.h"
#include "motion_sense_fifo.h"
#include "task.h"
#include "util.h"

#ifdef CONFIG_ALS_TCS3408_INT_EVENT
#define ALS_TCS3408_INT_ENABLE
#endif

#define CPRINTS(fmt, args...) cprints(CC_ACCEL, "%s " fmt, __func__, ##args)

volatile uint32_t last_interrupt_timestamp;

uint16_t tcs3408_als_gains[] = {
    0,
    1,
    2,
    4,
    8,
    16,
    32,
    64,
    128,
    256,
    512,
    1024,
    2048
};

/* DGF, C, R, G, B */
int tcs3408_lux_coefs[2][5] = {
    {833, 20, -20, 310, -160}, //high
    {833, 20, -20, 310, -160}  //low
};

int cct_calc[2][2] = {
    //coefA, cctOffset
    {87, 2359}, //high
    {12000, 2003} //low
};

#ifdef CONFIG_TCS_USE_LUX_TABLE
/*
 * Stores the number of atime increments/decrements needed to change light value
 * by 1% of saturation for each gain setting for each predefined LUX range.
 *
 * Values in array are TCS_ATIME_GAIN_FACTOR (100x) times actual value to allow
 * for fractions using integers.
 */
static const uint16_t range_atime[TCS_MAX_AGAIN - TCS_MIN_AGAIN +
				  1][TCS_MAX_ATIME_RANGES] = {
	{ 11200, 5600, 5600, 7200, 5500, 4500, 3800, 3800, 3300, 2900, 2575,
	  2275, 2075 },
	{ 11200, 5100, 2700, 1840, 1400, 1133, 981, 963, 833, 728, 650, 577,
	  525 },
	{ 250, 1225, 643, 441, 337, 276, 253, 235, 203, 176, 150, 0, 0 },
	{ 790, 261, 163, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

// static void decrement_atime(struct tcs_saturation_t *sat_p, uint16_t cur_lux,
// 			    int percent)
// {
// 	int atime;
// 	uint16_t steps;
// 	int lux = MIN(cur_lux, TCS_GAIN_TABLE_MAX_LUX);

// 	steps = percent * range_atime[sat_p->again][lux / 1000] /
// 		TCS_ATIME_GAIN_FACTOR;
// 	atime = MAX(sat_p->atime - steps, TCS_MIN_ATIME);
// 	sat_p->atime = MIN(atime, TCS_MAX_ATIME);
// }

#else
// auto gain control, use 1x 4x 16x 64x 256x,512X, 1024X, 2048X gain values
int tcs3408_gains_mask[] = {
    0, //0  0,
    1, //1  1,
    0, //2  2,
    1, //3  4,
    0, //4  8,
    1, //5  16,
    0, //6  32,
    1, //7  64,
    0, //8  128,
    1, //9  256,
    1, //10 512,
    1, //11 1024,
    1, //12 2048
};

static int tcs3408_get_als_new_gain(int cfg1, int value)
{
    int i = cfg1 + value;
    int new_cfg1 = -1;

    if (value > 0) {
        for (; i < sizeof(tcs3408_gains_mask); i++) {
            if (tcs3408_gains_mask[i] != 0) {
                new_cfg1 = i;
                break;
            }
        }
    } else {
        for (; i >= 0; i--) {
            if (tcs3408_gains_mask[i] != 0) {
                new_cfg1 = i;
                break;
            }
        }
    }

    if (new_cfg1 < 0 || new_cfg1 >= sizeof(tcs3408_gains_mask)) {
        new_cfg1 = cfg1;
    }

    return new_cfg1;
}

// static void decrement_atime(struct tcs_saturation_t *sat_p,
// 			    uint16_t __attribute__((unused)) cur_lux,
// 			    int __attribute__((unused)) percent)
// {
// 	sat_p->atime = MAX(sat_p->atime - TCS_ATIME_DEC_STEP, TCS_MIN_ATIME);
// }

#endif /* CONFIG_TCS_USE_LUX_TABLE */

// static void increment_atime(struct tcs_saturation_t *sat_p)
// {
// 	sat_p->atime = MIN(sat_p->atime + TCS_ATIME_INC_STEP, TCS_MAX_ATIME);
// }

static inline int tcs3408_i2c_read8(const struct motion_sensor_t *s, int reg,
				    int *data)
{
	return i2c_read8(s->port, s->i2c_spi_addr_flags, reg, data);
}

static inline int tcs3408_i2c_write8(const struct motion_sensor_t *s, int reg,
				     int data)
{
	return i2c_write8(s->port, s->i2c_spi_addr_flags, reg, data);
}


static int tcs3408_i2c_modify(const struct motion_sensor_t *chip, uint8_t reg, uint8_t mask, int val)
{
    int ret;
    int temp = 0;

    ret = tcs3408_i2c_read8(chip, reg, &temp);
    temp &= ~mask;
    temp |= val;
    ret = tcs3408_i2c_write8(chip, reg, temp);

    return ret;
}

static void tcs3408_set_als_thresh(struct motion_sensor_t *chip, uint16_t high, uint16_t low)
{
    tcs3408_i2c_write8(chip, TCS_I2C_AILTL, (uint8_t)(low & 0x00FF));
    tcs3408_i2c_write8(chip, TCS_I2C_AILTH, (uint8_t)((low & 0xFF00) >> 8));
    tcs3408_i2c_write8(chip, TCS_I2C_AIHTL, (uint8_t)(high & 0x00FF));
    tcs3408_i2c_write8(chip, TCS_I2C_AIHTH, (uint8_t)((high & 0xFF00) >> 8));
}

#ifndef CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT
static int tcs3408_get_new_thresh(struct motion_sensor_t *chip, uint16_t *high, uint16_t *low, uint16_t value)
{
    uint32_t margin;
    int time;

    tcs3408_i2c_read8(chip, TCS_I2C_ATIME, &time); //atime
    margin = value * chip->deltaP / 100;
    if (value + margin >= (time * 8 / 10)) {
        *high = value + margin;
    } else {
        *high = (time * 8 / 10);
    }
    if (value > margin) {
        *low = value - margin;
    } else {
        *low = 0;
    }
    CPRINTS("High: %d, Low: %d\n", *high, *low);
    return 1;
}
#endif 

#ifdef CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT
static void tcs3408_read_deferred(void)
#else
void tcs3408_interrupt(enum gpio_signal signal)
#endif
{
	last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE, CONFIG_ALS_TCS3408_INT_EVENT);
}
#ifdef CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT
DECLARE_DEFERRED(tcs3408_read_deferred);
#endif

/* convert ATIME register to integration time, in microseconds */
int tcs3408_get_integration_time(int atime)
{
	return TCS_MAX_INTEGRATION_TIME * (TCS_ATIME_GRANULARITY - atime);
}

static int tcs3408_read(const struct motion_sensor_t *s, intv3_t v)
{
	int atime, again;
	int ret;

	/* Chip may have been off, make sure to setup important registers */
	if (TCS3408_RGB_DRV_DATA(s + 1)->calibration_mode) {
		atime = TCS_CALIBRATION_ATIME;
		again = TCS_CALIBRATION_AGAIN;
	} else {
		atime = TCS3408_RGB_DRV_DATA(s + 1)->saturation.atime;
		again = TCS3408_RGB_DRV_DATA(s + 1)->saturation.again;
	}
	ret = tcs3408_i2c_write8(s, TCS_I2C_ATIME, atime);
	if (ret)
		return ret;
	ret = tcs3408_i2c_write8(s, TCS_I2C_CFG1, again);
	if (ret)
		return ret;

	// /* Enable power, ADC, and interrupt to start cycle */
	 ret = tcs3408_i2c_write8(s, TCS_I2C_ENABLE, TCS3408_MODE_COLLECTING);
	if (ret)
		return ret;

	ret = tcs3408_i2c_write8(s, TCS_I2C_INTENAB, 0);
	 if (ret)
	 	return ret;

#ifdef CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT
	hook_call_deferred(&tcs3408_read_deferred_data,
			   tcs3408_get_integration_time(atime));
#endif

	/*
	 * If write succeeded, we've started the read process, but can't
	 * complete it yet until data is ready, so pass back EC_RES_IN_PROGRESS
	 * to inform upper level that read data process is under way and data
	 * will be delivered when available.
	 */
	return EC_RES_IN_PROGRESS;
}

static int tcs3408_rgb_read(const struct motion_sensor_t *s, intv3_t v)
{
	return EC_SUCCESS;
}

/*
 * tcs3408_adjust_sensor_for_saturation() tries to keep CRGB values as
 * close to saturation as possible without saturating by implementing
 * the following logic:
 *
 * If any of the R, G, B, or C channels have saturated, then decrease AGAIN.
 * If AGAIN is already at its minimum, increase ATIME if not at its max already.
 *
 * Else if none of the R, G, B, or C channels have saturated, and
 * all samples read are less than 90% of saturation, then increase
 * AGAIN if it is not already at its maximum, or if it is, decrease
 * ATIME if it is not at it's minimum already.
 */
static int tcs3408_adjust_sensor_for_saturation(struct motion_sensor_t *s,
						uint16_t cur_lux,
						uint16_t *crgb_data,
						uint32_t status)
{
	uint16_t save_again ;
	uint16_t save_atime ;
	uint32_t calc_ir , cpl, mode; 
	int32_t lux;
	int data[2] = {0};
	struct tcs_saturation_t *sat_p =
		&TCS3408_RGB_DRV_DATA(s + 1)->saturation;

	tcs3408_i2c_read8(s, TCS_I2C_ATIME, &data[0]); //atime
    tcs3408_i2c_read8(s, TCS_I2C_CFG1, &data[1]); // again

	/* Adjust for saturation if needed */
	if (!(status & TCS_I2C_STATUS_RGBC_VALID))
		return EC_SUCCESS;

	save_again = tcs3408_als_gains[data[1] & TCS3408_MASK_AGAIN];
	save_atime = ((uint16_t)data[0] + 1) * ASTEP_US_PER_100;
	save_atime /= 100;
	cpl = save_atime * save_again;

	calc_ir = (crgb_data[0]+crgb_data[1]+crgb_data[2]+crgb_data[3]) / 2;
	if (calc_ir < 0) {
        calc_ir = 0;
    }
    if (crgb_data[0] == 0) {
        calc_ir = 0;
    } else {
        calc_ir = calc_ir * COEF_SCALE / crgb_data[0];
    }

    if (calc_ir > DIV_POINT) {
        mode = 0;
    } else {
        mode = 1;
    }
    calc_ir = calc_ir / COEF_SCALE;

	int32_t k = tcs3408_lux_coefs[mode][1] * crgb_data[0] +
				tcs3408_lux_coefs[mode][2] * crgb_data[1] +
				tcs3408_lux_coefs[mode][3] * crgb_data[2] +
				tcs3408_lux_coefs[mode][4] * crgb_data[3] ;
	
	lux = tcs3408_lux_coefs[mode][0] * k / cpl;
    lux = lux / COEF_SCALE;
	sat_p->again = data[1];
	sat_p->atime = data[0];

	s->xyz[X] = (int16_t)crgb_data[1];
	s->xyz[Y] = (int16_t)crgb_data[2];
	s->xyz[Z] = (int16_t)crgb_data[3];
	CPRINTS("~~~~~~LUX k: %d~~~~~~~~~",lux);
	return EC_SUCCESS;
}

/**
 * normalize_channel_data - normalize the light data to remove effect of
 * different atime and again settings from the sample.
 */
static uint32_t normalize_channel_data(struct motion_sensor_t *s,
				       uint32_t sample)
{
	return sample;
	// struct tcs_saturation_t *sat_p =
	// 	&(TCS3408_RGB_DRV_DATA(s + 1)->saturation);
	// const uint16_t cur_gain = ((2 * sat_p->again));
	// const uint16_t cal_again = ((2 * TCS_CALIBRATION_AGAIN));

	// return DIV_ROUND_NEAREST(
	// 	sample * (TCS_ATIME_GRANULARITY - TCS_CALIBRATION_ATIME) *
	// 		cal_again,
	// 	(TCS_ATIME_GRANULARITY - sat_p->atime) * cur_gain);
}

__overridable void tcs3408_translate_to_xyz(struct motion_sensor_t *s,
					    int32_t *crgb_data,
					    int32_t *xyz_data)
{
	struct tcs3408_rgb_drv_data_t *rgb_drv_data =
		TCS3408_RGB_DRV_DATA(s + 1);
	int32_t crgb_prime[CRGB_COUNT];
	int32_t ir;
	int i;

	/* normalize the data for atime and again changes */
	for (i = 0; i < CRGB_COUNT; i++)
		crgb_data[i] = normalize_channel_data(s, crgb_data[i]);

	/* IR removal */
	ir = FP_TO_INT(fp_mul(INT_TO_FP(crgb_data[1] + crgb_data[2] +
					crgb_data[3] - crgb_data[0]),
			      rgb_drv_data->calibration.irt) /
		       2);

	for (i = 0; i < ARRAY_SIZE(crgb_prime); i++) {
		if (crgb_data[i] < ir)
			crgb_prime[i] = 0;
		else
			crgb_prime[i] = crgb_data[i] - ir;
	}

	/* if CC == 0, set BC = 0 */
	if (crgb_prime[CLEAR_CRGB_IDX] == 0)
		crgb_prime[BLUE_CRGB_IDX] = 0;

	/* regression fit to XYZ space */
	for (i = 0; i < 3; i++) {
		const struct rgb_channel_calibration_t *p =
			&rgb_drv_data->calibration.rgb_cal[i];

		xyz_data[i] = p->offset +
			      FP_TO_INT((fp_inter_t)p->coeff[RED_CRGB_IDX] *
						crgb_prime[RED_CRGB_IDX] +
					(fp_inter_t)p->coeff[GREEN_CRGB_IDX] *
						crgb_prime[GREEN_CRGB_IDX] +
					(fp_inter_t)p->coeff[BLUE_CRGB_IDX] *
						crgb_prime[BLUE_CRGB_IDX] +
					(fp_inter_t)p->coeff[CLEAR_CRGB_IDX] *
						crgb_prime[CLEAR_CRGB_IDX]);

		if (xyz_data[i] < 0)
			xyz_data[i] = 0;
	}
}

static void tcs3408_process_raw_data(struct motion_sensor_t *s,
				     uint8_t *raw_data_buf,
				     uint16_t *raw_light_data,
				     int32_t *xyz_data)
{
	struct als_drv_data_t *als_drv_data = TCS3408_DRV_DATA(s);
	struct tcs3408_rgb_drv_data_t *rgb_drv_data =
		TCS3408_RGB_DRV_DATA(s + 1);
	const uint8_t calibration_mode = rgb_drv_data->calibration_mode;
	uint16_t k_channel_scale =
		als_drv_data->als_cal.channel_scale.k_channel_scale;
	uint16_t cover_scale = als_drv_data->als_cal.channel_scale.cover_scale;
	int32_t crgb_data[CRGB_COUNT];
	int i;

	/* adjust for calibration and scale data */
	for (i = 0; i < CRGB_COUNT; i++) {
		int index = i * 2;

		/* assemble the light value for this channel */
		 crgb_data[i] = raw_light_data[i] =
			((raw_data_buf[index + 1] << 8) | raw_data_buf[index]);

		/* in calibration mode, we only assemble the raw data */
		if (calibration_mode)
			continue;
		
		
		/* rgb data at index 1, 2, and 3 owned by rgb driver, not ALS */
		if (i >= 0) {
			struct als_channel_scale_t *csp =
				&rgb_drv_data->calibration.rgb_cal[i - 1].scale;
			k_channel_scale = csp->k_channel_scale;
			cover_scale = csp->cover_scale;
		}

		/* Step 1: divide by individual channel scale value */
		
		crgb_data[i] =crgb_data[i]*k_channel_scale/ALS_CAL_COEF_SCALE;
	}
	CPRINTS("~~~~~~CRGB: %d: %d, %d, %d , %d~~~~~~~~~", crgb_data[0], crgb_data[1], crgb_data[2], crgb_data[3],calibration_mode);
	if (!calibration_mode) {
		/* we're not in calibration mode & we want xyz translation */
		tcs3408_translate_to_xyz(s, crgb_data, xyz_data);
	} else {
		/* normalize the data for atime and again changes */
		for (i = 0; i < CRGB_COUNT; i++)
			crgb_data[i] = normalize_channel_data(s, crgb_data[i]);

		/* calibration mode returns raw data */
		for (i = 0; i < 3; i++)
			xyz_data[i] = crgb_data[i + 1];
	}
}

static int32_t get_lux_from_xyz(struct motion_sensor_t *s, int32_t *xyz_data)
{
	int32_t lux = xyz_data[Y];
	const int32_t offset =
		TCS3408_RGB_DRV_DATA(s + 1)->calibration.rgb_cal[Y].offset;

	/*
	 * Do not include the offset when determining LUX from XYZ.
	 */
	lux = MAX(0, lux - offset);

	return lux;
}

static bool is_spoof(struct motion_sensor_t *s)
{
	return IS_ENABLED(CONFIG_ACCEL_SPOOF_MODE) &&
	       (s->flags & MOTIONSENSE_FLAG_IN_SPOOF_MODE);
}

static int tcs3408_als_auto_gain(struct motion_sensor_t *chip, uint32_t ch0)
{
    uint16_t again = 0;
    uint16_t saturation = 0, again_adjust_high_thr = 0, again_adjust_low_thr = 0;
    int again_adjusted = 1;
    int data[12] = {0};
    uint8_t new_cfg1 = -1;

    tcs3408_i2c_read8(chip, TCS_I2C_ASTAPL, &data[0]); // astep
    tcs3408_i2c_read8(chip, TCS_I2C_ASTAPH, &data[1]);
    tcs3408_i2c_read8(chip, TCS_I2C_ATIME, &data[2]); //atime
    tcs3408_i2c_read8(chip, TCS_I2C_CFG1, &data[3]); // again

    //CPRINTS("data:(%d, %d, %d, %d)\n", data[0], data[1], data[2], data[3]);

    //calculate cpl
    again = tcs3408_als_gains[data[3] & TCS3408_MASK_AGAIN];
    saturation = (data[2] + 1) * (((uint32_t)data[1] << 8 | (uint32_t)data[0]) + 1); //atime * astep
    again_adjust_high_thr = (saturation * 8) / 10;
    again_adjust_low_thr = saturation / 10;

    //CPRINTS("again:%d, saturation:%d, ch0:%d\n", again, saturation, ch0);
    if (ch0 >= again_adjust_high_thr && data[3] > 0) {
        // dec again
        new_cfg1 = (uint8_t)tcs3408_get_als_new_gain(data[3], -1);
    } else if (ch0 <= again_adjust_low_thr && data[3] < 12) {
        // inc again
        new_cfg1 = (uint8_t)tcs3408_get_als_new_gain(data[3], 1);
    } else {
        again_adjusted = 0;
    }

    if (again_adjusted) {
        //CPRINTS("old again:%d, new again:%d\n", data[3], new_cfg1);
        if (new_cfg1 >= 0 && new_cfg1 <= 12) {
            tcs3408_i2c_modify(chip, TCS_I2C_ENABLE, 0x3, 0x1);
            tcs3408_i2c_modify(chip, TCS_I2C_CFG1, TCS3408_MASK_AGAIN, new_cfg1);
            tcs3408_i2c_modify(chip, TCS_I2C_ENABLE, 0x3, 0x3);
        }
    }
	
    return again_adjusted;
}

static int tcs3408_post_events(struct motion_sensor_t *s, uint32_t last_ts,
			       uint32_t status)
{
	/*
	 * Rule says RGB sensor is right after ALS sensor.
	 * This routine will only get called from ALS sensor driver.
	 */
	struct motion_sensor_t *rgb_s = s + 1;
	const uint8_t is_calibration =
		TCS3408_RGB_DRV_DATA(rgb_s)->calibration_mode;
	int als_status = 0;
	int gain_adjusted = 0;
	uint8_t buf[TCS_RGBC_DATA_SIZE]; /* holds raw data read from chip */
	int32_t xyz_data[3] = { 0, 0, 0 };
	uint16_t raw_data[CRGB_COUNT]; /* holds raw CRGB assembled from buf[] */
	int *last_v;
	int32_t lux = 0;
	int ret;

	if (IS_ENABLED(CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT)) {
		int i = 5; /* 100ms max */

		while (i--) {
			/* Make sure data is valid */
			if (status & TCS_I2C_STATUS_RGBC_VALID)
				break;
			msleep(20);
			/*
			 * When not in interrupt mode, we could have scheduled
			 * the handler too early.
			 */
			ret = tcs3408_i2c_read8(s, TCS_I2C_STATUS, &status);
			if (ret)
				return ret;
		}
		if (i < 0) {
			CPRINTS("Status (0x%x)", status);
			return EC_ERROR_UNCHANGED;
		}
	}

	tcs3408_i2c_write8(s, TCS_I2C_STATUS, status);
	/* Read the light registers */
	tcs3408_i2c_read8(s, TCS_I2C_STATUS2 ,&als_status);
	if(als_status & 0x40 )//ALS VALID
	{
		ret = i2c_read_block(s->port, s->i2c_spi_addr_flags,
				TCS_DATA_START_LOCATION, buf, sizeof(buf));
		gain_adjusted = tcs3408_als_auto_gain(s, (uint16_t)(buf[0] | (buf[1] << 8)));
		if(!gain_adjusted)
		{
			#ifndef CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT
				uint16_t high,low;
				if (tcs3408_get_new_thresh(s, &high, &low, (uint16_t)(buf[0] | (buf[1] << 8))) 
				{
                    tcs3408_set_als_thresh(s, high, low);
				}
			#endif
		}
	}
	
	if (ret)
		return ret;
		
	/* Process the raw light data, adjusting for scale and calibration */
	tcs3408_process_raw_data(s, buf, raw_data, xyz_data);

	/* get lux value */
	lux = is_calibration ? xyz_data[Y] : get_lux_from_xyz(s, xyz_data);

	/* if clear channel data changed, send illuminance upstream */
	last_v = s->raw_xyz;
	if (is_calibration ||
	    ((raw_data[CLEAR_CRGB_IDX] != TCS_SATURATION_LEVEL) &&
	     (last_v[X] != lux))) {
		CPRINTS("~~~~~1~~~~~~~");
		if (is_spoof(s))
			last_v[X] = s->spoof_xyz[X];
		else
			last_v[X] = is_calibration ? raw_data[CLEAR_CRGB_IDX] :
						     lux;

		if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
			struct ec_response_motion_sensor_data vector = {
				.flags = 0,
			};

			vector.udata[X] = ec_motion_sensor_clamp_u16(last_v[X]);
			vector.udata[Y] = 0;
			vector.udata[Z] = 0;

			vector.sensor_num = s - motion_sensors;
			motion_sense_fifo_stage_data(&vector, s, 3, last_ts);
		} else {
			CPRINTS("~~~~~2~~~~~~~");
			motion_sense_push_raw_xyz(s);
		}
	}

	/*
	 * If rgb channel data changed since last sample and didn't saturate,
	 * send it upstream
	 */
	last_v = rgb_s->raw_xyz;
	if (is_calibration ||
	    (((last_v[X] != xyz_data[X]) || (last_v[Y] != xyz_data[Y]) ||
	      (last_v[Z] != xyz_data[Z])) &&
	     ((raw_data[RED_CRGB_IDX] != TCS_SATURATION_LEVEL) &&
	      (raw_data[BLUE_CRGB_IDX] != TCS_SATURATION_LEVEL) &&
	      (raw_data[GREEN_CRGB_IDX] != TCS_SATURATION_LEVEL)))) {
		CPRINTS("~~~~~3~~~~~~~");
		if (is_spoof(rgb_s)) {
			memcpy(last_v, rgb_s->spoof_xyz,
			       sizeof(rgb_s->spoof_xyz));
		} else if (is_calibration) {
			last_v[0] = raw_data[RED_CRGB_IDX];
			last_v[1] = raw_data[GREEN_CRGB_IDX];
			last_v[2] = raw_data[BLUE_CRGB_IDX];
		} else {
			memcpy(last_v, xyz_data, sizeof(xyz_data));
		}

		if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
			CPRINTS("~~~~~4~~~~~~~");
			struct ec_response_motion_sensor_data vector = {
				.flags = 0,
			};
			void *udata = vector.udata;

			ec_motion_sensor_clamp_u16s(udata, last_v);

			vector.sensor_num = rgb_s - motion_sensors;
			motion_sense_fifo_stage_data(&vector, rgb_s, 3,
						     last_ts);
		} else {
			CPRINTS("~~~~~5~~~~~~~");
			motion_sense_push_raw_xyz(rgb_s);
		}
	}
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
	{
		CPRINTS("~~~~~6~~~~~~~");
		motion_sense_fifo_commit_data();
	}
		

	if (!is_calibration)
	{
		ret = tcs3408_adjust_sensor_for_saturation(s, xyz_data[Y],
							   raw_data, status);
	}
		

	return ret;
}

/*
 * tcs3408_irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt,
 * and posts those events via motion_sense_fifo_stage_data()..
 *
 * This routine will get called for the TCS3408 ALS driver, but NOT for the
 * RGB driver.  We harvest data for both drivers in this routine.  The RGB
 * driver is guaranteed to directly follow the ALS driver in the sensor list
 * (i.e rgb's motion_sensor_t structure can be found at (s+1) ).
 */
static int tcs3408_irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	uint32_t status = 0;
	int ret;

	if (!(*event & CONFIG_ALS_TCS3408_INT_EVENT))
		return EC_ERROR_NOT_HANDLED;

	/* Disable future interrupts */
	// ret = tcs3408_i2c_write8(s, TCS_I2C_ENABLE, TCS3408_MODE_IDLE);
	// if (ret)
	// 	return ret;
	
	if (IS_ENABLED(CONFIG_ALS_TCS3408_EMULATED_IRQ_EVENT)) {
		ret = tcs3408_post_events(s, last_interrupt_timestamp, status);
		if (ret)
			return ret;
	}

	// tcs3408_i2c_write8(s, TCS_I2C_CFG3, 0x08);

	/* Disable ADC and turn off internal oscillator */
	// ret = tcs3408_i2c_write8(s, TCS_I2C_ENABLE, TCS3408_MODE_SUSPEND);
	// if (ret)
	// 	return ret;

	return EC_SUCCESS;
}

static int tcs3408_rgb_get_scale(const struct motion_sensor_t *s,
				 uint16_t *scale, int16_t *temp)
{
	struct rgb_channel_calibration_t *rgb_cal =
		TCS3408_RGB_DRV_DATA(s)->calibration.rgb_cal;

	scale[X] = rgb_cal[RED_RGB_IDX].scale.k_channel_scale;
	scale[Y] = rgb_cal[GREEN_RGB_IDX].scale.k_channel_scale;
	scale[Z] = rgb_cal[BLUE_RGB_IDX].scale.k_channel_scale;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int tcs3408_rgb_set_scale(const struct motion_sensor_t *s,
				 const uint16_t *scale, int16_t temp)
{
	struct rgb_channel_calibration_t *rgb_cal =
		TCS3408_RGB_DRV_DATA(s)->calibration.rgb_cal;

	if (scale[X] == 0 || scale[Y] == 0 || scale[Z] == 0|| scale[3] ==0|| scale[4] ==0)
		return EC_ERROR_INVAL;
	rgb_cal[RED_RGB_IDX].scale.k_channel_scale = scale[X];
	rgb_cal[GREEN_RGB_IDX].scale.k_channel_scale = scale[Y];
	rgb_cal[BLUE_RGB_IDX].scale.k_channel_scale = scale[Z];
	rgb_cal[CLEAR_IDX].scale.k_channel_scale = scale[3];
	rgb_cal[WIDEBAND_IDX].scale.k_channel_scale = scale[4];
	return EC_SUCCESS;
}

static int tcs3408_rgb_get_offset(const struct motion_sensor_t *s,
				  int16_t *offset, int16_t *temp)
{
	offset[X] = TCS3408_RGB_DRV_DATA(s)->calibration.rgb_cal[X].offset;
	offset[Y] = TCS3408_RGB_DRV_DATA(s)->calibration.rgb_cal[Y].offset;
	offset[Z] = TCS3408_RGB_DRV_DATA(s)->calibration.rgb_cal[Z].offset;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int tcs3408_rgb_set_offset(const struct motion_sensor_t *s,
				  const int16_t *offset, int16_t temp)
{
	/* do not allow offset to be changed, it's predetermined */
	return EC_SUCCESS;
}

static int tcs3408_rgb_set_data_rate(const struct motion_sensor_t *s, int rate,
				     int rnd)
{
	return EC_SUCCESS;
}

/* Enable/disable special factory calibration mode */
static int tcs3408_perform_calib(struct motion_sensor_t *s, int enable)
{
	TCS3408_RGB_DRV_DATA(s + 1)->calibration_mode = enable;
	return EC_SUCCESS;
}

static int tcs3408_rgb_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	return EC_SUCCESS;
}

static int tcs3408_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	TCS3408_DRV_DATA(s)->als_cal.scale = range >> 16;
	TCS3408_DRV_DATA(s)->als_cal.uscale = range & 0xffff;
	s->current_range = range;
	return EC_SUCCESS;
}

static int tcs3408_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
			     int16_t *temp)
{
	scale[X] = TCS3408_DRV_DATA(s)->als_cal.channel_scale.k_channel_scale;
	scale[Y] = 0;
	scale[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int tcs3408_set_scale(const struct motion_sensor_t *s,
			     const uint16_t *scale, int16_t temp)
{
	if (scale[X] == 0)
		return EC_ERROR_INVAL;
	TCS3408_DRV_DATA(s)->als_cal.channel_scale.k_channel_scale = scale[X];
	return EC_SUCCESS;
}

static int tcs3408_get_offset(const struct motion_sensor_t *s, int16_t *offset,
			      int16_t *temp)
{
	offset[X] = TCS3408_DRV_DATA(s)->als_cal.offset;
	offset[Y] = 0;
	offset[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int tcs3408_set_offset(const struct motion_sensor_t *s,
			      const int16_t *offset, int16_t temp)
{
	/* do not allow offset to be changed, it's predetermined */
	return EC_SUCCESS;
}

static int tcs3408_get_data_rate(const struct motion_sensor_t *s)
{
	return TCS3408_DRV_DATA(s)->rate;
}

static int tcs3408_rgb_get_data_rate(const struct motion_sensor_t *s)
{
	return tcs3408_get_data_rate(s - 1);
}

static int tcs3408_set_data_rate(const struct motion_sensor_t *s, int rate,
				 int rnd)
{
	enum tcs3408_mode mode;
	int data;
	int ret;

	if (rate == 0) {
		/* Suspend driver */
		mode = TCS3408_MODE_SUSPEND;
	} else {
		/*
		 * We set the sensor for continuous mode,
		 * integrating over 800ms.
		 * Do not allow range higher than 1Hz.
		 */
		if (rate > TCS3408_LIGHT_MAX_FREQ)
			rate = TCS3408_LIGHT_MAX_FREQ;
		mode = TCS3408_MODE_COLLECTING;
	}
	TCS3408_DRV_DATA(s)->rate = rate;

	ret = tcs3408_i2c_read8(s, TCS_I2C_ENABLE, &data);
	if (ret)
		return ret;

	data = (data & TCS_I2C_ENABLE_MASK) | mode;
	ret = tcs3408_i2c_write8(s, TCS_I2C_ENABLE, data);
	if (ret)
		return ret;

	ret = tcs3408_i2c_read8(s, TCS_I2C_ENABLE, &data);
	return ret;
}

/**
 * Initialise TCS3408 light sensor.
 */
static int tcs3408_rgb_init(struct motion_sensor_t *s)
{
	uint16_t scale[RGB_CHANNEL_COUNT] = {
		ALS_CAL_COEF_SCALE, 
		ALS_CAL_COEF_SCALE, 
		ALS_CAL_COEF_SCALE, 
		ALS_CAL_COEF_SCALE,
		ALS_CAL_COEF_SCALE };	
	tcs3408_rgb_set_scale(s, scale,0);
	return EC_SUCCESS;
}

static int tcs3408_init(struct motion_sensor_t *s)
{
	/*
	 * These are default power-on register values with two exceptions:
	 * Set ATIME = 0 (712 ms)
	 * Set AGAIN = 16 (0x10)  (AGAIN is in CONTROL register)
	 */
	const struct reg_data {
		uint8_t reg;
		uint8_t data;
	} defaults[] = {
		{ TCS_I2C_ENABLE, 0 },
		{ TCS_I2C_ENABLE, 1 },	  
		{ TCS_I2C_ATIME, ATIME_MS(100)},
		{ TCS_I2C_WTIME, 0 }, 
		{ TCS_I2C_CFG8, 0x80 },
		{ TCS_I2C_PERS, 0x01 },  
		{ TCS_I2C_ASTAPL, 0xE7 },
		{ TCS_I2C_ASTAPH, 0x03 },	  
		{ TCS_I2C_CFG1,  0x09 << 0},
		{ TCS_I2C_INTENAB, 0 },
	};
	int data = 0;
	int ret;
	uint16_t scale[5] = {1,1,1,1,1};

	ret = tcs3408_i2c_read8(s, TCS_I2C_ID, &data);
	CPRINTS("~~~~~~~~WGX : %s~~~~~~~~~",__func__);
	if (ret) {
		CPRINTS("failed reading ID reg 0x%x, ret=%d", TCS_I2C_ID, ret);
		return ret;
	}
	/*
	 * if ((data != TCS340015_DEVICE_ID) && (data != TCS340037_DEVICE_ID)) {
	 *	CPRINTS("no ID match, data = 0x%x", data);
	 *	return EC_ERROR_ACCESS_DENIED;
	 *}
	 */

	/* reset chip to default power-on settings, changes ATIME and CONTROL */
	for (int x = 0; x < ARRAY_SIZE(defaults); x++) {
		ret = tcs3408_i2c_write8(s, defaults[x].reg, defaults[x].data);
		if (x == 0)
			udelay(300);
		if (ret)
			return ret;
	}

	s->deltaP = 10;
	tcs3408_set_als_thresh(s, 0x0000, 0xFFFF);
	tcs3408_rgb_set_scale(s,scale,0);
	return sensor_init_done(s);
}

const struct accelgyro_drv tcs3408_drv = {
	.init = tcs3408_init,
	.read = tcs3408_read,
	.set_range = tcs3408_set_range,
	.set_offset = tcs3408_set_offset,
	.get_offset = tcs3408_get_offset,
	.set_scale = tcs3408_set_scale,
	.get_scale = tcs3408_get_scale,
	.set_data_rate = tcs3408_set_data_rate,
	.get_data_rate = tcs3408_get_data_rate,
	.perform_calib = tcs3408_perform_calib,
#ifdef ALS_TCS3408_INT_ENABLE
	.irq_handler = tcs3408_irq_handler,
#endif
};

const struct accelgyro_drv tcs3408_rgb_drv = {
	.init = tcs3408_rgb_init,
	.read = tcs3408_rgb_read,
	.set_range = tcs3408_rgb_set_range,
	.set_offset = tcs3408_rgb_set_offset,
	.get_offset = tcs3408_rgb_get_offset,
	.set_scale = tcs3408_rgb_set_scale,
	.get_scale = tcs3408_rgb_get_scale,
	.set_data_rate = tcs3408_rgb_set_data_rate,
	.get_data_rate = tcs3408_rgb_get_data_rate,
};
