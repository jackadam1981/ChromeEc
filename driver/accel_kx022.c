/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* KX022 Accelerometer driver for Chrome EC */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accel_kx022.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* Number of times to attempt to enable sensor before giving up. */
#define SENSOR_ENABLE_ATTEMPTS 3

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct accel_param_pair {
	int val; /* Value in engineering units. */
	int reg; /* Corresponding register value. */
};

/* List of range values in +/-G's and their associated register values. */
static const struct accel_param_pair ranges[] = {
	{2, KX022_GSEL_2G},
	{4, KX022_GSEL_4G},
	{8, KX022_GSEL_8G}
};

/* List of resolution values in bits and their associated register values. */
static const struct accel_param_pair resolutions[] = {
	{8,  KX022_RES_8BIT},
	{16, KX022_RES_16BIT}
};

/* List of ODR values in mHz and their associated register values. */
static const struct accel_param_pair datarates[] = {
	{781,     KX022_OSA_0_781HZ},
	{1563,    KX022_OSA_1_563HZ},
	{3125,    KX022_OSA_3_125HZ},
	{6250,    KX022_OSA_6_250HZ},
	{12500,   KX022_OSA_12_50HZ},
	{25000,   KX022_OSA_25_00HZ},
	{50000,   KX022_OSA_50_00HZ},
	{100000,  KX022_OSA_100_0HZ},
	{200000,  KX022_OSA_200_0HZ},
	{400000,  KX022_OSA_400_0HZ},
	{800000,  KX022_OSA_800_0HZ},
	{1600000, KX022_OSA_1600HZ}
};
/* TODO(aaboagye): Add the other tables for Directional Tap(TM), etc. */

/**
 * Find index into a accel_param_pair that matches the given engineering value
 * passed in. The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid index. If the request is
 * outside the range of values, it returns the closest valid index.
 */
static int find_param_index(const int eng_val, const int round_up,
			    const struct accel_param_pair *pairs,
			    const int size)
{
	int i;

	/* Linear search for index to match. */
	for (i = 0; i < size - 1; i++) {
		if (eng_val <= pairs[i].val)
			return i;

		if (eng_val < pairs[i+1].val) {
			if (round_up)
				return i + 1;
			else
				return i;
		}
	}

	return i;
}

/**
 * Read register from accelerometer.
 */
static int raw_read8(const int addr, const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_ACCEL, addr, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_ACCEL, addr, reg, data);
}

/**
 * Disable sensor by taking it out of operating mode. When disabled, the
 * acceleration data does not change.
 *
 * Note: This is intended to be called in a pair with enable_sensor().
 *
 * @data Pointer to motion sensor data
 * @cntl1 Pointer to location to store KX022_CNTL1 register after disabling
 *
 * @return EC_SUCCESS if successful, EC_ERROR_* otherwise
 */
static int disable_sensor(const struct motion_sensor_t *s, int *cntl1)
{
	int i, ret;

	/*
	 * Read the current state of the CNTL1 register
	 * so that we can restore it later.
	 */
	for (i = 0; i < SENSOR_ENABLE_ATTEMPTS; i++) {
		ret = raw_read8(s->addr, KX022_CNTL1, cntl1);
		if (ret != EC_SUCCESS)
			continue;

		*cntl1 &= ~KX022_CNTL1_PC1;

		ret = raw_write8(s->addr, KX022_CNTL1, *cntl1);
		if (ret == EC_SUCCESS)
			return EC_SUCCESS;
	}
	return ret;
}

/**
 * Enable sensor by placing it in operating mode.
 *
 * Note: This is intended to be called in a pair with disable_sensor().
 *
 * @data Pointer to motion sensor data
 * @cntl1 Value of KX022_CNTL1 register to write to sensor
 *
 * @return EC_SUCCESS if successful, EC_ERROR_* otherwise
 */
static int enable_sensor(const struct motion_sensor_t *s, int cntl1)
{
	int i, ret;

	for (i = 0; i < SENSOR_ENABLE_ATTEMPTS; i++) {
		ret = raw_read8(s->addr, KX022_CNTL1, &cntl1);
		if (ret != EC_SUCCESS)
			continue;

		/* Enable accelerometer based on cntl1 value. */
		ret = raw_write8(s->addr, KX022_CNTL1,
				cntl1 | KX022_CNTL1_PC1);

		/* On first success, we are done. */
		if (ret == EC_SUCCESS)
			return EC_SUCCESS;
	}
	return ret;
}

