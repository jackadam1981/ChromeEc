/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "driver/accelgyro_bmi_common.h"
#include "motion_sense_fifo.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

/* List of range values in +/-G's and their associated register values. */
const struct bmi_accel_param_pair g_ranges[] = {
	{2, BMI_GSEL_2G},
	{4, BMI_GSEL_4G},
	{8, BMI_GSEL_8G},
	{16, BMI_GSEL_16G}
};

/*
 * List of angular rate range values in +/-dps's
 * and their associated register values.
 */
const struct bmi_accel_param_pair dps_ranges[] = {
	{125, BMI_DPS_SEL_125},
	{250, BMI_DPS_SEL_250},
	{500, BMI_DPS_SEL_500},
	{1000, BMI_DPS_SEL_1000},
	{2000, BMI_DPS_SEL_2000}
};

int get_xyz_reg(enum motionsensor_type type)
{
	switch (type) {
	case MOTIONSENSE_TYPE_ACCEL:
		return BMI_ACC_X_L_G;
	case MOTIONSENSE_TYPE_GYRO:
		return BMI_GYR_X_L_G;
	case MOTIONSENSE_TYPE_MAG:
		return BMI_AUX_X_L_G;
	default:
		return -1;
	}
}

const struct bmi_accel_param_pair *bmi_get_range_table(
		enum motionsensor_type type, int *psize)
{
	if (type == MOTIONSENSE_TYPE_ACCEL) {
		if (psize)
			*psize = ARRAY_SIZE(g_ranges);
		return g_ranges;
	}
	if (psize)
		*psize = ARRAY_SIZE(dps_ranges);
	return dps_ranges;
}

/**
 * @return reg value that matches the given engineering value passed in.
 * The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid reg value. If the request is
 * outside the range of values, it returns the closest valid reg value.
 */
int bmi_get_reg_val(const int eng_val, const int round_up,
		    const struct bmi_accel_param_pair *pairs,
		    const int size)
{
	int i;

	for (i = 0; i < size - 1; i++) {
		if (eng_val <= pairs[i].val)
			break;

		if (eng_val < pairs[i+1].val) {
			if (round_up)
				i += 1;
			break;
		}
	}
	return pairs[i].reg_val;
}

/**
 * @return engineering value that matches the given reg val
 */
int bmi_get_engineering_val(const int reg_val,
			    const struct bmi_accel_param_pair *pairs,
			    const int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (reg_val == pairs[i].reg_val)
			break;
	}
	return pairs[i].val;
}

#ifdef CONFIG_SPI_ACCEL_PORT
int bmi_spi_raw_read(const int addr, const uint8_t reg,
		     uint8_t *data, const int len)
{
	uint8_t cmd = 0x80 | reg;

	return spi_transaction(&spi_devices[addr], &cmd, 1, data, len);
}
#endif

/**
 * Read 8bit register from accelerometer.
 */
int bmi_read8(const int port, const uint16_t i2c_spi_addr_flags,
	      const int reg, int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		uint8_t val;

		rv = bmi_spi_raw_read(SLAVE_GET_SPI_ADDR(i2c_spi_addr_flags),
				  reg, &val, 1);
		if (rv == EC_SUCCESS)
			*data_ptr = val;
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read8(port, i2c_spi_addr_flags,
			       reg, data_ptr);
#endif
	}
	return rv;
}

/**
 * Write 8bit register from accelerometer.
 */
int bmi_write8(const int port, const uint16_t i2c_spi_addr_flags,
	       const int reg, int data)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		uint8_t cmd[2] = { reg, data };

		rv = spi_transaction(
			&spi_devices[SLAVE_GET_SPI_ADDR(i2c_spi_addr_flags)],
			cmd, 2, NULL, 0);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_write8(port, i2c_spi_addr_flags,
				reg, data);
