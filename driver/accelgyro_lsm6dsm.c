/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LSM6DSM accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)

/*
 * ODR are in Hz motion sense wants in milliHz
 */
#define HZ2mHZ(_mHz)	(_mHz * 1000)

/* List of ODR (gyro off) values in mHz and their associated register values.*/
struct lsm6dsm_odr_reg {
	uint32_t hz;
	uint8_t value;
};

static const struct lsm6dsm_odr_table {
	uint8_t addr[2];
	uint8_t mask[2];
	struct lsm6dsm_odr_reg odr_avl[LSM6DSM_ODR_LIST_NUM];
} lsm6dsm_odr_table = {
	.addr[LSM6DSM_ACCEL] = LSM6DSM_ACC_ODR_ADDR,
	.mask[LSM6DSM_ACCEL] = LSM6DSM_ACC_ODR_MASK,
	.addr[LSM6DSM_GYRO] = LSM6DSM_GYR_ODR_ADDR,
	.mask[LSM6DSM_GYRO] = LSM6DSM_GYR_ODR_MASK,
	.odr_avl[0] = { .hz = HZ2mHZ(13), .value = LSM6DSM_ODR_13HZ_VAL },
	.odr_avl[1] = { .hz = HZ2mHZ(26), .value = LSM6DSM_ODR_26HZ_VAL },
	.odr_avl[2] = { .hz = HZ2mHZ(52), .value = LSM6DSM_ODR_52HZ_VAL },
	.odr_avl[3] = { .hz = HZ2mHZ(104), .value = LSM6DSM_ODR_104HZ_VAL },
	.odr_avl[4] = { .hz = HZ2mHZ(208), .value = LSM6DSM_ODR_208HZ_VAL },
	.odr_avl[5] = { .hz = HZ2mHZ(416), .value = LSM6DSM_ODR_416HZ_VAL },
#ifdef LSM6DSM_HIFI
	/* Hi ODR */
	.odr_avl[6] = { .hz = HZ2mHZ(833), .value = LSM6DSM_ODR_833HZ_VAL },
	.odr_avl[7] = { .hz = HZ2mHZ(1660), .value = LSM6DSM_ODR_1660HZ_VAL },
	.odr_avl[8] = { .hz = HZ2mHZ(3330), .value = LSM6DSM_ODR_3330HZ_VAL },
	.odr_avl[9] = { .hz = HZ2mHZ(6660), .value = LSM6DSM_ODR_6660HZ_VAL },
#endif /* LSM6DSM_HIFI */
};

struct lsm6dsm_fs_reg {
	unsigned int gain;
	uint8_t value;
	uint32_t urv;
};

/* Full Scale Range Table for Acc & Gyro */
static struct lsm6dsm_fs_table {
	uint8_t addr;
	uint8_t mask;
	struct lsm6dsm_fs_reg fs_avl[LSM6DSM_FS_LIST_NUM];
} lsm6dsm_fs_table[LSM6DSM_SENSORS_NUMB] = {
	[LSM6DSM_ACCEL] = {
		.addr = LSM6DSM_ACCEL_FS_ADDR,
		.mask = LSM6DSM_ACCEL_FS_MASK,
		.fs_avl[0] = { .gain = LSM6DSM_ACCEL_FS_2G_GAIN,
			       .value = LSM6DSM_ACCEL_FS_2G_VAL,
			       .urv = 2, },
		.fs_avl[1] = { .gain = LSM6DSM_ACCEL_FS_4G_GAIN,
			       .value = LSM6DSM_ACCEL_FS_4G_VAL,
			       .urv = 4, },
		.fs_avl[2] = { .gain = LSM6DSM_ACCEL_FS_8G_GAIN,
			       .value = LSM6DSM_ACCEL_FS_8G_VAL,
			       .urv = 8, },
		.fs_avl[3] = { .gain = LSM6DSM_ACCEL_FS_16G_GAIN,
			       .value = LSM6DSM_ACCEL_FS_16G_VAL,
			       .urv = 16, },
	},
	[LSM6DSM_GYRO] = {
		.addr = LSM6DSM_GYRO_FS_ADDR,
		.mask = LSM6DSM_GYRO_FS_MASK,
		.fs_avl[0] = { .gain = LSM6DSM_GYRO_FS_245_GAIN,
			       .value = LSM6DSM_GYRO_FS_245_VAL,
			       .urv = 245, },
		.fs_avl[1] = { .gain = LSM6DSM_GYRO_FS_500_GAIN,
			       .value = LSM6DSM_GYRO_FS_500_VAL,
			       .urv = 500, },
		.fs_avl[2] = { .gain = LSM6DSM_GYRO_FS_1000_GAIN,
			       .value = LSM6DSM_GYRO_FS_1000_VAL,
			       .urv = 1000, },
		.fs_avl[3] = { .gain = LSM6DSM_GYRO_FS_2000_GAIN,
			       .value = LSM6DSM_GYRO_FS_2000_VAL,
			       .urv = 2000, },
	}
};

