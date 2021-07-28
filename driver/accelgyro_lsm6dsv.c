/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LSM6DSV Accel and Gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 *
 * For any details on driver implementation please
 * Refer to AN5192 Application Note on www.st.com
 */

#include <inttypes.h>
#include "driver/accelgyro_lsm6dsv.h"
#include "hooks.h"
#include "hwtimer.h"
#include "math_util.h"
#include "motion_sense_fifo.h"
#include "task.h"
#include "timer.h"

/* QVAR configuration */
#define LSM6DSV_QVAR_IN_FIFO
#define LSM6DSV_OUT_QVAR_SIZE		2
#define LSM6DSV_QVAR_BATCH_FILT 	0
#define LSM6DSV_QVAR_BATCH_FEAT 	1
#define LSM6DSV_QVAR_BATCH_MODE 	LSM6DSV_QVAR_BATCH_FEAT

#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

STATIC_IF(CONFIG_ACCEL_FIFO) volatile uint32_t last_interrupt_timestamp;
STATIC_IF(CONFIG_ACCEL_INTERRUPTS) int config_interrupt(
		const struct motion_sensor_t *s);

static uint32_t halfbits_to_floatbits(uint16_t h)
{
	uint16_t h_exp, h_sig;
	uint32_t f_sgn, f_exp, f_sig;

	h_exp = (h & 0x7c00u);
	f_sgn = ((uint32_t)h & 0x8000u) << 16;
	switch (h_exp) {
	case 0x0000u:
		/* 0 or subnormal */
		h_sig = (h & 0x03ffu);
		/* Signed zero */
		if (h_sig == 0)
			return f_sgn;
		/* Subnormal */
		h_sig <<= 1;
		while ((h_sig & 0x0400u) == 0) {
			h_sig <<= 1;
			h_exp++;
		}
		f_exp = ((uint32_t)(127 - 15 - h_exp)) << 23;
		f_sig = ((uint32_t)(h_sig & 0x03ffu)) << 13;
		return f_sgn + f_exp + f_sig;
	case 0x7c00u:
		/* inf or NaN */
		/* All-ones exponent and a copy of the significand */
		return f_sgn + 0x7f800000u + (((uint32_t)(h & 0x03ffu)) << 13);
	default:
		/* normalized */
		/* Just need to adjust the exponent and shift */
		return f_sgn + (((uint32_t)(h & 0x7fffu) + 0x1c000u) << 13);
    }
}

/* Converts half float to float */
static float half_to_float(uint16_t h)
{
	union {
		float ret;
		uint32_t retbits;
	} conv;

	conv.retbits = halfbits_to_floatbits(h);

	return conv.ret;
}

/*
 * When ODR change, the sensor filters need settling time;
 * Add a counter to discard a well known number of data with
 * incorrect values.
 */
static uint32_t samples_to_discard[LSM6DSV_FIFO_DEV_NUM];

