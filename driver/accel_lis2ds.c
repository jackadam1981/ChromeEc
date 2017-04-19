/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LIS2DS accelerometer module for Chrome EC 3D digital accelerometer
 */
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "util.h"
#include "driver/accel_lis2ds.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

#ifdef CONFIG_ACCEL_FIFO
/**
 * lis2ds_enable_fifo - Enable/Disable FIFO in LIS2DS12
 * @s: Motion sensor pointer
 * @mode: fifo_modes
 */
static int lis2ds_enable_fifo(const struct motion_sensor_t *s, int mode)
{
	return st_write_data_with_mask(s, LIS2DS_FIFO_CTRL_ADDR,
				       LIS2DS_FIFO_MODE_MASK, mode);
}

/*
 * Load data from internal sensor FIFO
 * DIFF8 bits set means FIFO Full because 256 samples -> 0x100
 */
static int lis2ds_load_fifo(struct motion_sensor_t *s)
{
	int ret, tmp, nsamples, i;
	struct ec_response_motion_sensor_data vect;
	int done = 0;
	int *axis = s->raw_xyz;
	uint8_t fifo[FIFO_READ_LEN];

	do {
		/* FTH FIFO OVR DIFF8 bits */
		ret = raw_read8(s->port, s->addr, LIS2DS_FIFO_SRC_ADDR, &tmp);
		if (ret != EC_SUCCESS)
			return ret;

		/* Check if FIFO is full. */
		if (tmp & LIS2DS_FIFO_DIFF8_MASK) {
			nsamples = 0x100;
		} else {
			/* Unread FIFO samples */
			ret = raw_read8(s->port, s->addr,
					LIS2DS_FIFO_SAMPLES_ADDR, &tmp);
			if (ret != EC_SUCCESS)
				return ret;

			nsamples = tmp;
		}

		/* Check FIFO empty. */
		if (nsamples == 0)
			return EC_SUCCESS;

		/* Each sample are OUT_XYZ_SIZE bytes. */
		nsamples = nsamples * OUT_XYZ_SIZE;

		/*
		 * Limit FIFO read data to burst of FIFO_READ_LEN size because
		 * read operatios in under i2c mutex lock.
		 */
		if (nsamples > FIFO_READ_LEN)
			nsamples = FIFO_READ_LEN;
		else
			done = 1;

		ret = st_raw_read_n(s->port, s->addr, LIS2DS_OUT_X_L_ADDR, fifo,
				    nsamples);
		if (ret != EC_SUCCESS)
			return ret;

		for (i = 0; i < nsamples; i += OUT_XYZ_SIZE) {
			/* Apply precision, sensitivity and rotation vector */
			st_normalize(s, axis, &fifo[i]);

			/* Fill vector array */
			vect.data[0] = axis[0];
			vect.data[1] = axis[1];
			vect.data[2] = axis[2];
			vect.flags = 0;
			vect.sensor_num = 0;
			motion_sense_fifo_add_unit(&vect, s, 3);
		}
	} while (!done);

	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_FIFO */

#ifdef CONFIG_ACCEL_INTERRUPTS

static int lis2ds_config_interrupt(const struct motion_sensor_t *s)
{
	int ret = EC_SUCCESS;

#ifdef CONFIG_ACCEL_FIFO_THRES
	/* Configure FIFO watermark level. */
	ret = raw_write8(s->port, s->addr, LIS2DS_FIFO_THS_ADDR,
			 CONFIG_ACCEL_FIFO_THRES);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable interrupt on FIFO watermask and route to int1. */
	ret = st_write_data_with_mask(s, LIS2DS_CTRL4_ADDR,
				      LIS2DS_INT1_FTH, LIS2DS_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;
#endif /* CONFIG_ACCEL_FIFO */

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	/* Enable tap detection. */
	ret = st_write_data_with_mask(s, LIS2DS_CTRL3_ADDR,
				      LIS2DS_TAP_EN_MASK, LIS2DS_TAP_EN_ALL);
	if (ret != EC_SUCCESS)
		return ret;

	/* Configure tap detection as suggest AN. */
	ret = raw_write8(s->port, s->addr, LIS2DS_TAP_6D_THS_ADDR, 0x0C);
	if (ret != EC_SUCCESS)
		return ret;

	ret = raw_write8(s->port, s->addr, LIS2DS_INT_DUR_ADDR, 0x7f);
	if (ret != EC_SUCCESS)
		return ret;

	ret = raw_write8(s->port, s->addr, LIS2DS_WAKE_UP_THS_ADDR, 0x80);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable tap event on int1. */
	ret = st_write_data_with_mask(s, LIS2DS_CTRL4_ADDR,
				      LIS2DS_INT1_D_TAP, LIS2DS_EN_BIT);
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */

	return ret;
}

/**
 * lis2ds_interrupt - interrupt from int1 pin of sensor
 * Schedule Motion Sense Task to manage Interrupts
 */
void lis2ds_interrupt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCEL_LIS2DS_INT_EVENT, 0);
}