struct lsm6dsm_data lsm6dsm_a_data;
struct lsm6dsm_data lsm6dsm_g_data;

static inline int get_xyz_reg(enum motionsensor_type type)
{
	return (MOTIONSENSE_TYPE_ACCEL == type) ?
		LSM6DSM_ACCEL_OUT_X_L_ADDR : LSM6DSM_GYRO_OUT_X_L_ADDR;
}

/**
 * Read single register
 */
static inline int raw_read8(const int port, const int addr, const int reg,
			    int *data_ptr)
{
	return i2c_read8(port, addr, reg, data_ptr);
}

/**
 * Write single register
 */
static inline int raw_write8(const int port, const int addr, const int reg,
			     int data)
{
	return i2c_write8(port, addr, reg, data);
}

/**
 * __ffs - find first bit in mask
 * @mask: The mask to search
 */
static inline uint8_t __ffs(uint8_t mask)
{
	uint8_t num = 0;

	if ((mask & 0xff) == 0) {
		num += 8;
		mask >>= 8;
	}
	if ((mask & 0xf) == 0) {
		num += 4;
		mask >>= 4;
	}
	if ((mask & 0x3) == 0) {
		num += 2;
		mask >>= 2;
	}
	if ((mask & 0x1) == 0)
		mask += 1;

	return num;
}

 /**
 * write_data_with_mask - Write register with mask
 * @s: Motion sensor pointer
 * @reg: Device register
 * @mask: The mask to search
 * @data: Data pointer
 */
static int write_data_with_mask(const struct motion_sensor_t *s, int reg,
				uint8_t mask, uint8_t data)
{
	int err;
	int new_data = 0x00, old_data = 0x00;

	err = raw_read8(s->port, s->addr, reg, &old_data);
	if (err != EC_SUCCESS)
		return err;

	new_data = ((old_data & (~mask)) | ((data << __ffs(mask)) & mask));

	if (new_data == old_data)
		return EC_SUCCESS;

	return raw_write8(s->port, s->addr, reg, new_data);
}

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 */
static int set_range(const struct motion_sensor_t *s,
		     int range, int rnd)
{
	int err, i;
	uint8_t index =
		(s->type == MOTIONSENSE_TYPE_GYRO ? LSM6DSM_GYRO : LSM6DSM_ACCEL);
	struct lsm6dsm_data *data = s->drv_data;

	for (i = 0; i < LSM6DSM_FS_LIST_NUM; i++) {
		if (lsm6dsm_fs_table[index].fs_avl[i].urv == range)
			break;
	}

	if (i == LSM6DSM_FS_LIST_NUM)
		return EC_ERROR_INVAL;

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);
	err = write_data_with_mask(s,
				   lsm6dsm_fs_table[index].addr,
				   lsm6dsm_fs_table[index].mask,
				   lsm6dsm_fs_table[index].fs_avl[i].value);

	if (err == EC_SUCCESS) {
		data->base.range = lsm6dsm_fs_table[index].fs_avl[i].urv;
		data->fs_id = i;
	}

	mutex_unlock(s->mutex);
	return EC_SUCCESS;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct lsm6dsm_data *data = s->drv_data;

	return data->base.range;
}

static int set_resolution(const struct motion_sensor_t *s, int res, int rnd)
{
	/* Only one resolution, LSM6DSM_RESOLUTION, so nothing to do. */
	return EC_SUCCESS;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	/* Only one resolution, LSM6DSM_RESOLUTION, so nothing to do. */
	return LSM6DSM_RESOLUTION;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, i;
	struct lsm6dsm_data *data = s->drv_data;
	uint8_t index =
		(s->type == MOTIONSENSE_TYPE_GYRO ? LSM6DSM_GYRO : LSM6DSM_ACCEL);
	uint8_t value = LSM6DSM_ODR_POWER_OFF_VAL;
	uint32_t odr = data->base.odr;

	if (rate > 0) {
		for (i = 0; i < LSM6DSM_ODR_LIST_NUM; i++) {
			if (lsm6dsm_odr_table.odr_avl[i].hz >= rate)
				break;
		}

		if (i == LSM6DSM_ODR_LIST_NUM)
			return EC_ERROR_INVAL;

		value = lsm6dsm_odr_table.odr_avl[i].value;
		odr = lsm6dsm_odr_table.odr_avl[i].hz;
	}

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);
	ret = write_data_with_mask(s, lsm6dsm_odr_table.addr[index],
				   lsm6dsm_odr_table.mask[index],
				   value);

	if (ret == EC_SUCCESS)
		data->base.odr = odr;

	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	struct lsm6dsm_data *data = s->drv_data;

	return data->base.odr;
}