#ifdef LSM6DSV_QVAR_IN_FIFO
static const uint8_t qvar_fifo_config[][2] = {
/*
 * Machine Learning Core Tool v9.9.3.7_BETA, LSM6DSV
 *
 * <MLC1_SRC>DT1,0='negative',4='positive'
 *
 *  FILTER_IIR1_EXT_X -> 023Eh
 *  FILTER_IIR1_EXT_Y -> 0240h
 *  FILTER_IIR1_EXT_Z -> 0242h
 *  F1_MEAN_on_filter IIR1 on EXT X -> 0244h
 */
	{ 0x05, 0x00 }, { 0x17, 0x40 }, { 0x02, 0x11 }, { 0x08, 0xE8 },
	{ 0x09, 0x00 }, { 0x09, 0x3C }, { 0x09, 0x2A }, { 0x09, 0x02 },
	{ 0x09, 0x44 }, { 0x09, 0x02 }, { 0x09, 0x02 }, { 0x09, 0x00 },
	{ 0x09, 0x14 }, { 0x09, 0x01 },
#if LSM6DSV_QVAR_BATCH_MODE == LSM6DSV_QVAR_BATCH_FILT
	{ 0x09, 0xFF },
#else /* LSM6DSV_QVAR_BATCH_FILT */
	{ 0x09, 0x01 },
#endif /* LSM6DSV_QVAR_BATCH_FEAT */
	{ 0x02, 0x11 }, { 0x08, 0xFA }, { 0x09, 0x14 }, { 0x09, 0x02 },
	{ 0x09, 0x46 }, { 0x09, 0x02 }, { 0x09, 0x52 }, { 0x09, 0x02 },
	{ 0x02, 0x21 }, { 0x08, 0x14 },
#if LSM6DSV_QVAR_BATCH_MODE == LSM6DSV_QVAR_BATCH_FILT
	{ 0x09, 0xA8 },
#else /* LSM6DSV_QVAR_BATCH_FILT */
	{ 0x09, 0x28 },
#endif /* LSM6DSV_QVAR_BATCH_FEAT */
	{ 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 },
	{ 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 },
	{ 0x09, 0x3C }, { 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 },
	{ 0x09, 0x00 }, { 0x09, 0x3F }, { 0x09, 0x00 }, { 0x09, 0x00 },
#if LSM6DSV_QVAR_BATCH_MODE == LSM6DSV_QVAR_BATCH_FEAT
	{ 0x09, 0x2D },
#else /* LSM6DSV_QVAR_BATCH_FEAT */
	{ 0x09, 0x2C },
#endif /* LSM6DSV_QVAR_BATCH_FILT */
	{ 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x1F }, { 0x09, 0x00 },
	{ 0x02, 0x21 }, { 0x08, 0x46 }, { 0x09, 0x00 }, { 0x09, 0x00 },
	{ 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 },
	{ 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x00 },
	{ 0x09, 0x00 }, { 0x17, 0x40 }, { 0x02, 0x21 }, { 0x08, 0x52 },
	{ 0x09, 0x00 }, { 0x09, 0x00 }, { 0x09, 0x40 }, { 0x09, 0xE0 },
	{ 0x17, 0x00 }, { 0x04, 0x00 }, { 0x05, 0x10 }, { 0x02, 0x01 },
	{ 0x60, 0x45 }, { 0x45, 0x02 },
};
#endif /* LSM6DSV_QVAR_IN_FIFO */

static int qvar_init(const struct motion_sensor_t *s)
{
	int err;

#ifdef LSM6DSV_QVAR_IN_FIFO
	uint8_t i;

	err = st_write_data_with_mask(s, LSM6DSV_FUNC_CFG_ACC_ADDR,
				      LSM6DSV_FUNC_CFG_EN_MASK, LSM6DSV_EN_BIT);
	if (err != EC_SUCCESS)
		return err;

	for (i = 0; i < ARRAY_SIZE(qvar_fifo_config); i++) {
		err = st_raw_write8(s->port, s->i2c_spi_addr_flags,
				    qvar_fifo_config[i][0],
				    qvar_fifo_config[i][1]);
		if (err < 0) {
			CPRINTS("Error writing to QVAR configuration");
			break;
		}
	}

	err = st_write_data_with_mask(s, LSM6DSV_FUNC_CFG_ACC_ADDR,
				      LSM6DSV_FUNC_CFG_EN_MASK,
				      LSM6DSV_DIS_BIT);
	if (err != EC_SUCCESS)
		return err;
#endif /* LSM6DSV_QVAR_IN_FIFO */

	/* enable QVAR */
	err = st_write_data_with_mask(s, LSM6DSV_CTRL7_ADDR,
				      LSM6DSV_QVAR_ENABLE_MASK,
				      LSM6DSV_EN_BIT);
	if (err != EC_SUCCESS)
		return err;

	/* impedance selection */
	return st_write_data_with_mask(s, LSM6DSV_CTRL7_ADDR,
					  LSM6DSV_QVAR_C_ZIN_MASK, 3);
}

#ifdef CONFIG_ACCEL_INTERRUPTS
/**
 * Configure interrupt int 1 to fire handler for:
 *
 * FIFO threshold on watermark (1 sample)
 *
 * @s: Motion sensor pointer
 */