/**
 * lis2ds_irq_handler - bottom half of the interrupt stack.
 */
static int lis2ds_irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
	    (!(*event & CONFIG_ACCEL_LIS2DS_INT_EVENT))) {
		return EC_ERROR_NOT_HANDLED;
	}

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	{
		int status = 0;

		/* Read Status register to check TAP events. */
		raw_read8(s->port, s->addr, LIS2DS_TAP_SRC_ADDR, &status);
		if (status & LIS2DS_TAP_EVENT_DETECT)
			*event |= CONFIG_GESTURE_TAP_EVENT;
	}
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */

	/*
	 * No need to read the FIFO here, motion sense task is
	 * doing it on every interrupt.
	 */
	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_INTERRUPTS */

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 */
static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	int err;
	uint8_t reg_val;
	struct stprivate_data *data = s->drv_data;
	int newrange = ST_NORMALIZE_RATE(range);

	/* Adjust and check rounded value */
	if (rnd && (newrange < range))
		newrange <<= 1;

	if (newrange > LIS2DS_ACCEL_FS_MAX_VAL)
		newrange = LIS2DS_ACCEL_FS_MAX_VAL;

	reg_val = LIS2DS_FS_REG(newrange);

	mutex_lock(s->mutex);
	err = st_write_data_with_mask(s, LIS2DS_FS_ADDR, LIS2DS_FS_MASK,
				      reg_val);
	if (err == EC_SUCCESS)
		/* Save internally gain for speed optimization. */
		data->base.range = newrange;
	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	return data->base.range;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, normalized_rate;
	struct stprivate_data *data = s->drv_data;
	uint8_t reg_val;

	mutex_lock(s->mutex);

#ifdef CONFIG_ACCEL_FIFO
	/* FIFO stop collecting events. Restart FIFO in Bypass mode */
	ret = lis2ds_enable_fifo(s, LIS2DS_FIFO_BYPASS_MODE);
	if (ret != EC_SUCCESS)
		goto unlock_rate;
#endif /* CONFIG_ACCEL_FIFO */

	if (rate == 0) {
		ret = st_write_data_with_mask(s, LIS2DS_ACC_ODR_ADDR,
					      LIS2DS_ACC_ODR_MASK,
					      LIS2DS_ODR_POWER_OFF_VAL);
		if (ret == EC_SUCCESS)
			data->base.odr = LIS2DS_ODR_POWER_OFF_VAL;

		goto unlock_rate;
	}

	reg_val = LIS2DS_ODR_TO_REG(rate);
	normalized_rate = LIS2DS_ODR_TO_NORMALIZE(rate);

	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate <<= 1;
	}

	/* Adjust rounded value for acc and gyro because ODR are shared. */
	if (reg_val > LIS2DS_ODR_800HZ_VAL) {
		reg_val = LIS2DS_ODR_800HZ_VAL;
		normalized_rate = LIS2DS_ODR_MAX_VAL;
	} else if (reg_val < LIS2DS_ODR_12HZ_VAL) {
		reg_val = LIS2DS_ODR_12HZ_VAL;
		normalized_rate = LIS2DS_ODR_MIN_VAL;
	}

	ret = st_write_data_with_mask(s, LIS2DS_ACC_ODR_ADDR,
				      LIS2DS_ACC_ODR_MASK, reg_val);
	if (ret == EC_SUCCESS)
		data->base.odr = normalized_rate;

