/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Kionix Accelerometer driver for Chrome EC
 *
 * Supported: KX022, KXCJ9
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accel_kionix.h"
#include "driver/accel_kx022.h"
#include "driver/accel_kxcj9.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/* Number of times to attempt to enable sensor before giving up. */
#define SENSOR_ENABLE_ATTEMPTS 3

#ifdef CONFIG_ACCEL_KX022
/* List of range values in +/-G's and their associated register values. */
static const struct accel_param_pair kx022_ranges[] = {
	{2, KX022_GSEL_2G},
	{4, KX022_GSEL_4G},
	{8, KX022_GSEL_8G}
};

/* List of resolution values in bits and their associated register values. */
static const struct accel_param_pair kx022_resolutions[] = {
	{8,  KX022_RES_8BIT},
	{16, KX022_RES_16BIT}
};

/* List of ODR values in mHz and their associated register values. */
static const struct accel_param_pair kx022_datarates[] = {
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
#endif /* defined(CONFIG_ACCEL_KX022) */

#ifdef CONFIG_ACCEL_KXCJ9
/* List of range values in +/-G's and their associated register values. */
static const struct accel_param_pair kxcj9_ranges[] = {
	{2, KXCJ9_GSEL_2G},
	{4, KXCJ9_GSEL_4G},
	{8, KXCJ9_GSEL_8G_14BIT}
};

/* List of resolution values in bits and their associated register values. */
static const struct accel_param_pair kxcj9_resolutions[] = {
	{8,  KXCJ9_RES_8BIT},
	{12, KXCJ9_RES_12BIT}
};

/* List of ODR values in mHz and their associated register values. */
static const struct accel_param_pair kxcj9_datarates[] = {
	{0,       KXCJ9_OSA_0_000HZ},
	{781,     KXCJ9_OSA_0_781HZ},
	{1563,    KXCJ9_OSA_1_563HZ},
	{3125,    KXCJ9_OSA_3_125HZ},
	{6250,    KXCJ9_OSA_6_250HZ},
	{12500,   KXCJ9_OSA_12_50HZ},
	{25000,   KXCJ9_OSA_25_00HZ},
	{50000,   KXCJ9_OSA_50_00HZ},
	{100000,  KXCJ9_OSA_100_0HZ},
	{200000,  KXCJ9_OSA_200_0HZ},
	{400000,  KXCJ9_OSA_400_0HZ},
	{800000,  KXCJ9_OSA_800_0HZ},
	{1600000, KXCJ9_OSA_1600_HZ}
};
#endif /* defined(CONFIG_ACCEL_KXCJ9) */

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
 * @s Pointer to motion sensor data
 * @reg_val Pointer to location to store control register after disabling
 *
 * @return EC_SUCCESS if successful, EC_ERROR_* otherwise
 */
static int disable_sensor(const struct motion_sensor_t *s, int *reg_val)
{
	int i, ret, reg, pc1_field;
	struct kionix_accel_data *drv_data;

	drv_data = (struct kionix_accel_data *)s->drv_data;
	switch (drv_data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		reg = KX022_CNTL1;
		pc1_field = KX022_CNTL1_PC1;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		reg = KXCJ9_CTRL1;
		pc1_field = KXCJ9_CTRL1_PC1;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	/*
	 * Read the current state of the control register
	 * so that we can restore it later.
	 */
	for (i = 0; i < SENSOR_ENABLE_ATTEMPTS; i++) {
		ret = raw_read8(s->addr, reg, reg_val);
		if (ret != EC_SUCCESS)
			continue;

		*reg_val &= ~pc1_field;

		ret = raw_write8(s->addr, reg, *reg_val);
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
 * @s Pointer to motion sensor data
 * @reg_val Value of the control register to write to sensor
 *
 * @return EC_SUCCESS if successful, EC_ERROR_* otherwise
 */
static int enable_sensor(const struct motion_sensor_t *s, int reg_val)
{
	int i, ret, reg, pc1_field;
	struct kionix_accel_data *drv_data;

	drv_data = (struct kionix_accel_data *)s->drv_data;
	switch (drv_data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		reg = KX022_CNTL1;
		pc1_field = KX022_CNTL1_PC1;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		reg = KXCJ9_CTRL1;
		pc1_field = KXCJ9_CTRL1_PC1;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	for (i = 0; i < SENSOR_ENABLE_ATTEMPTS; i++) {
		ret = raw_read8(s->addr, reg, &reg_val);
		if (ret != EC_SUCCESS)
			continue;

		/* Enable accelerometer based on reg_val value. */
		ret = raw_write8(s->addr, reg,
				reg_val | pc1_field);

		/* On first success, we are done. */
		if (ret == EC_SUCCESS)
			return EC_SUCCESS;
	}
	return ret;
}

static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	int ret, reg_val, reg_val_new, index, range_field, reg, range_val;
	struct kionix_accel_data *data = s->drv_data;

	/* Find index for interface pair matching the specified range. */
	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		index = find_param_index(range, rnd, kx022_ranges,
					 ARRAY_SIZE(kx022_ranges));
		range_field = KX022_GSEL_FIELD;
		reg = KX022_CNTL1;
		range_val = kx022_ranges[index].reg;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		index = find_param_index(range, rnd, kxcj9_ranges,
					 ARRAY_SIZE(kxcj9_ranges));
		range_field = KXCJ9_GSEL_ALL;
		reg = KXCJ9_CTRL1;
		range_val = kxcj9_ranges[index].reg;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	/* Disable the sensor to allow for changing of critical parameters. */
	mutex_lock(s->mutex);
	ret = disable_sensor(s, &reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* Determine new value of control reg and attempt to write it. */
	reg_val_new = (reg_val & ~range_field) | range_val;
	ret = raw_write8(s->addr, reg, reg_val_new);

	/* If successfully written, then save the range. */
	if (ret == EC_SUCCESS) {
		data->sensor_range = index;
		reg_val = reg_val_new;
	}

	/* Re-enable the sensor. */
	if (enable_sensor(s, reg_val) != EC_SUCCESS)
		ret = EC_ERROR_UNKNOWN;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct kionix_accel_data *data = s->drv_data;

	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		return kx022_ranges[data->sensor_range].val;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		return kxcj9_ranges[data->sensor_range].val;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	}
	return 0;
}

static int set_resolution(const struct motion_sensor_t *s, int res, int rnd)
{
	int ret, reg_val, reg_val_new, index, reg, res_field, res_val;
	struct kionix_accel_data *data = s->drv_data;

	/* Find index for interface pair matching the specified resolution. */
	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		index = find_param_index(res, rnd, kx022_resolutions,
					 ARRAY_SIZE(kx022_resolutions));
		res_val = kx022_resolutions[index].reg;
		reg = KX022_CNTL1;
		res_field = KX022_RES_16BIT;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		index = find_param_index(res, rnd, kxcj9_resolutions,
					 ARRAY_SIZE(kxcj9_resolutions));
		res_val = kxcj9_resolutions[index].reg;
		reg = KXCJ9_CTRL1;
		res_field = KXCJ9_RES_12BIT;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	/* Disable the sensor to allow for changing of critical parameters. */
	mutex_lock(s->mutex);
	ret = disable_sensor(s, &reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* Determine new value of the control reg and attempt to write it. */
	reg_val_new = (reg_val & ~res_field) | res_val;
	ret = raw_write8(s->addr, reg, reg_val_new);

	/* If successfully written, then save the range. */
	if (ret == EC_SUCCESS) {
		data->sensor_resolution = index;
		reg_val = reg_val_new;
	}

	/* Re-enable the sensor. */
	if (enable_sensor(s, reg_val) != EC_SUCCESS)
		ret = EC_ERROR_UNKNOWN;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	struct kionix_accel_data *data = s->drv_data;
	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		return kx022_resolutions[data->sensor_resolution].val;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		return kxcj9_resolutions[data->sensor_resolution].val;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};
	return 0;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, reg_val, index, odr_val, odr_val_new, reg, odr_field,
		odr_reg_val;
	struct kionix_accel_data *data = s->drv_data;

	/* Find index for interface pair matching the specified rate. */
	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		index = find_param_index(rate, rnd, kx022_datarates,
					 ARRAY_SIZE(kx022_datarates));
		odr_val = kx022_datarates[index].reg;
		reg = KX022_ODCNTL;
		odr_field = KX022_OSA_FIELD;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		index = find_param_index(rate, rnd, kxcj9_datarates,
					 ARRAY_SIZE(kxcj9_datarates));
		odr_val = kxcj9_datarates[index].reg;
		reg = KXCJ9_DATA_CTRL;
		odr_field = KXCJ9_OSA_FIELD;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	/* Disable the sensor to allow for changing of critical parameters. */
	mutex_lock(s->mutex);
	ret = disable_sensor(s, &reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* Determine the new value of ODCNTL reg and attempt to write it. */
	ret = raw_read8(s->addr, reg, &odr_reg_val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	odr_val_new = (odr_reg_val & ~odr_field) | odr_val;
	/* Set output data rate. */
	ret = raw_write8(s->addr, reg, odr_val_new);

	/* If successfully written, then save the new data rate. */
	if (ret == EC_SUCCESS)
		data->sensor_datarate = index;

	/* Re-enable the sensor. */
	if (enable_sensor(s, reg_val) != EC_SUCCESS)
		ret = EC_ERROR_UNKNOWN;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	struct kionix_accel_data *data = s->drv_data;

	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		return kx022_datarates[data->sensor_datarate].val;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		return kxcj9_datarates[data->sensor_datarate].val;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};
	return 0;
}

static int set_offset(const struct motion_sensor_t *s, const int16_t *offset,
		      int16_t temp)
{
	/* temperature is ignored */
	struct kionix_accel_data *data = s->drv_data;
	data->offset[X] = offset[X];
	data->offset[Y] = offset[Y];
	data->offset[Z] = offset[Z];
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s, int16_t *offset,
		      int16_t *temp)
{
	struct kionix_accel_data *data = s->drv_data;
	offset[X] = data->offset[X];
	offset[Y] = data->offset[Y];
	offset[Z] = data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t acc[6];
	uint8_t reg;
	int ret, i, range, resolution;
	struct kionix_accel_data *data = s->drv_data;

	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		reg = KX022_XOUT_L;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		reg = KXCJ9_XOUT_L;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	/* Read 6 bytes starting at XOUT_L. */
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
	 * acc[0] = XOUT_L
	 * acc[1] = XOUT_H
	 * acc[2] = YOUT_L
	 * acc[3] = YOUT_H
	 * acc[4] = ZOUT_L
	 * acc[5] = ZOUT_H
	 *
	 * Add calibration offset before returning the data.
	 */
	resolution = get_resolution(s);
	for (i = X; i <= Z; i++) {
		v[i] = (((int8_t)acc[i * 2 + 1]) << 4) |
		       (acc[i * 2] >> 4);
		if (KXCJ9 == data->variant)
			v[i] <<= (16 - resolution);
		else if ((KX022 == data->variant) && (resolution == 8))
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
#ifdef CONFIG_ACCEL_KXCJ9
static int config_kxcj9_interrupt(const struct motion_sensor_t *s)
{
	int ctrl1;
	mutex_lock(s->mutex);

	/* Disable the sensor to allow for changing of critical parameters. */
	ret = disable_sensor(s, &ctrl1);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Enable wake up (motion detect) functionality. */
	ret = raw_read8(s->addr, KXCJ9_CTRL1, &tmp);
	tmp &= ~KXCJ9_CTRL1_PC1;
	tmp |= KXCJ9_CTRL1_WUFE;
	ret = raw_write8(s->addr, KXCJ9_CTRL1, tmp);

	/* Set interrupt polarity to rising edge and keep interrupt disabled. */
	ret = raw_write8(s->addr,
			  KXCJ9_INT_CTRL1,
			  KXCJ9_INT_CTRL1_IEA);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Set output data rate for wake-up interrupt function. */
	ret = raw_write8(s->addr, KXCJ9_CTRL2, KXCJ9_OWUF_100_0HZ);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/* Set interrupt to trigger on motion on any axis. */
	ret = raw_write8(s->addr, KXCJ9_INT_CTRL2,
			KXCJ9_INT_SRC2_XNWU | KXCJ9_INT_SRC2_XPWU |
			KXCJ9_INT_SRC2_YNWU | KXCJ9_INT_SRC2_YPWU |
			KXCJ9_INT_SRC2_ZNWU | KXCJ9_INT_SRC2_ZPWU);
	if (ret != EC_SUCCESS)
		goto cleanup_exit;

	/*
	 * Enable accel interrupts. Note: accels will not initiate an interrupt
	 * until interrupt enable bit in KXCJ9_INT_CTRL1 is set on the device.
	 */
	gpio_enable_interrupt(GPIO_ACCEL_INT_LID);
	gpio_enable_interrupt(GPIO_ACCEL_INT_BASE);

	/* Enable the sensor. */
	ret = enable_sensor(s, ctrl1);
cleanup_exit:
	mutex_unlock(s->mutex);
	return ret;
}
#endif /* defined(CONFIG_ACCEL_KXCJ9) */

#ifdef CONFIG_ACCEL_KX022
static int config_kx022_interrupt(const struct motion_sensor_t *s)
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
#endif /* defined(CONFIG_ACCEL_KX022) */
#endif /* defined(CONFIG_ACCEL_INTERRUPTS) */

static int init(const struct motion_sensor_t *s)
{
	int ret, val, reg, reset_field;
	uint8_t timeout;
	struct kionix_accel_data *data;
	data = (struct kionix_accel_data *)s->drv_data;

	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		reg = KX022_CNTL2;
		reset_field = KX022_CNTL2_SRST;
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		reg = KXCJ9_CTRL2;
		reset_field = KXCJ9_CTRL2_SRST;
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};

	/* Issue a software reset. */
	mutex_lock(s->mutex);

	/* Place the sensor in standby mode to make changes. */
	ret = disable_sensor(s, &val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	ret = raw_read8(s->addr, reg, &val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}
	val |= reset_field;
	ret = raw_write8(s->addr, reg, val);
	if (ret != EC_SUCCESS) {
		mutex_unlock(s->mutex);
		return ret;
	}

	/* The SRST will be cleared when reset is complete. */
	timeout = 0;
	do {
		msleep(1);

		ret = raw_read8(s->addr, reg, &val);
		if (ret != EC_SUCCESS) {
			mutex_unlock(s->mutex);
			return ret;
		}

		/* Reset complete. */
		if ((ret == EC_SUCCESS) && !(val & reset_field))
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

	if (KXCJ9 == data->variant)
		ret = set_resolution(s, 12, 1);
	else if (KX022 == data->variant)
		ret = set_resolution(s, 16, 1);
	if (ret != EC_SUCCESS)
		return ret;

#ifdef CONFIG_ACCEL_INTERRUPTS
	switch (data->variant) {
#ifdef CONFIG_ACCEL_KX022
	case KX022:
		config_kx022_interrupt(s);
		break;
#endif /* defined(CONFIG_ACCEL_KX022) */
#ifdef CONFIG_ACCEL_KXCJ9
	case KXCJ9:
		config_kxcj9_interrupt(s);
		break;
#endif /* defined(CONFIG_ACCEL_KXCJ9) */
	default:
		break;
	};
#endif /* defined(CONFIG_ACCEL_INTERRUPTS) */

	CPRINTF("[%T %s: Done Init type:0x%X range:%d]\n",
		s->name, s->type, get_range(s));

	mutex_unlock(s->mutex);
	return ret;
}

const struct accelgyro_drv kionix_accel_drv = {
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