static int config_interrupt(const struct motion_sensor_t *s)
{
	int ret = EC_SUCCESS;
	int int1_ctrl_val;

	if (!IS_ENABLED(CONFIG_ACCEL_FIFO))
		return ret;

	ret = st_raw_read8(s->port, s->i2c_spi_addr_flags, LSM6DSV_INT1_CTRL,
			   &int1_ctrl_val);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * Configure FIFO threshold to 1 sample: interrupt on watermark
	 * will be generated every time a new data sample will be stored
	 * in FIFO. The interrupr on watermark is cleared only when the
	 * number or samples still present in FIFO exceeds the
	 * configured threshold.
	 */
	ret = st_raw_write8(s->port, s->i2c_spi_addr_flags,
			    LSM6DSV_FIFO_CTRL1_ADDR, 1);
	if (ret != EC_SUCCESS)
		return ret;

	int1_ctrl_val |= LSM6DSV_INT_FIFO_TH_MASK | LSM6DSV_INT_FIFO_OVR_MASK |
		LSM6DSV_INT_FIFO_FULL_MASK;

	ret = st_raw_write8(s->port, s->i2c_spi_addr_flags, LSM6DSV_INT1_CTRL,
			    int1_ctrl_val);

	return ret;
}

/**
 * fifo_disable - set fifo mode to LSM6DSV_FIFO_MODE_BYPASS_VAL
 * @s: Motion sensor pointer: must be MOTIONSENSE_TYPE_ACCEL.
 */
static int fifo_disable(const struct motion_sensor_t *s)
{
	return st_raw_write8(s->port, s->i2c_spi_addr_flags,
			     LSM6DSV_FIFO_CTRL4_ADDR,
			     LSM6DSV_FIFO_MODE_BYPASS_VAL);
}

/**
 * set_fifo_params - Configure internal FIFO parameters
 *
 * Configure FIFO decimator to have every time the right pattern
 * with acc/gyro
 */
static int fifo_enable(const struct motion_sensor_t *s)
{
	return st_raw_write8(s->port, s->i2c_spi_addr_flags,
			     LSM6DSV_FIFO_CTRL4_ADDR,
			     LSM6DSV_FIFO_MODE_CONT_VAL);
}

static inline int get_id_from_tag(uint8_t tag, enum motionsensor_type *type)
{
	int index;

	switch (tag) {
	case LSM6DSV_ACC_TAG:
		index = 0;
		*type = MOTIONSENSE_TYPE_ACCEL;
		break;
	case LSM6DSV_GYRO_TAG:
		index = 1;
		*type = MOTIONSENSE_TYPE_GYRO;
		break;
#ifdef LSM6DSV_QVAR_IN_FIFO
	case LSM6DSV_QVAR_FILTER_X_TAG:
	case LSM6DSV_QVAR_FEATURE_TAG:
		index = 2;
		*type = MOTIONSENSE_TYPE_PROX;
		break;
#endif /* LSM6DSV_QVAR_IN_FIFO */
	default:
		index = -1;
		*type = -1;
		break;
	}

	return index;
}

/**
 * push_fifo_data - Scan data pattern and push upside
 */