#endif
	}
	/*
	 * From Bosch:  BMI160 needs a delay of 450us after each write if it
	 * is in suspend mode, otherwise the operation may be ignored by
	 * the sensor. Given we are only doing write during init, add
	 * the delay unconditionally.
	 */
	msleep(1);
	return rv;
}

/**
 * Read 16bit register from accelerometer.
 */
int bmi_read16(const int port, const uint16_t i2c_spi_addr_flags,
	       const uint8_t reg, int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		rv = bmi_spi_raw_read(SLAVE_GET_SPI_ADDR(i2c_spi_addr_flags),
				  reg, (uint8_t *)data_ptr, 2);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read16(port, i2c_spi_addr_flags,
				reg, data_ptr);
#endif
	}
	return rv;
}

/**
 * Write 16bit register from accelerometer.
 */
int bmi_write16(const int port, const uint16_t i2c_spi_addr_flags,
		const int reg, int data)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		CPRINTS("%s() spi part is not implemented", __func__);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_write16(port, i2c_spi_addr_flags,
				 reg, data);
#endif
	}
	/*
	 * From Bosch:  BMI260 needs a delay of 450us after each write if it
	 * is in suspend mode, otherwise the operation may be ignored by
	 * the sensor. Given we are only doing write during init, add
	 * the delay unconditionally.
	 */
	msleep(1);
	return rv;
}

/**
 * Read 32bit register from accelerometer.
 */
int bmi_read32(const int port, const uint16_t i2c_spi_addr_flags,
	       const uint8_t reg, int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		rv = bmi_spi_raw_read(SLAVE_GET_SPI_ADDR(i2c_spi_addr_flags),
				  reg, (uint8_t *)data_ptr, 4);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read32(port, i2c_spi_addr_flags,
				reg, data_ptr);
#endif
	}
	return rv;
}

/**
 * Read n bytes from accelerometer.
 */
int bmi_read_n(const int port, const uint16_t i2c_spi_addr_flags,
	       const uint8_t reg, uint8_t *data_ptr, const int len)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		rv = bmi_spi_raw_read(SLAVE_GET_SPI_ADDR(i2c_spi_addr_flags),
				  reg, data_ptr, len);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read_block(port, i2c_spi_addr_flags,
				    reg, data_ptr, len);
#endif
	}
	return rv;
}

/**
 * Write n bytes from accelerometer.
 */
int bmi_write_n(const int port, const uint16_t i2c_spi_addr_flags,
		const uint8_t reg, uint8_t *data_ptr, const int len)
{
	int rv = -EC_ERROR_PARAM1;

	if (SLAVE_IS_SPI(i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		CPRINTS("%s() spi part is not implemented", __func__);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_write_block(port, i2c_spi_addr_flags,
				     reg, data_ptr, len);
#endif
	}
	/*
	 * From Bosch:  BMI260 needs a delay of 450us after each write if it
	 * is in suspend mode, otherwise the operation may be ignored by
	 * the sensor. Given we are only doing write during init, add
	 * the delay unconditionally.
	 */
	msleep(1);
	return rv;
}
/*
 * Enable specific bit set of a 8-bit reg.
 */
int enable_reg8_bits(const struct motion_sensor_t *s, int reg, uint8_t bits)
{
	int ret, val;

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags, reg, &val);
	val |= bits;
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags, reg, val);
	return ret;
}
/*
 * Disable specific bit set of a 8-bit reg.
 */
int disable_reg8_bits(const struct motion_sensor_t *s, int reg, uint8_t bits)
{
	int ret, val;

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags, reg, &val);
	val &= ~bits;
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags, reg, val);
	return ret;
}