static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	int ret, cntl1, cntl1_new, index;
	struct kx022_data *data = s->drv_data;

	/* Find index for interface pair matching the specified range. */
	index = find_param_index(range, rnd, ranges, ARRAY_SIZE(ranges));

	/* Disable the sensor to allow for changing of critical parameters. */
	mutex_lock(s->mutex);
	ret = disable_sensor(s, &cntl1);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* Determine new value of CNTL1 reg and attempt to write it. */
	cntl1_new = (cntl1 & ~KX022_GSEL_FIELD) | ranges[index].reg;
	ret = raw_write8(s->addr,  KX022_CNTL1, cntl1_new);

	/* If successfully written, then save the range. */
	if (ret == EC_SUCCESS) {
		data->sensor_range = index;
		cntl1 = cntl1_new;
	}

	/* Re-enable the sensor. */
	if (enable_sensor(s, cntl1) != EC_SUCCESS)
		ret = EC_ERROR_UNKNOWN;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct kx022_data *data = s->drv_data;
	return ranges[data->sensor_range].val;
}

static int set_resolution(const struct motion_sensor_t *s, int res, int rnd)
{
	int ret, cntl1, cntl1_new, index;
	struct kx022_data *data = s->drv_data;

	/* Find index for interface pair matching the specified resolution. */
	index = find_param_index(res, rnd, resolutions,
			ARRAY_SIZE(resolutions));

	/* Disable the sensor to allow for changing of critical parameters. */
	mutex_lock(s->mutex);
	ret = disable_sensor(s, &cntl1);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* Determine new value of CNTL1 reg and attempt to write it. */
	cntl1_new = (cntl1 & ~KX022_RES_16BIT) | resolutions[index].reg;
	ret = raw_write8(s->addr,  KX022_CNTL1, cntl1_new);

	/* If successfully written, then save the range. */
	if (ret == EC_SUCCESS) {
		data->sensor_resolution = index;
		cntl1 = cntl1_new;
	}

	/* Re-enable the sensor. */
	if (enable_sensor(s, cntl1) != EC_SUCCESS)
		ret = EC_ERROR_UNKNOWN;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	struct kx022_data *data = s->drv_data;
	return resolutions[data->sensor_resolution].val;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, cntl1, index, odcntl, odcntl_new;
	struct kx022_data *data = s->drv_data;

	/* Find index for interface pair matching the specified rate. */
	index = find_param_index(rate, rnd, datarates, ARRAY_SIZE(datarates));

	/* Disable the sensor to allow for changing of critical parameters. */
	mutex_lock(s->mutex);
	ret = disable_sensor(s, &cntl1);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* Determine the new value of ODCNTL reg and attempt to write it. */
	ret = raw_read8(s->addr, KX022_ODCNTL, &odcntl);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	odcntl_new = (odcntl & ~KX022_OSA_FIELD) | datarates[index].reg;
	/* Set output data rate. */
	ret = raw_write8(s->addr, KX022_ODCNTL, odcntl_new);

	/* If successfully written, then save the new data rate. */
	if (ret == EC_SUCCESS)
		data->sensor_datarate = index;

	/* Re-enable the sensor. */
	if (enable_sensor(s, cntl1) != EC_SUCCESS)
		ret = EC_ERROR_UNKNOWN;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	struct kx022_data *data = s->drv_data;
	return datarates[data->sensor_datarate].val;
}

static int set_offset(const struct motion_sensor_t *s, const int16_t *offset,
		      int16_t temp)
{
	/* temperature is ignored */
	struct kx022_data *data = s->drv_data;
	data->offset[X] = offset[X];
	data->offset[Y] = offset[Y];
	data->offset[Z] = offset[Z];
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s, int16_t *offset,
		      int16_t *temp)
{
	struct kx022_data *data = s->drv_data;
	offset[X] = data->offset[X];
	offset[Y] = data->offset[Y];
	offset[Z] = data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t acc[6];
	uint8_t reg = KX022_XOUT_L;
	int ret, i, range, resolution;
	struct kx022_data *data = s->drv_data;

	/* Read 6 bytes starting at KX022_XOUT_L. */
	mutex_lock(s->mutex);
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, s->addr, &reg, 1, acc, 6,
		       I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);
	mutex_unlock(s->mutex);

	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * Convert acceleration to a signed 16-bit number. Note, based on
	 * the order of the registers:
	 *
	 * acc[0] = KX022_XOUT_L
	 * acc[1] = KX022_XOUT_H
	 * acc[2] = KX022_YOUT_L
	 * acc[3] = KX022_YOUT_H
	 * acc[4] = KX022_ZOUT_L
	 * acc[5] = KX022_ZOUT_H
	 *
	 * Add calibration offset before returning the data.
	 */
	resolution = get_resolution(s);
	for (i = X; i <= Z; i++) {
		v[i] = (((int8_t)acc[i * 2 + 1]) << 4) |
		       (acc[i * 2] >> 4);
		if (resolution == 8)
			v[i] = (int8_t)(v[i] & 0xff);
	}
	rotate(v, *s->rot_standard_ref, v);

	/* apply offset in the device coordinates */
	range = get_range(s);
	for (i = X; i <= Z; i++)
		v[i] += (data->offset[i] << 5) / range;

	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