static void push_fifo_data(struct motion_sensor_t *main_s, uint8_t *fifo,
			   uint32_t saved_ts)
{
	struct ec_response_motion_sensor_data vect;
	struct motion_sensor_t *sensor;
	enum motionsensor_type type;
	int *axis, id;
	uint8_t *ptr;
	uint8_t tag;
	int index;

	/*
	 * FIFO pattern is as follow (i.e. Acc/Gyro/Qvar @ same ODR)
	 *  ________ ____________ _______ ____________ _______ ______
	 * | TAG_XL | Acc[x,y,z] | TAG_G | Gyr[x,y,z] | TAG_Q | Qvar |
	 * |________|____________|_______|____________|_______|______|
	 * |<-------- 1 -------->|<-------- 2 ------->|<---- 3 ----->|
	 *
	 * First byte is tag, next data.
	 * Data pattern len is fixed for each sample.
	 * FIFO threshold is related to sample data (7 byte).
	 */
	ptr = fifo + LSM6DSV_TAG_SIZE;
	tag = (*fifo >> 3);
	index = get_id_from_tag(tag, &type);
	if (index < 0)
		return;

	/* Discard samples every ODR changes. */
	if (samples_to_discard[index] > 0) {
		samples_to_discard[index]--;
		return;
	}

	sensor = main_s + index;

	switch(type) {
	case MOTIONSENSE_TYPE_ACCEL:
	case MOTIONSENSE_TYPE_GYRO:
		/* Apply precision, sensitivity and rotation. */
		axis = sensor->raw_xyz;
		st_normalize(sensor, axis, ptr);
		vect.data[X] = axis[X];
		vect.data[Y] = axis[Y];
		vect.data[Z] = axis[Z];

		vect.flags = 0;
		vect.sensor_num = sensor - motion_sensors;
		motion_sense_fifo_stage_data(&vect, sensor, 3, saved_ts);
		break;
#ifdef LSM6DSV_QVAR_IN_FIFO
	case MOTIONSENSE_TYPE_PROX:
		id = (uint16_t)((ptr[3] << 8) + ptr[2]);

#if LSM6DSV_QVAR_BATCH_MODE == LSM6DSV_QVAR_BATCH_FEAT
		if (id == LSM6DSV_QVAR_FEATURE) {
#else
		if (id == LSM6DSV_QVAR_FILTER_X) {
#endif
			vect.data[X] = (int)half_to_float((ptr[1] << 8) + ptr[0]);
			vect.data[Y] = 0;
			vect.data[Z] = 0;

			vect.flags = 0;
			vect.sensor_num = sensor - motion_sensors;
			motion_sense_fifo_stage_data(&vect, sensor, 3, saved_ts);
		}
		break;
#endif /* LSM6DSV_QVAR_IN_FIFO */
	default:
		break;
	}
}

static inline int load_fifo(struct motion_sensor_t *s,
			    const struct lsm6dsv_fstatus *fsts,
			    uint32_t saved_ts)
{
	uint8_t fifo[FIFO_READ_LEN], *ptr;
	int i, err, read_len = 0, word_len, fifo_len;
	uint16_t fifo_depth;

	fifo_depth = fsts->len & LSM6DSV_FIFO_DIFF_MASK;
	fifo_len = fifo_depth * LSM6DSV_FIFO_SAMPLE_SIZE;

	while (read_len < fifo_len) {
		word_len = GENERIC_MIN(fifo_len - read_len, sizeof(fifo));
		err = st_raw_read_n_noinc(s->port, s->i2c_spi_addr_flags,
					  LSM6DSV_FIFO_DATA_ADDR_TAG,
					  fifo, word_len);
		if (err != EC_SUCCESS)
			return err;

		for (i = 0; i < word_len; i += LSM6DSV_FIFO_SAMPLE_SIZE) {
			ptr = &fifo[i];
			push_fifo_data(LSM6DSV_MAIN_SENSOR(s), ptr, saved_ts);
		}
		read_len += word_len;
	}

	return read_len;
}

/**
 * accelgyro_config_fifo - update mode and ODR for FIFO decimator
 */
static int accelgyro_config_fifo(const struct motion_sensor_t *s)
{
	int err;
	struct stprivate_data *data = LSM6DSV_GET_DATA(s);
	uint8_t reg_val;
	uint8_t fifo_odr_mask;

	/* Changing in ODR must stop FIFO. */
	err = fifo_disable(s);
	if (err != EC_SUCCESS)
		return err;

	/*
	 * If ODR changes restore to default discard samples number
	 * the counter related to this sensor.
	 */
	samples_to_discard[s->type] = LSM6DSV_DISCARD_SAMPLES;

	fifo_odr_mask = LSM6DSV_FIFO_ODR_TO_REG(s);
	reg_val = LSM6DSV_ODR_TO_REG(data->base.odr);

	err = st_write_data_with_mask(s, LSM6DSV_FIFO_CTRL3_ADDR,
				      fifo_odr_mask, reg_val);
	if (err != EC_SUCCESS)
		return err;

	return fifo_enable(s);
}

/**
 * lsm6dsv_interrupt - interrupt from int1 pin of sensor
 */
void lsm6dsv_interrupt(enum gpio_signal signal)
{
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE, CONFIG_ACCEL_LSM6DSV_INT_EVENT);
}

/**
 * irq_handler - bottom half of the interrupt task sheduled by consumer
 */