static int set_offset(const struct motion_sensor_t *s,
		      const int16_t *offset, int16_t temp)
{
	struct lsm6dsm_data *data = s->drv_data;

	data->offset[X] = offset[X];
	data->offset[Y] = offset[Y];
	data->offset[Z] = offset[Z];
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s,
		      int16_t *offset,
		      int16_t *temp)
{
	struct lsm6dsm_data *data = s->drv_data;

	offset[X] = data->offset[X];
	offset[Y] = data->offset[Y];
	offset[Z] = data->offset[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
static int set_interrupt(const struct motion_sensor_t *s,
			 unsigned int threshold)
{
	/* TODO: Currently unsupported. */
	return EC_ERROR_UNKNOWN;
}
#endif

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->port, s->addr, LSM6DSM_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	if (MOTIONSENSE_TYPE_ACCEL == s->type)
		*ready = (LSM6DSM_STS_XLDA_UP == (tmp & LSM6DSM_STS_XLDA_MASK));
	else
		*ready = (LSM6DSM_STS_GDA_UP == (tmp & LSM6DSM_STS_GDA_MASK));

	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t raw[6];
	uint8_t xyz_reg;
	int ret, range, i, tmp = 0;
	struct lsm6dsm_data *data = s->drv_data;
	uint8_t index =
		(s->type == MOTIONSENSE_TYPE_GYRO ? LSM6DSM_GYRO : LSM6DSM_ACCEL);

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

	xyz_reg = get_xyz_reg(s->type);

	/* Read 6 bytes starting at xyz_reg */
	i2c_lock(s->port, 1);
	ret = i2c_xfer(s->port, s->addr, &xyz_reg, 1, raw,
		       LSM6DSM_OUT_XYZ_SIZE, I2C_XFER_SINGLE);
	i2c_lock(s->port, 0);

	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error]",
			s->name, s->type);
		return ret;
	}

	for (i = X; i <= Z; i++) {
		v[i] = ((int16_t)((raw[i * 2 + 1] << 8) | raw[i * 2]));
		/* Multiply axis gain related to FS */
		v[i] = v[i] * lsm6dsm_fs_table[index].fs_avl[data->fs_id].gain;
	}
	/* Apply rotation matrix */
	rotate(v, *s->rot_standard_ref, v);

	/* apply offset in the device coordinates */
	range = get_range(s);
	for (i = X; i <= Z; i++)
		v[i] += (data->offset[i] << 5) / range;

	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
/**
 * config_interrupt - configure interrupt for embedded algo
 * @s: Motion sensor pointer
 */
static int config_interrupt(const struct motion_sensor_t *s)
{
	int ret;

	if (s->type != MOTIONSENSE_TYPE_ACCEL)
		return EC_SUCCESS;

	mutex_lock(s->mutex);

	/* Enable Latch Mode Bit */
	ret = write_data_with_mask(s, LSM6DSM_LIR_ADDR,
				   LSM6DSM_LIR_MASK, LSM6DSM_EN_BIT);
	if (ret != EC_SUCCESS)
		goto err_unlock;

	ret = write_data_with_mask(s, LSM6DSM_INT2_ON_INT1_ADDR,
							   LSM6DSM_INT2_ON_INT1_MASK,
							   LSM6DSM_EN_BIT);
err_unlock:
	mutex_unlock(s->mutex);
	return ret;
}
#endif /* CONFIG_ACCEL_INTERRUPTS */

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;

	ret = raw_read8(s->port, s->addr, LSM6DSM_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LSM6DSM_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * This sensor can be powered through an EC reboot, so the state of
	 * the sensor is unknown here. Initiate software reset to restore
	 * sensor to default.
	 * SW_RESET software reset
	 * BDU Enable Block Data Update.
	 * 
	 * lsm6dsm supports both accel & gyro features
	 * Board will see two virtual sensor devices: accel & gyro.
	 * Requirement: Accel need be init before gyro.
	 * SW_RESET is down for accel only!
	 */
	if (MOTIONSENSE_TYPE_ACCEL == s->type) {
		mutex_lock(s->mutex);

		/* Software reset */
		ret = write_data_with_mask(s, LSM6DSM_RESET_ADDR,
					   LSM6DSM_RESET_MASK, LSM6DSM_EN_BIT);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		/* Output data not updated until have been read */
		ret = write_data_with_mask(s, LSM6DSM_BDU_ADDR,
					   LSM6DSM_BDU_MASK, LSM6DSM_EN_BIT);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		mutex_unlock(s->mutex);

		/* Config initial Acc Range */
		ret = set_range(s, s->default_range, 0);
		if (ret != EC_SUCCESS)
			return ret;
	}

	if (MOTIONSENSE_TYPE_GYRO == s->type) {
		/* Config initial Gyro Range */
		ret = set_range(s, s->default_range, 0);
		if (ret != EC_SUCCESS)
			return ret;
	}

#ifdef CONFIG_ACCEL_INTERRUPTS
		ret = config_interrupt(s);
#endif /* CONFIG_ACCEL_INTERRUPTS */

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d]\n",
		s->name, s->type, get_range(s));
	return ret;

err_unlock:
	mutex_unlock(s->mutex);

	return EC_ERROR_UNKNOWN;
}

const struct accelgyro_drv lsm6dsm_drv = {
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
#ifdef CONFIG_ACCEL_INTERRUPTS
	.set_interrupt = set_interrupt,
#endif
};