static int config_interrupt(const struct motion_sensor_t *s)
{
	/*
	 * The KX022 has 2 interrupt pins, INT1 and INT2.  There are few
	 * features that can be enabled for interrupt sources.
	 *
	 * Directional Tap(TM) (Tap/Double Tap)
	 * Wake up (Motion Detect)
	 * Tilt position
	 * Buffer full
	 * Watermark
	 *
	 * For now, motion detect will be configured for the INT1 pin.
	 */

	/* TODO(aaboagye): Configure Directional Tap(TM) */
	int cntl1, val, ret;
	mutex_lock(s->mutex);

	/* Disable the sensor to allow for changing of critical parameters. */
	ret = disable_sensor(s, &cntl1);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Enable wake up (motion detect) functionality. */
	ret = raw_read8(s->addr, KX022_CNTL1, &val);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;
	val |= KX022_CNTL1_WUFE;
	ret = raw_write8(s->addr, KX022_CNTL1, val);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Set interrupt polarity to active HIGH and keep interrupt disabled. */
	ret = raw_read8(s->addr, KX022_INC1, &val);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;
	val |= KX022_INC1_IEA;
	val &= ~KX022_INC1_IEN;
	ret = raw_write8(s->addr, KX022_INC1, val);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Set output data rate for wake-up interrupt function. */
	ret = raw_read8(s->addr, KX022_CNTL3, &val);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;
	val &= ~KX022_CNTL3_OWUF_FIELD;
	val |= KX022_OWUF_100_0HZ;
	ret = raw_write8(s->addr, KX022_CNTL3, val);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Set interrupt to trigger on motion on any axis. */
	ret = raw_write8(s->addr, KX022_INC2,
			KX022_INC2_XNWUE | KX022_INC2_XPWUE |
			KX022_INC2_YNWUE | KX022_INC2_YPWUE |
			KX022_INC2_ZNWUE | KX022_INC2_ZPWUE);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/*
	 * Enable accel interrupts. Note: accels will not initiate an interrupt
	 * until interrupt enable bit(IEN1) in KX022_INC1 is set on the device.
	 */
	/* TODO: Enable gpio interrupts on the right pin. */

cleanup_exit:
	/* Enable the sensor. */
	ret = enable_sensor(s, cntl1);
	mutex_unlock(s->mutex);
	return ret;
}
#endif

static int init(const struct motion_sensor_t *s)
{
	int ret, val;
	uint8_t timeout;
	/* Issue a software reset. */
	mutex_lock(s->mutex);
	/*
	 * Place the sensor in standby mode to make changes.  This is also the
	 * reset value for the register.
	 */
	ret = raw_write8(s->addr, KX022_CNTL1, 0);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	ret = raw_read8(s->addr, KX022_CNTL2, &val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	val |= KX022_CNTL2_SRST;
	ret = raw_write8(s->addr, KX022_CNTL2, val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* The SRST will be cleared when reset is complete. */
	timeout = 0;
	do {
		msleep(1);

		ret = raw_read8(s->addr, KX022_CNTL2, &val);
		if (ret != EC_SUCCESS) {
			mutex_unlock(s->mutex);
			return ret;
		}

		/* Reset complete. */
		if ((ret == EC_SUCCESS) && !(val & KX022_CNTL2_SRST))
			break;

		/* Check for timeout. */
		if (timeout++ > 5) {
			ret = EC_ERROR_TIMEOUT;
			mutex_unlock(s->mutex);
			return ret;
		}
	} while (1);
	mutex_unlock(s->mutex);

	/* Initialize with the desired parameters. */
	ret = set_range(s, s->default_range, 1);
	if (ret != EC_SUCCESS)
		return ret;

	ret = set_resolution(s, 16, 1);
	if (ret != EC_SUCCESS)
		return ret;

#ifdef CONFIG_ACCEL_INTERRUPTS
	config_interrupts(s);
#endif

	CPRINTF("[%T %s: Done Init type:0x%X range:%d]\n",
		s->name, s->type, get_range(s));

	return ret;
}

const struct accelgyro_drv kx022_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = set_resolution,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = set_offset,
	.get_offset = get_offset,
	.perform_calib = NULL,
};
