/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LIS2DW12 accelerometer module for Chrome EC 3D digital accelerometer */
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accel_lis2dw12.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "util.h"

#ifdef CONFIG_ACCEL_FIFO
/**
 * lis2dw12_enable_fifo - Enable/Disable FIFO in LIS2DW12
 * @s: Motion sensor pointer
 * @mode: fifo_modes
 */
static int lis2dw12_enable_fifo(const struct motion_sensor_t *s,
				enum lis2dw12_fmode mode)
{
	return st_write_data_with_mask(s, LIS2DW12_FIFO_CTRL_ADDR,
				       LIS2DW12_FIFO_MODE_MASK, mode);
}

/**
 * Load data from internal sensor FIFO.
 * @s: Motion sensor pointer
 */
static int lis2dw12_load_fifo(struct motion_sensor_t *s)
{
	int ret, tmp, nsamples, i;
	struct ec_response_motion_sensor_data vect;
	int done = 0;
	int *axis = s->raw_xyz;
	uint8_t fifo[FIFO_READ_LEN];

	do {
		ret = raw_read8(s->port, s->addr, LIS2DW12_FIFO_SAMPLES_ADDR,
				&tmp);
		if (ret != EC_SUCCESS)
			return ret;

		nsamples = tmp & LIS2DW12_FIFO_DIFF_MASK;
		if (nsamples == 0)
			return EC_SUCCESS;

		/* Each sample are OUT_XYZ_SIZE byte. */
		nsamples = nsamples * OUT_XYZ_SIZE;

		/*
		 * Limit FIFO read data to burst of FIFO_READ_LEN size because
		 * read operatios in under i2c mutex lock.
		 */
		if (nsamples > FIFO_READ_LEN)
			nsamples = FIFO_READ_LEN;
		else
			done = 1;

		ret = st_raw_read_n(s->port, s->addr, LIS2DW12_OUT_X_L_ADDR,
				    fifo, nsamples);
		if (ret != EC_SUCCESS)
			return ret;

		for (i = 0; i < nsamples; i += OUT_XYZ_SIZE) {
			/* Apply precision, sensitivity and rotation vector. */
			st_normalize(s, axis, &fifo[i]);

			/* Fill vector array. */
			vect.data[0] = axis[0];
			vect.data[1] = axis[1];
			vect.data[2] = axis[2];
			vect.flags = 0;
			vect.sensor_num = 0;
			motion_sense_fifo_add_unit(&vect, s, 3);
		}
	} while(!done);

	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_FIFO */

#ifdef CONFIG_ACCEL_INTERRUPTS

/**
 * lis2dw12_config_interrupt- Configure interrupt for supported features.
 * @s: Motion sensor pointer
 *
 * Must works with interface mutex locked
 */
static int lis2dw12_config_interrupt(const struct motion_sensor_t *s)
{
	int ret = EC_SUCCESS;

#ifdef CONFIG_ACCEL_FIFO_THRES
	/* Configure FIFO watermark level. */
	ret = st_write_data_with_mask(s, LIS2DW12_FIFO_CTRL_ADDR,
				      LIS2DW12_FIFO_THRESHOLD_MASK,
				      CONFIG_ACCEL_FIFO_THRES);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable interrupt on FIFO watermask and route to int1. */
	ret = st_write_data_with_mask(s, LIS2DW12_INT1_FTH_ADDR,
				      LIS2DW12_INT1_FTH_MASK, LIS2DW12_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;
#endif /* CONFIG_ACCEL_FIFO */

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	/* Configure D-TAP event detection on 3 axis. */
	ret = raw_write8(s->port, s->addr, LIS2DW12_TAP_THS_X_ADDR, 0x09);
	if (ret != EC_SUCCESS)
		return ret;
	ret = raw_write8(s->port, s->addr, LIS2DW12_TAP_THS_Y_ADDR, 0x09);
	if (ret != EC_SUCCESS)
		return ret;
	ret = raw_write8(s->port, s->addr, LIS2DW12_TAP_THS_Z_ADDR, 0xE9);
	if (ret != EC_SUCCESS)
		return ret;
	ret = raw_write8(s->port, s->addr, LIS2DW12_INT_DUR_ADDR, 0x7F);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable D-TAP event detection. */
	ret = st_write_data_with_mask(s, LIS2DW12_WAKE_UP_THS_ADDR,
				      LIS2DW12_SINGLE_DOUBLE_TAP,
				      LIS2DW12_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * Enable D-TAP detection on int_1 pad. In any case D-TAP event
	 * can be detected only if ODR is over 200 Hz.
	 */
	ret = st_write_data_with_mask(s, LIS2DW12_INT1_TAP_ADDR,
				      LIS2DW12_INT1_DTAP_MASK,
				      LIS2DW12_EN_BIT);
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */
	return ret;
}

/**
 * lis2dw12_interrupt - interrupt from int1 pin of sensor
 * Schedule Motion Sense Task to manage Interrupts.
 */
void lis2dw12_interrupt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCEL_LIS2DW12_INT_EVENT, 0);
}

/**
 * lis2dw12_irq_handler - bottom half of the interrupt stack.
 */
static int lis2dw12_irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
	    (!(*event & CONFIG_ACCEL_LIS2DW12_INT_EVENT))) {
		return EC_ERROR_NOT_HANDLED;
	}

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	{
		int status = 0;

		/* Read Status register to check TAP events. */
		raw_read8(s->port, s->addr, LIS2DW12_STATUS_TAP, &status);
		if (status & LIS2DW12_DOUBLE_TAP)
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
 * set_power_mode - set sensor power mode
 * @s: Motion sensor pointer
 * @mode: LIS2DW12_LOW_POWER, LIS2DW12_HIGH_PERF
 * @lpmode:  LIS2DW12_LOW_POWER_MODE_2, LIS2DW12_LOW_POWER_MODE_3,
 *           LIS2DW12_LOW_POWER_MODE_4
 *
 * TODO: (LIS2DW12_LOW_POWER_MODE_1 not implemented because differ in resol.)
 */
static int set_power_mode(const struct motion_sensor_t *s,
			  enum lis2sw12_mode mode,
			  enum lis2sw12_lpmode lpmode)
{
	int ret = EC_SUCCESS;

	if (mode == LIS2DW12_LOW_POWER &&
	    lpmode == LIS2DW12_LOW_POWER_MODE_1)
		return EC_ERROR_UNIMPLEMENTED;

	/* Set Mode and Low Power Mode. */
	ret = st_write_data_with_mask(s, LIS2DW12_ACC_MODE_ADDR,
				      LIS2DW12_ACC_MODE_MASK,
				      mode);
	if (ret != EC_SUCCESS)
		return ret;

	ret = st_write_data_with_mask(s, LIS2DW12_ACC_LPMODE_ADDR,
				      LIS2DW12_ACC_LPMODE_MASK,
				      lpmode);
	return ret;
}

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 */
static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	int err = EC_SUCCESS;
	uint8_t reg_val;
	struct stprivate_data *data = s->drv_data;
	int newrange = range;

	/* Adjust and check rounded value. */
	if (rnd && (newrange < LIS2DW12_NORMALIZE_FS(newrange)))
		newrange <<= 1;

	if (newrange > LIS2DW12_ACCEL_FS_MAX_VAL)
		newrange = LIS2DW12_ACCEL_FS_MAX_VAL;

	reg_val = LIS2DW12_FS_REG(newrange);

	mutex_lock(s->mutex);
#ifdef CONFIG_ACCEL_FIFO
	/*
	 * FIFO stop collecting events. Restart FIFO in Bypass mode.
	 * If Range is changed all samples in FIFO must be discharged because
	 * with a different sensitivity.
	 */
	err = lis2dw12_enable_fifo(s, LIS2DW12_FIFO_BYPASS_MODE);
	if (err != EC_SUCCESS)
		goto unlock_rate;
#endif /* CONFIG_ACCEL_FIFO */

	err = st_write_data_with_mask(s, LIS2DW12_FS_ADDR, LIS2DW12_FS_MASK,
				      reg_val);
	if (err == EC_SUCCESS)
		/* Save internally gain for speed optimization. */
		data->base.range = LIS2DW12_FS_GAIN(newrange);
#ifdef CONFIG_ACCEL_FIFO
	/* FIFO restart collecting events in Cont. mode. */
	err = lis2dw12_enable_fifo(s, LIS2DW12_FIFO_CONT_MODE);
#endif /* CONFIG_ACCEL_FIFO */
unlock_rate:
	mutex_unlock(s->mutex);

	return err;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	return LIS2DW12_GAIN_FS(data->base.range);
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, normalized_rate;
	struct stprivate_data *data = s->drv_data;
	uint8_t reg_val;

	mutex_lock(s->mutex);

#ifdef CONFIG_ACCEL_FIFO
	/* FIFO stop collecting events. Restart FIFO in Bypass mode. */
	ret = lis2dw12_enable_fifo(s, LIS2DW12_FIFO_BYPASS_MODE);
	if (ret != EC_SUCCESS)
		goto unlock_rate;
#endif /* CONFIG_ACCEL_FIFO */

	if (rate == 0) {
		ret = st_write_data_with_mask(s, LIS2DW12_ACC_ODR_ADDR,
					      LIS2DW12_ACC_ODR_MASK,
					      LIS2DW12_ODR_POWER_OFF_VAL);
		if (ret == EC_SUCCESS)
			data->base.odr = LIS2DW12_ODR_POWER_OFF_VAL;

		goto unlock_rate;
	}

	reg_val = LIS2DW12_ODR_TO_REG(rate);
	normalized_rate = LIS2DW12_ODR_TO_NORMALIZE(rate);

	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate <<= 1;
	}

	/* Adjust rounded value for acc and gyro because ODR are shared. */
	if (reg_val > LIS2DW12_ODR_1_6kHZ_VAL) {
		reg_val = LIS2DW12_ODR_1_6kHZ_VAL;
		normalized_rate = LIS2DW12_ODR_MAX_VAL;
	} else if (reg_val < LIS2DW12_ODR_12HZ_VAL) {
		reg_val = LIS2DW12_ODR_12HZ_VAL;
		normalized_rate = LIS2DW12_ODR_MIN_VAL;
	}
	if (reg_val > LIS2DW12_ODR_200HZ_VAL)
		ret = set_power_mode(s, LIS2DW12_HIGH_PERF, 0);
	else
		ret = set_power_mode(s, LIS2DW12_LOW_POWER,
				     LIS2DW12_LOW_POWER_MODE_2);

	ret = st_write_data_with_mask(s, LIS2DW12_ACC_ODR_ADDR,
				      LIS2DW12_ACC_ODR_MASK, reg_val);
	if (ret == EC_SUCCESS)
		data->base.odr = normalized_rate;