void bmi_normalize(const struct motion_sensor_t *s, intv3_t v, uint8_t *input)
{
	int i;
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

#ifdef CONFIG_MAG_BMI_BMM150
	if (s->type == MOTIONSENSE_TYPE_MAG)
		bmm150_normalize(s, v, input);
	else
#endif
#ifdef CONFIG_MAG_BMI_LIS2MDL
	if (s->type == MOTIONSENSE_TYPE_MAG)
		lis2mdl_normalize(s, v, data);
	else
#endif
	{
		v[0] = ((int16_t)((input[1] << 8) | input[0]));
		v[1] = ((int16_t)((input[3] << 8) | input[2]));
		v[2] = ((int16_t)((input[5] << 8) | input[4]));
	}
	rotate(v, *s->rot_standard_ref, v);
	for (i = X; i <= Z; i++)
		v[i] = SENSOR_APPLY_SCALE(v[i], data->scale[i]);
}

int bmi_decode_header(struct motion_sensor_t *accel,
		enum fifo_header hdr, uint32_t last_ts,
		uint8_t **bp, uint8_t *ep)
{
	if ((hdr & BMI_FH_MODE_MASK) == BMI_FH_EMPTY &&
			(hdr & BMI_FH_PARM_MASK) != 0) {
		int i, size = 0;
		/* Check if there is enough space for the data frame */
		for (i = MOTIONSENSE_TYPE_MAG; i >= MOTIONSENSE_TYPE_ACCEL;
		     i--) {
			if (hdr & (1 << (i + BMI_FH_PARM_OFFSET)))
				size += (i == MOTIONSENSE_TYPE_MAG ? 8 : 6);
		}
		if (*bp + size > ep) {
			/* frame is not complete, it will be retransmitted. */
			*bp = ep;
			return 1;
		}
		for (i = MOTIONSENSE_TYPE_MAG; i >= MOTIONSENSE_TYPE_ACCEL;
		     i--) {
			struct motion_sensor_t *s = accel + i;

			if (hdr & (1 << (i + BMI_FH_PARM_OFFSET))) {
				struct ec_response_motion_sensor_data vector;
				int *v = s->raw_xyz;

				vector.flags = 0;
				bmi_normalize(s, v, *bp);
				if (IS_ENABLED(CONFIG_ACCEL_SPOOF_MODE) &&
					s->flags &
					MOTIONSENSE_FLAG_IN_SPOOF_MODE)
					v = s->spoof_xyz;
				vector.data[X] = v[X];
				vector.data[Y] = v[Y];
				vector.data[Z] = v[Z];
				vector.sensor_num = s - motion_sensors;
				motion_sense_fifo_stage_data(&vector, s, 3,
						last_ts);
				*bp += (i == MOTIONSENSE_TYPE_MAG ? 8 : 6);
			}
		}

		return 1;
	} else {
		return 0;
	}
}

int bmi_set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	int ret, range_tbl_size;
	uint8_t reg_val, ctrl_reg;
	const struct bmi_accel_param_pair *ranges;
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

	if (s->type == MOTIONSENSE_TYPE_MAG) {
		data->range = range;
		return EC_SUCCESS;
	}

	ctrl_reg = BMI_RANGE_REG(s->type);
	ranges = bmi_get_range_table(s->type, &range_tbl_size);
	reg_val = bmi_get_reg_val(range, rnd, ranges, range_tbl_size);

	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 ctrl_reg, reg_val);
	/* Now that we have set the range, update the driver's value. */
	if (ret == EC_SUCCESS)
		data->range = bmi_get_engineering_val(reg_val, ranges,
				range_tbl_size);
	return ret;
}

int bmi_get_range(const struct motion_sensor_t *s)
{
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

	return data->range;
}

int bmi_get_resolution(const struct motion_sensor_t *s)
{
	return BMI_RESOLUTION;
}

int bmi_set_scale(const struct motion_sensor_t *s,
	      const uint16_t *scale, int16_t temp)
{
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

	data->scale[X] = scale[X];
	data->scale[Y] = scale[Y];
	data->scale[Z] = scale[Z];
	return EC_SUCCESS;
}

int bmi_get_scale(const struct motion_sensor_t *s,
	      uint16_t *scale, int16_t *temp)
{
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

	scale[X] = data->scale[X];
	scale[Y] = data->scale[Y];
	scale[Z] = data->scale[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