static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	int ret = EC_SUCCESS;
	struct lsm6dsv_fstatus fsts;

	if (((s->type != MOTIONSENSE_TYPE_ACCEL) &&
	     (s->type != MOTIONSENSE_TYPE_GYRO) &&
	     (s->type != MOTIONSENSE_TYPE_PROX)) ||
	    (!(*event & CONFIG_ACCEL_LSM6DSV_INT_EVENT)))
		return EC_ERROR_NOT_HANDLED;

	if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
		/* Read how many data patterns on FIFO to read. */
		ret = st_raw_read_n_noinc(s->port, s->i2c_spi_addr_flags,
					  LSM6DSV_FIFO_STS1_ADDR,
					  (uint8_t *)&fsts, sizeof(fsts));
		if (ret != EC_SUCCESS)
			return ret;

		if (fsts.len & (LSM6DSV_FIFO_DATA_OVR_MASK |
				LSM6DSV_FIFO_FULL_MASK))
			CPRINTS("%s FIFO Overrun: %04x", s->name, fsts.len);

		if (fsts.len & LSM6DSV_FIFO_DIFF_MASK)
			ret = load_fifo(s, &fsts, last_interrupt_timestamp);

		if (IS_ENABLED(CONFIG_ACCEL_FIFO) && ret > 0)
			motion_sense_fifo_commit_data();
	}

	return ret;
}
#endif /* CONFIG_ACCEL_INTERRUPTS */

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 * Note: Range is sensitivity/gain for speed purpose
 */
static int set_range(struct motion_sensor_t *s, int range, int rnd)
{
	int err;
	uint8_t ctrl_reg, reg_val;
	int newrange = range;

	ctrl_reg = LSM6DSV_RANGE_REG(s->type);
	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		/* Adjust and check rounded value for Acc. */
		if (rnd && (newrange < LSM6DSV_ACCEL_NORMALIZE_FS(newrange)))
			newrange *= 2;

		if (newrange > LSM6DSV_ACCEL_FS_MAX_VAL)
			newrange = LSM6DSV_ACCEL_FS_MAX_VAL;

		reg_val = LSM6DSV_XL_FS_REG(newrange);
	} else {
		/* Adjust and check rounded value for Gyro. */
		reg_val = LSM6DSV_GYRO_FS_REG(range);
		if (rnd && (range > LSM6DSV_GYRO_NORMALIZE_FS(reg_val)))
			reg_val++;

		if (reg_val > LSM6DSV_GYRO_FS_MAX_REG_VAL)
			reg_val = LSM6DSV_GYRO_FS_MAX_REG_VAL;

		newrange = LSM6DSV_GYRO_NORMALIZE_FS(reg_val);
	}

	mutex_lock(s->mutex);
	err = st_write_data_with_mask(s, ctrl_reg, LSM6DSV_RANGE_MASK,
				      reg_val);

	if (err == EC_SUCCESS)
		s->current_range = newrange;

	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

/**
 * set_data_rate set sensor data rate
 * @s: Motion sensor pointer
 * @range: Rate (mHz)
 * @rnd: Round up/down flag
 */
static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, normalized_rate = 0;
	struct stprivate_data *data = LSM6DSV_GET_DATA(s);
	uint8_t ctrl_reg, reg_val = 0;

	ctrl_reg = LSM6DSV_ODR_REG(s->type);
	if (rate > 0) {
		reg_val = LSM6DSV_ODR_TO_REG(rate);
		normalized_rate = LSM6DSV_REG_TO_ODR(reg_val);
		if (rnd && (normalized_rate < rate)) {
			reg_val++;
			normalized_rate = LSM6DSV_REG_TO_ODR(reg_val);
		}

		if (normalized_rate < LSM6DSV_ODR_MIN_VAL ||
		    normalized_rate > MIN(LSM6DSV_ODR_MAX_VAL,
					  CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ)) {
			return EC_RES_INVALID_PARAM;
		}
	}

	mutex_lock(s->mutex);

	ret = st_write_data_with_mask(s, ctrl_reg, LSM6DSV_ODR_MASK, reg_val);
	if (ret == EC_SUCCESS) {
		data->base.odr = normalized_rate;
		if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
			accelgyro_config_fifo(s);
		}
	}

	mutex_unlock(s->mutex);

	return ret;
}

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = st_raw_read8(s->port, s->i2c_spi_addr_flags,
			   LSM6DSV_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS) {
		CPRINTS("%s type:0x%X RS Error", s->name, s->type);

		return ret;
	}

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		*ready = (LSM6DSV_STS_XLDA_UP == (tmp & LSM6DSV_STS_XLDA_MASK));
		break;
	case MOTIONSENSE_TYPE_GYRO:
		*ready = (LSM6DSV_STS_GDA_UP == (tmp & LSM6DSV_STS_GDA_MASK));
		break;
	case MOTIONSENSE_TYPE_PROX:
		*ready = (LSM6DSV_STS_QVARDA_UP == (tmp & LSM6DSV_STS_QVARDA_MASK));
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