#ifdef CONFIG_ACCEL_FIFO
	/* FIFO restart collecting events in Cont. mode. */
	ret = lis2dw12_enable_fifo(s, LIS2DW12_FIFO_CONT_MODE);
#endif /* CONFIG_ACCEL_FIFO */

unlock_rate:
	mutex_unlock(s->mutex);

	return ret;
}

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->port, s->addr, LIS2DW12_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	*ready = (LIS2DW12_STS_DRDY_UP == (tmp & LIS2DW12_STS_DRDY_UP));

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

	/* Read 6 bytes starting at xyz_reg. */
	ret = st_raw_read_n_noinc(s->port, s->addr, LIS2DW12_OUT_X_L_ADDR, raw,
				  LIS2DW12_OUT_XYZ_SIZE);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error]", s->name, s->type);
		return ret;
	}

	/* Transform from LSB to real data with rotation and gain. */
	st_normalize(s, v, raw);

	/* Apply offset in the device coordinates. */
	for (i = X; i <= Z; i++)
		v[i] += ((data->offset[i] << 5) / data->base.range);

	return EC_SUCCESS;
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp, timeout = 0, status;
	struct stprivate_data *data = s->drv_data;

	ret = raw_read8(s->port, s->addr, LIS2DW12_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LIS2DW12_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 */
	mutex_lock(s->mutex);
	ret = raw_write8(s->port, s->addr, LIS2DW12_SOFT_RESET_ADDR,
			 LIS2DW12_SOFT_RESET_MASK);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Check End of Reset. */
	do {
		if (timeout > 100) {
			ret = EC_RES_TIMEOUT;
			goto err_unlock;
		}

		msleep(10);
		timeout += 10;
		ret = raw_read8(s->port, s->addr, LIS2DW12_SOFT_RESET_ADDR,
				&status);
		if (ret != EC_SUCCESS)
			continue;
	} while ((status & LIS2DW12_SOFT_RESET_MASK) != 0);

	/* Enable BDU. */
	ret = st_write_data_with_mask(s, LIS2DW12_BDU_ADDR, LIS2DW12_BDU_MASK,
				      LIS2DW12_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = st_write_data_with_mask(s, LIS2DW12_LIR_ADDR, LIS2DW12_LIR_MASK,
				      LIS2DW12_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	/* Set Mode and Low Power Mode. */
	ret = set_power_mode(s, LIS2DW12_LOW_POWER, LIS2DW12_LOW_POWER_MODE_2);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	mutex_unlock(s->mutex);

	/* Config initial Acc Range. */
	ret = set_range(s, s->default_range, 0);
	if (ret != EC_SUCCESS)
		return ret;

	/* Set default resolution. */
	data->resol = LIS2DW12_RESOLUTION;

#ifdef CONFIG_ACCEL_INTERRUPTS
	ret = lis2dw12_config_interrupt(s);
#endif /* CONFIG_ACCEL_INTERRUPTS */

	sensor_init_done(s, get_range(s));

	return ret;

err_unlock:
	mutex_unlock(s->mutex);
	CPRINTF("[%T %s: MS Init type:0x%X Error]\n", s->name, s->type);

	return EC_ERROR_UNKNOWN;
}

const struct accelgyro_drv lis2dw12_drv = {
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
	.load_fifo = lis2dw12_load_fifo,
#endif /* CONFIG_ACCEL_FIFO */
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = lis2dw12_irq_handler,
#endif /* CONFIG_ACCEL_INTERRUPTS */
};