#ifdef CONFIG_ACCEL_FIFO
	/* FIFO restart collecting events in Cont. mode. */
	ret = lis2ds_enable_fifo(s, LIS2DS_FIFO_CONT_MODE);
#endif /* CONFIG_ACCEL_FIFO */

unlock_rate:
	mutex_unlock(s->mutex);

	return ret;
}

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->port, s->addr, LIS2DS_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	*ready = (LIS2DS_STS_XLDA_UP == (tmp & LIS2DS_STS_XLDA_UP));

	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t raw[OUT_XYZ_SIZE];
	int ret, i, tmp = 0;
	struct stprivate_data *data = s->drv_data;

	ret = is_data_ready(s, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion senor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (!tmp) {
		if (v != s->raw_xyz)
			memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
		return EC_SUCCESS;
	}

	/* Read 6 bytes starting at xyz_reg */
	ret = st_raw_read_n_noinc(s->port, s->addr, LIS2DS_OUT_X_L_ADDR, raw,
				  LIS2DS_OUT_XYZ_SIZE);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error]", s->name, s->type);
		return ret;
	}

	/* Transform from LSB to real data with rotation and gain */
	st_normalize(s, v, raw);

	return EC_SUCCESS;
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp, timeout = 0, status;
	struct stprivate_data *data = s->drv_data;

	ret = raw_read8(s->port, s->addr, LIS2DS_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LIS2DS_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 */
	mutex_lock(s->mutex);

	ret = raw_write8(s->port, s->addr, LIS2DS_SOFT_RESET_ADDR,
			 LIS2DS_SOFT_RESET_MASK);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Check End of Reset */
	do {
		if (timeout > 100) {
			ret = EC_RES_TIMEOUT;
			goto err_unlock;
		}

		msleep(10);
		timeout += 10;
		ret = raw_read8(s->port, s->addr, LIS2DS_SOFT_RESET_ADDR,
				&status);
		if (ret != EC_SUCCESS)
			continue;
	} while ((status & LIS2DS_SOFT_RESET_MASK) != 0);

	/* Enable BDU */
	ret = st_write_data_with_mask(s, LIS2DS_BDU_ADDR, LIS2DS_BDU_MASK,
				      LIS2DS_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = st_write_data_with_mask(s, LIS2DS_LIR_ADDR, LIS2DS_LIR_MASK,
				      LIS2DS_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = st_write_data_with_mask(s, LIS2DS_INT2_ON_INT1_ADDR,
				      LIS2DS_INT2_ON_INT1_MASK, LIS2DS_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	mutex_unlock(s->mutex);

	/* Set default resolution */
	data->resol = LIS2DS_RESOLUTION;

#ifdef CONFIG_ACCEL_INTERRUPTS
	ret = lis2ds_config_interrupt(s);
#endif /* CONFIG_ACCEL_INTERRUPTS */

	return sensor_init_done(s);

err_unlock:
	mutex_unlock(s->mutex);
	CPRINTF("[%T %s: MS Init type:0x%X Error]\n", s->name, s->type);

	return EC_ERROR_UNKNOWN;
}

const struct accelgyro_drv lis2ds_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = st_set_resolution,
	.get_resolution = st_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = st_get_data_rate,
	.set_offset = st_set_offset,
	.get_offset = st_get_offset,
	.perform_calib = NULL,
#ifdef CONFIG_ACCEL_FIFO
	.load_fifo = lis2ds_load_fifo,
#endif /* CONFIG_ACCEL_FIFO */
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = lis2ds_irq_handler,
#endif /* CONFIG_ACCEL_INTERRUPTS */
};