/*
 * Is not very efficient to collect the data in read: better have an interrupt
 * and collect in FIFO, even if it has one item: we don't have to check if the
 * sensor is ready (minimize I2C access).
 */
static int read(const struct motion_sensor_t *s, intv3_t v)
{
	uint8_t raw[OUT_XYZ_SIZE];
	int ret, tmp = 0;

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

	switch(s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		ret = st_raw_read_n_noinc(s->port, s->i2c_spi_addr_flags,
					  LSM6DSV_ACCEL_OUT_X_L_ADDR,
					  raw, OUT_XYZ_SIZE);
		if (ret != EC_SUCCESS)
			return ret;

		/* Apply precision, sensitivity and rotation vector. */
		st_normalize(s, v, raw);
		break;
	case MOTIONSENSE_TYPE_GYRO:
		ret = st_raw_read_n_noinc(s->port, s->i2c_spi_addr_flags,
					  LSM6DSV_GYRO_OUT_X_L_ADDR,
					  raw, OUT_XYZ_SIZE);
		if (ret != EC_SUCCESS)
			return ret;

		/* Apply precision, sensitivity and rotation vector. */
		st_normalize(s, v, raw);
		break;
	case MOTIONSENSE_TYPE_PROX:
		ret = st_raw_read_n_noinc(s->port, s->i2c_spi_addr_flags,
					  LSM6DSV_QVAR_OUT_ADDR,
					  raw, LSM6DSV_OUT_QVAR_SIZE);
		if (ret != EC_SUCCESS)
			return ret;

		v[0] = (int16_t)(((int16_t)raw[1] << 8) + (int16_t)raw[0]);
		v[1] = v[2] = 0;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return ret;
}

static int init(struct motion_sensor_t *s)
{
	int ret = 0, tmp;
	struct stprivate_data *data = LSM6DSV_GET_DATA(s);

	ret = st_raw_read8(s->port, s->i2c_spi_addr_flags,
			   LSM6DSV_WHO_AM_I_REG, &tmp);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	if (tmp != LSM6DSV_WHO_AM_I)
		return EC_ERROR_ACCESS_DENIED;

	/*
	 * This sensor can be powered through an EC reboot, so the state of the
	 * sensor is unknown here so reset it
	 * LSM6DSV supports both Acc & Gyro features
	 * Board will see two virtual sensor devices: Acc & Gyro
	 * Requirement: Acc need be init before Gyro
	 */

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		mutex_lock(s->mutex);

		/* Software reset. */
		ret = st_raw_write8(s->port, s->i2c_spi_addr_flags,
				    LSM6DSV_CTRL3_ADDR, LSM6DSV_SW_RESET_MASK);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		/*
		 * Output data not updated until have been read.
		 * Require interrupt to be active low.
		 */
		ret = st_raw_write8(s->port, s->i2c_spi_addr_flags,
				    LSM6DSV_CTRL3_ADDR,
				    LSM6DSV_BDU_MASK | LSM6DSV_IF_INC_MASK);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
			ret = fifo_disable(s);
			if (ret != EC_SUCCESS)
				goto err_unlock;
		}

		if (IS_ENABLED(CONFIG_ACCEL_INTERRUPTS)) {
			ret = config_interrupt(s);
			if (ret != EC_SUCCESS)
				goto err_unlock;
		}

		qvar_init(s);

		s->current_range = 2;
		mutex_unlock(s->mutex);
	}

	/* Set default resolution common to Acc and Gyro. */
	data->resol = LSM6DSV_RESOLUTION;

	return sensor_init_done(s);

err_unlock:
	mutex_unlock(s->mutex);
	CPRINTS("%s: MS Init type:0x%X Error", s->name, s->type);

	return ret;
}

const struct accelgyro_drv lsm6dsv_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_resolution = st_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = st_get_data_rate,
	.set_offset = st_set_offset,
	.get_offset = st_get_offset,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif /* CONFIG_ACCEL_INTERRUPTS */
};
