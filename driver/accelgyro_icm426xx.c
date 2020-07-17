/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * ICM-426xx accelerometer and gyroscope module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "console.h"
#include "driver/accelgyro_icm_common.h"
#include "driver/accelgyro_icm426xx.h"
#include "hwtimer.h"
#include "i2c.h"
#include "math_util.h"
#include "motion_sense.h"
#include "motion_sense_fifo.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

STATIC_IF(CONFIG_ACCEL_FIFO) volatile uint32_t last_interrupt_timestamp;

/* list of supported chips */
static const struct icm_chip_desc supported_chips[] = {
	{
		.whoami = ICM426XX_CHIP_ICM42605,
		.name = "icm42605",
	},
};

/* chip serial bus configuration */
struct icm426xx_serial_bus {
	uint8_t intf_config6;
	uint8_t i3c_bus_mode;
	uint8_t drive_config;
};

#ifdef I2C_PORT_ACCEL
static const struct icm426xx_serial_bus i2c_bus_conf = {
	.intf_config6 = ICM426XX_I3C_EN,
	.i3c_bus_mode = 0,
	.drive_config = ICM426XX_I2C_SLEW_RATE(ICM426XX_SLEW_RATE_12NS_36NS) |
			ICM426XX_SPI_SLEW_RATE(ICM426XX_SLEW_RATE_12NS_36NS),
};
#endif

#ifdef CONFIG_SPI_ACCEL_PORT
static const struct icm426xx_serial_bus spi_bus_conf = {
	.intf_config6 = ICM426XX_I3C_EN | ICM426XX_I3C_SDR_EN |
			ICM426XX_I3C_DDR_EN,
	.i3c_bus_mode = ICM426XX_I3C_BUS_MODE,
	.drive_config = ICM426XX_I2C_SLEW_RATE(ICM426XX_SLEW_RATE_20NS_60NS) |
			ICM426XX_SPI_SLEW_RATE(ICM426XX_SLEW_RATE_INF_2NS),
};
#endif

/* fs is in +/- g */
static const struct icm_param_pair accel_fs[] = {
	{
		.val = 2,
		.reg_val = ICM426XX_ACCEL_FS_2G,
	}, {
		.val = 4,
		.reg_val = ICM426XX_ACCEL_FS_4G,
	}, {
		.val = 8,
		.reg_val = ICM426XX_ACCEL_FS_8G,
	}, {
		.val = 16,
		.reg_val = ICM426XX_ACCEL_FS_16G,
	}
};

/* fs is in +/- dps */
static const struct icm_param_pair gyro_fs[] = {
	{
		.val = 16,
		.reg_val = ICM426XX_GYRO_FS_15_625DPS,
	}, {
		.val = 31,
		.reg_val = ICM426XX_GYRO_FS_31_25DPS,
	}, {
		.val = 63,
		.reg_val = ICM426XX_GYRO_FS_62_5DPS,
	}, {
		.val = 125,
		.reg_val = ICM426XX_GYRO_FS_125DPS,
	}, {
		.val = 250,
		.reg_val = ICM426XX_GYRO_FS_250DPS,
	}, {
		.val = 500,
		.reg_val = ICM426XX_GYRO_FS_500DPS,
	}, {
		.val = 1000,
		.reg_val = ICM426XX_GYRO_FS_1000DPS,
	}, {
		.val = 2000,
		.reg_val = ICM426XX_GYRO_FS_2000DPS,
	}
};

/* Low-power rates exprimed in milli-hertz */
static const struct icm_param_pair low_power_rates[] = {
	{
		.val = 1563,
		.reg_val = ICM426XX_ODR_1_5625HZ_LP,
	}, {
		.val = 3125,
		.reg_val = ICM426XX_ODR_3_125HZ_LP,
	}, {
		.val = 6250,
		.reg_val = ICM426XX_ODR_6_25HZ_LP,
	}, {
		.val = 12500,
		.reg_val = ICM426XX_ODR_12_5HZ,
	}, {
		.val = 25000,
		.reg_val = ICM426XX_ODR_25HZ,
	}, {
		.val = 50000,
		.reg_val = ICM426XX_ODR_50HZ,
	}, {
		.val = 100000,
		.reg_val = ICM426XX_ODR_100HZ,
	}, {
		.val = 200000,
		.reg_val = ICM426XX_ODR_200HZ,
	}, {
		.val = 500000,
		.reg_val = ICM426XX_ODR_500HZ,
	},
};

/* Low-noise rates exprimed in milli-hertz */
static const struct icm_param_pair low_noise_rates[] = {
	{
		.val = 12500,
		.reg_val = ICM426XX_ODR_12_5HZ,
	}, {
		.val = 25000,
		.reg_val = ICM426XX_ODR_25HZ,
	}, {
		.val = 50000,
		.reg_val = ICM426XX_ODR_50HZ,
	}, {
		.val = 100000,
		.reg_val = ICM426XX_ODR_100HZ,
	}, {
		.val = 200000,
		.reg_val = ICM426XX_ODR_200HZ,
	}, {
		.val = 500000,
		.reg_val = ICM426XX_ODR_500HZ,
	}, {
		.val = 1000000,
		.reg_val = ICM426XX_ODR_1KHZ_LN,
	}, {
		.val = 2000000,
		.reg_val = ICM426XX_ODR_2KHZ_LN,
	}, {
		.val = 4000000,
		.reg_val = ICM426XX_ODR_4KHZ_LN,
	}
};

struct icm426xx_sensor_conf {
	enum icm426xx_sensor_mode mode;
	enum icm426xx_filter_bw filter;
	unsigned int startup_time_ms;
	unsigned int stop_time_ms;
	const struct icm_param_pair *fs;
	unsigned int fs_len;
	const struct icm_param_pair *rates;
	unsigned int rates_len;
};

static const struct icm426xx_sensor_conf sensors_conf[] = {
	[MOTIONSENSE_TYPE_ACCEL] = {
		.mode = ICM426XX_SENSOR_MODE_LOW_POWER,
		.filter = ICM426XX_FILTER_BW_AVG_16X,
		.startup_time_ms = 20,
		.stop_time_ms = 0,
		.fs = accel_fs,
		.fs_len = ARRAY_SIZE(accel_fs),
		.rates = low_power_rates,
		.rates_len = ARRAY_SIZE(low_power_rates),
	},
	[MOTIONSENSE_TYPE_GYRO] = {
		.mode = ICM426XX_SENSOR_MODE_LOW_NOISE,
		.filter = ICM426XX_FILTER_BW_ODR_DIV_2,
		.startup_time_ms = 60,
		.stop_time_ms = 150,
		.fs = gyro_fs,
		.fs_len = ARRAY_SIZE(gyro_fs),
		.rates = low_noise_rates,
		.rates_len = ARRAY_SIZE(low_noise_rates),
	},
};

static int icm426xx_normalize(const struct motion_sensor_t *s, intv3_t v,
		     const uint8_t *raw)
{
	struct accelgyro_saved_data_t *data = ICM_GET_SAVED_DATA(s);
	int i;

	/* sensor data is configured as little-endian */
	v[X] = (int16_t)UINT16_FROM_BYTE_ARRAY_LE(raw, 0);
	v[Y] = (int16_t)UINT16_FROM_BYTE_ARRAY_LE(raw, 2);
	v[Z] = (int16_t)UINT16_FROM_BYTE_ARRAY_LE(raw, 4);

	/* check if data is valid */
	if (v[X] == ICM426XX_INVALID_DATA &&
	    v[Y] == ICM426XX_INVALID_DATA &&
	    v[Z] == ICM426XX_INVALID_DATA) {
		return EC_ERROR_INVAL;
	}

	rotate(v, *s->rot_standard_ref, v);

	for (i = X; i <= Z; i++)
		v[i] = SENSOR_APPLY_SCALE(v[i], data->scale[i]);

	return EC_SUCCESS;
}

/* use FIFO threshold interrupt on INT1 */
#define ICM426XX_FIFO_INT_EN		ICM426XX_FIFO_THS_INT1_EN
#define ICM426XX_FIFO_INT_STATUS	ICM426XX_FIFO_THS_INT

static int __maybe_unused enable_fifo(const struct motion_sensor_t *s,
		int enable)
{
	int val, ret;

	if (enable) {
		/* enable FIFO interrupts */
		ret = icm_field_update8(s, ICM426XX_REG_INT_SOURCE0,
					ICM426XX_FIFO_INT_EN,
					ICM426XX_FIFO_INT_EN);
		if (ret != EC_SUCCESS)
			return ret;

		/* flush FIFO data */
		ret = icm_write8(s, ICM426XX_REG_SIGNAL_PATH_RESET,
				 ICM426XX_FIFO_FLUSH);
		if (ret != EC_SUCCESS)
			return ret;

		/* set FIFO in streaming mode */
		ret = icm_write8(s, ICM426XX_REG_FIFO_CONFIG,
				 ICM426XX_FIFO_MODE_STREAM);
		if (ret != EC_SUCCESS)
			return ret;

		/* workaround: first read of FIFO count is always 0 */
		ret = icm_read16(s, ICM426XX_REG_FIFO_COUNT, &val);
		if (ret != EC_SUCCESS)
			return ret;
	} else {
		/* set FIFO in bypass mode */
		ret = icm_write8(s, ICM426XX_REG_FIFO_CONFIG,
				 ICM426XX_FIFO_MODE_BYPASS);
		if (ret != EC_SUCCESS)
			return ret;

		/* flush FIFO data */
		ret = icm_write8(s, ICM426XX_REG_SIGNAL_PATH_RESET,
				 ICM426XX_FIFO_FLUSH);
		if (ret != EC_SUCCESS)
			return ret;

		/* disable FIFO interrupts */
		ret = icm_field_update8(s, ICM426XX_REG_INT_SOURCE0,
					ICM426XX_FIFO_INT_EN, 0);
		if (ret != EC_SUCCESS)
			return ret;
	}

	return EC_SUCCESS;
}

static int __maybe_unused config_fifo(const struct motion_sensor_t *s,
		int enable)
{
	struct icm_drv_data_t *st = ICM_GET_DATA(s);
	int mask, val;
	uint8_t old_fifo_en;
	int ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		mask = ICM426XX_FIFO_ACCEL_EN;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		mask = ICM426XX_FIFO_GYRO_EN;
		break;
	default:
		return EC_ERROR_INVAL;
	}
	/* temperature data has to be always present in the FIFO */
	mask |= ICM426XX_FIFO_TEMP_EN;

	val = enable ? mask : 0;

	mutex_lock(s->mutex);

	ret = icm_field_update8(s, ICM426XX_REG_FIFO_CONFIG1, mask, val);
	if (ret != EC_SUCCESS)
		goto out_unlock;

	old_fifo_en = st->fifo_en;
	if (enable)
		st->fifo_en |= BIT(s->type);
	else
		st->fifo_en &= ~BIT(s->type);

	if (!old_fifo_en && st->fifo_en) {
		/* 1st sensor enabled => turn FIFO on */
		ret = enable_fifo(s, 1);
		if (ret != EC_SUCCESS)
			goto out_unlock;
	} else if (old_fifo_en && !st->fifo_en) {
		/* last sensor disabled => turn FIFO off */
		ret = enable_fifo(s, 0);
		if (ret != EC_SUCCESS)
			goto out_unlock;
	}

out_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

static void __maybe_unused push_fifo_data(struct motion_sensor_t *s,
					  const uint8_t *raw, uint32_t ts)
{
	intv3_t v;
	struct ec_response_motion_sensor_data vect;
	int ret;

	if (s == NULL)
		return;

	ret = icm426xx_normalize(s, v, raw);
	if (ret == EC_SUCCESS) {
		vect.data[X] = v[X];
		vect.data[Y] = v[Y];
		vect.data[Z] = v[Z];
		vect.flags = 0;
		vect.sensor_num = s - motion_sensors;
		motion_sense_fifo_stage_data(&vect, s, 3, ts);
	}
}

static int __maybe_unused load_fifo(struct motion_sensor_t *s, uint32_t ts)
{
	struct icm_drv_data_t *st = ICM_GET_DATA(s);
	int count, i, size;
	const uint8_t *accel, *gyro;
	int ret;

	ret = icm_read16(s, ICM426XX_REG_FIFO_COUNT, &count);
	if (ret != EC_SUCCESS)
		return ret;

	if (count <= 0)
		return EC_ERROR_INVAL;

	/* flush FIFO if buffer is not large enough */
	if (count > ICM_FIFO_BUFFER) {
		CPRINTS("It should not happen, the EC is too slow for the ODR");
		ret = icm_write8(s, ICM426XX_REG_SIGNAL_PATH_RESET,
				 ICM426XX_FIFO_FLUSH);
		if (ret != EC_SUCCESS)
			return ret;
		return EC_ERROR_OVERFLOW;
	}

	ret = icm_read_n(s, ICM426XX_REG_FIFO_DATA, st->fifo_buffer, count);
	if (ret != EC_SUCCESS)
		return ret;

	for (i = 0; i < count; i += size) {
		size = icm_fifo_decode_packet(&st->fifo_buffer[i],
				&accel, &gyro);
		/* exit if error or FIFO is empty */
		if (size <= 0)
			return -size;
		if (accel != NULL)
			push_fifo_data(st->accel, accel, ts);
		if (gyro != NULL)
			push_fifo_data(st->gyro, gyro, ts);
	}

	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS

/**
 * icm426xx_interrupt - called when the sensor activates the interrupt line.
 *
 * This is a "top half" interrupt handler, it just asks motion sense ask
 * to schedule the "bottom half", ->irq_handler().
 */
void icm426xx_interrupt(enum gpio_signal signal)
{
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCELGYRO_ICM426XX_INT_EVENT, 0);
}

/**
 * irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt.
 */
static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	int status;
	int ret;

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
	    (!(*event & CONFIG_ACCELGYRO_ICM426XX_INT_EVENT)))
		return EC_ERROR_NOT_HANDLED;

	mutex_lock(s->mutex);

	/* read and clear interrupt status */
	ret = icm_read8(s, ICM426XX_REG_INT_STATUS, &status);
	if (ret != EC_SUCCESS)
		goto out_unlock;

	if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
		if (status & ICM426XX_FIFO_INT_STATUS) {
			ret = load_fifo(s, last_interrupt_timestamp);
			if (ret == EC_SUCCESS)
				motion_sense_fifo_commit_data();
		}
	}

out_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

static int config_interrupt(const struct motion_sensor_t *s)
{
	struct icm_drv_data_t *st = ICM_GET_DATA(s);
	int val, ret;

	/* configure INT1 pin */
	val = ICM426XX_INT1_PUSH_PULL;
	if (s->flags & MOTIONSENSE_FLAG_INT_ACTIVE_HIGH)
		val |= ICM426XX_INT1_ACTIVE_HIGH;

	ret = icm_write8(s, ICM426XX_REG_INT_CONFIG, val);
	if (ret != EC_SUCCESS)
		return ret;

	/* deassert async reset for proper INT pin operation */
	ret = icm_field_update8(s, ICM426XX_REG_INT_CONFIG1,
				ICM426XX_INT_ASYNC_RESET, 0);
	if (ret != EC_SUCCESS)
		return ret;

	if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
		/*
		 * configure FIFO:
		 * - enable FIFO partial read
		 * - enable continuous watermark interrupt
		 * - disable all FIFO en bits
		 */
		val = ICM426XX_FIFO_PARTIAL_READ | ICM426XX_FIFO_WM_GT_TH;
		ret = icm_field_update8(s, ICM426XX_REG_FIFO_CONFIG1,
					GENMASK(6, 5) | ICM426XX_FIFO_EN_MASK,
					val);
		if (ret != EC_SUCCESS)
			return ret;

		/* clear internal FIFO enable bits tracking */
		st->fifo_en = 0;

		/* set FIFO watermark to 1 data packet (8 bytes) */
		ret = icm_write16(s, ICM426XX_REG_FIFO_WATERMARK, 8);
		if (ret != EC_SUCCESS)
			return ret;
	}

	return ret;
}

#endif	/* CONFIG_ACCEL_INTERRUPTS */

static int enable_sensor(const struct motion_sensor_t *s, int enable)
{
	const struct icm426xx_sensor_conf *conf = &sensors_conf[s->type];
	unsigned int sleep;
	uint8_t mask, val;
	int ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		mask = ICM426XX_ACCEL_MODE_MASK;
		if (enable)
			val = ICM426XX_ACCEL_MODE(conf->mode);
		else
			val = ICM426XX_ACCEL_MODE(ICM426XX_SENSOR_MODE_OFF);
		break;
	case MOTIONSENSE_TYPE_GYRO:
		mask = ICM426XX_GYRO_MODE_MASK;
		if (enable)
			val = ICM426XX_GYRO_MODE(conf->mode);
		else
			val = ICM426XX_GYRO_MODE(ICM426XX_SENSOR_MODE_OFF);
		break;
	default:
		return -EC_ERROR_INVAL;
	}

	sleep = enable ? conf->startup_time_ms : conf->stop_time_ms;

	mutex_lock(s->mutex);

	ret = icm_field_update8(s, ICM426XX_REG_PWR_MGMT0, mask, val);
	/* when turning sensor on block any register write for 200 us */
	if (ret == EC_SUCCESS && enable)
		usleep(200);

	mutex_unlock(s->mutex);

	if (ret != EC_SUCCESS)
		return ret;

	if (sleep)
		msleep(sleep);

	return EC_SUCCESS;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	const struct icm426xx_sensor_conf *conf = &sensors_conf[s->type];
	struct accelgyro_saved_data_t *data = ICM_GET_SAVED_DATA(s);
	const struct icm_param_pair *p;
	int reg, ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		reg = ICM426XX_REG_ACCEL_CONFIG0;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		reg = ICM426XX_REG_GYRO_CONFIG0;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	if (rate == 0) {
		/* disable data in FIFO */
		if (IS_ENABLED(CONFIG_ACCEL_FIFO))
			config_fifo(s, 0);
		/* disable sensor */
		ret = enable_sensor(s, 0);
		data->odr = 0;
		return ret;
	} else if (data->odr == 0) {
		/* enable sensor */
		ret = enable_sensor(s, 1);
		if (ret)
			return ret;
	}

	p = icm_get_pair(rate, rnd, conf->rates, conf->rates_len);

	mutex_lock(s->mutex);

	ret = icm_field_update8(s, reg, ICM426XX_ODR_MASK,
				ICM426XX_ODR(p->reg_val));
	if (ret != EC_SUCCESS)
		goto out_unlock;

	data->odr = p->val;

	mutex_unlock(s->mutex);

	/* enable data in FIFO */
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		config_fifo(s, 1);

	return EC_SUCCESS;

out_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	const struct icm426xx_sensor_conf *conf = &sensors_conf[s->type];
	struct accelgyro_saved_data_t *data = ICM_GET_SAVED_DATA(s);
	const struct icm_param_pair *p;
	int reg, ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		reg = ICM426XX_REG_ACCEL_CONFIG0;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		reg = ICM426XX_REG_GYRO_CONFIG0;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	p = icm_get_pair(range, rnd, conf->fs, conf->fs_len);

	mutex_lock(s->mutex);

	ret = icm_field_update8(s, reg, ICM426XX_FS_MASK,
				ICM426XX_FS_SEL(p->reg_val));
	if (ret == EC_SUCCESS)
		data->range = p->val;

	mutex_unlock(s->mutex);

	return ret;
}

static int get_hw_offset(const struct motion_sensor_t *s, intv3_t offset)
{
	uint8_t raw[5];
	int i, ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		mutex_lock(s->mutex);
		ret = icm_read_n(s, ICM426XX_REG_OFFSET_USER4,
				raw, sizeof(raw));
		mutex_unlock(s->mutex);
		if (ret != EC_SUCCESS)
			return ret;
		/*
		 * raw[0]: Accel X[11:8] | gyro Z[11:8]
		 * raw[1]: Accel X[0:7]
		 * raw[2]: Accel Y[7:0]
		 * raw[3]: Accel Z[11:8] | Accel Y[11:8]
		 * raw[4]: Accel Z[7:0]
		 */
		offset[X] = (((int)raw[0] << 4) & ~GENMASK(7, 0)) | raw[1];
		offset[Y] = (((int)raw[3] << 8) & ~GENMASK(7, 0)) | raw[2];
		offset[Z] = (((int)raw[3] << 4) & ~GENMASK(7, 0)) | raw[4];
		break;
	case MOTIONSENSE_TYPE_GYRO:
		mutex_lock(s->mutex);
		ret = icm_read_n(s, ICM426XX_REG_OFFSET_USER0,
				raw, sizeof(raw));
		mutex_unlock(s->mutex);
		if (ret != EC_SUCCESS)
			return ret;
		/*
		 * raw[0]: Gyro X[7:0]
		 * raw[1]: Gyro Y[11:8] | Gyro X[11:8]
		 * raw[2]: Gyro Y[7:0]
		 * raw[3]: Gyro Z[7:0]
		 * raw[4]: Accel X[11:8] | gyro Z[11:8]
		 */
		offset[X] = (((int)raw[1] << 8) & ~GENMASK(7, 0)) | raw[0];
		offset[Y] = (((int)raw[1] << 4) & ~GENMASK(7, 0)) | raw[2];
		offset[Z] = (((int)raw[4] << 8) & ~GENMASK(7, 0)) | raw[3];
		break;
	default:
		return EC_ERROR_INVAL;
	}

	/* Extend sign-bit of 12 bits signed values */
	for (i = X; i <= Z; ++i)
		offset[i] = sign_extend(offset[i], 11);

	return EC_SUCCESS;
}

static int set_hw_offset(const struct motion_sensor_t *s, intv3_t offset)
{
	int i, val, ret;

	/* value is 12 bits signed maximum */
	for (i = X; i <= Z; ++i) {
		if (offset[i] > 2047)
			offset[i] = 2047;
		else if (offset[i] < -2048)
			offset[i] = -2048;
	}

	mutex_lock(s->mutex);

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		/* Accel X[11:8] | gyro Z[11:8] */
		val = (offset[X] >> 4) & GENMASK(7, 4);
		ret = icm_field_update8(s, ICM426XX_REG_OFFSET_USER4,
					GENMASK(7, 4), val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Accel X[7:0] */
		val = offset[X] & GENMASK(7, 0);
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER5, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Accel Y[7:0] */
		val = offset[Y] & GENMASK(7, 0);
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER6, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Accel Z[11:8] | Accel Y[11:8] */
		val = ((offset[Z] >> 4) & GENMASK(7, 4)) |
		      ((offset[Y] >> 8) & GENMASK(3, 0));
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER7, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Accel Z[7:0] */
		val = offset[Z] & GENMASK(7, 0);
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER8, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;
		break;

	case MOTIONSENSE_TYPE_GYRO:
		/* Gyro X[7:0] */
		val = offset[X] & GENMASK(7, 0);
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER0, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Gyro Y[11:8] | Gyro X[11:8] */
		val = ((offset[Y] >> 4) & GENMASK(7, 4)) |
		      ((offset[X] >> 8) & GENMASK(3, 0));
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER1, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Gyro Y[7:0] */
		val = offset[Y] & GENMASK(7, 0);
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER2, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Gyro Z[7:0] */
		val = offset[Z] & GENMASK(7, 0);
		ret = icm_write8(s, ICM426XX_REG_OFFSET_USER3, val);
		if (ret != EC_SUCCESS)
			goto out_unlock;

		/* Accel X[11:8] | gyro Z[11:8] */
		val = (offset[Z] >> 8) & GENMASK(3, 0);
		ret = icm_field_update8(s, ICM426XX_REG_OFFSET_USER4,
					GENMASK(3, 0), val);
		if (ret != EC_SUCCESS)
			goto out_unlock;
		break;

	default:
		ret = EC_ERROR_INVAL;
		break;
	}

out_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

static int set_offset(const struct motion_sensor_t *s, const int16_t *offset,
		      int16_t temp)
{
	struct accelgyro_saved_data_t *data = ICM_GET_SAVED_DATA(s);
	intv3_t v = { offset[X], offset[Y], offset[Z] };
	int range, div1, div2;
	int i;

	/* unscale values and rotate back to chip frame */
	for (i = X; i <= Z; ++i)
		v[i] = SENSOR_APPLY_DIV_SCALE(v[i], data->scale[i]);
	rotate_inv(v, *s->rot_standard_ref, v);

	/* convert raw data to hardware offset units */
	range = icm_get_range(s);
	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		/* hardware offset is 1/2048g by LSB */
		div1 = range * 2048;
		div2 = 32768;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		/* hardware offset is 1/32dps by LSB */
		div1 = range * 32;
		div2 = 32768;
		break;
	default:
		return EC_ERROR_INVAL;
	}
	for (i = X; i <= Z; ++i)
		v[i] = round_divide(v[i] * div1, div2);

	return set_hw_offset(s, v);
}

static int get_offset(const struct motion_sensor_t *s, int16_t *offset,
		      int16_t *temp)
{
	struct accelgyro_saved_data_t *data = ICM_GET_SAVED_DATA(s);
	intv3_t v;
	int range, div1, div2;
	int i, ret;

	ret = get_hw_offset(s, v);
	if (ret != EC_SUCCESS)
		return ret;

	/* transform offset to raw data */
	range = icm_get_range(s);
	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		/* hardware offset is 1/2048g by LSB */
		div1 = 32768;
		div2 = 2048 * range;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		/* hardware offset is 1/32dps by LSB */
		div1 = 32768;
		div2 = 32 * range;
		break;
	default:
		return EC_ERROR_INVAL;
	}
	for (i = X; i <= Z; ++i)
		v[i] = round_divide(v[i] * div1, div2);

	rotate(v, *s->rot_standard_ref, v);
	for (i = X; i <= Z; i++)
		v[i] = SENSOR_APPLY_SCALE(v[i], data->scale[i]);

	offset[X] = v[X];
	offset[Y] = v[Y];
	offset[Z] = v[Z];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, intv3_t v)
{
	uint8_t raw[6];
	int reg, ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		reg = ICM426XX_REG_ACCEL_DATA_XYZ;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		reg = ICM426XX_REG_GYRO_DATA_XYZ;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	/* read data registers */
	mutex_lock(s->mutex);
	ret = icm_read_n(s, reg, raw, sizeof(raw));
	mutex_unlock(s->mutex);
	if (ret != EC_SUCCESS)
		return ret;

	ret = icm426xx_normalize(s, v, raw);
	/* if data is invalid return the previous read data */
	if (ret != EC_SUCCESS) {
		if (v != s->raw_xyz)
			memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
	}

	return EC_SUCCESS;
}

static int read_temp(const struct motion_sensor_t *s, int *temp_ptr)
{
	int val, ret;

	mutex_lock(s->mutex);
	ret = icm_read16(s, ICM426XX_REG_TEMP_DATA, &val);
	mutex_unlock(s->mutex);
	if (ret != EC_SUCCESS)
		return ret;

	/* ensure correct propagation of 16 bits sign bit */
	val = sign_extend(val, 15);

	if (val == ICM426XX_INVALID_DATA)
		return EC_ERROR_NOT_POWERED;

	*temp_ptr = C_TO_K((val * 100) / 13248 + 25);
	return EC_SUCCESS;
}

static int init_config(const struct motion_sensor_t *s)
{
	const struct icm426xx_serial_bus *serial_bus_conf;
	uint8_t mask, val;
	int ret;

	if (SLAVE_IS_SPI(s->i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		serial_bus_conf = &spi_bus_conf;
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		serial_bus_conf = &i2c_bus_conf;
#endif
	}

	/*
	 * serial bus setup (i2c or spi)
	 *
	 * Do not check result for INTF_CONFIG6, since it can induce
	 * interferences on the bus.
	 */
	icm_field_update8(s, ICM426XX_REG_INTF_CONFIG6,
			  ICM426XX_INTF_CONFIG6_MASK,
			  serial_bus_conf->intf_config6);

	ret = icm_field_update8(s, ICM426XX_REG_INTF_CONFIG4,
				ICM426XX_I3C_BUS_MODE,
				serial_bus_conf->i3c_bus_mode);
	if (ret)
		return ret;

	ret = icm_field_update8(s, ICM426XX_REG_DRIVE_CONFIG,
				ICM426XX_DRIVE_CONFIG_MASK,
				serial_bus_conf->drive_config);
	if (ret)
		return ret;

	/*
	 * Use invalid value in registers and FIFO.
	 * Data registers in little-endian format.
	 * Disable unused serial interface.
	 */
	mask = ICM426XX_DATA_CONF_MASK | ICM426XX_UI_SIFS_CFG_MASK;
	val = 0;
	if (SLAVE_IS_SPI(s->i2c_spi_addr_flags)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		val |= ICM426XX_UI_SIFS_CFG_I2C_DIS;
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		val |= ICM426XX_UI_SIFS_CFG_SPI_DIS;
#endif
	}

	return icm_field_update8(s, ICM426XX_REG_INTF_CONFIG0, mask, val);
}

static int init(const struct motion_sensor_t *s)
{
	const struct icm426xx_sensor_conf *conf = &sensors_conf[s->type];
	struct icm_drv_data_t *st = ICM_GET_DATA(s);
	struct accelgyro_saved_data_t *saved_data = ICM_GET_SAVED_DATA(s);
	int mask, val;
	int ret, i;

	mutex_lock(s->mutex);

	/* manually force register bank to 0 */
	st->bank = 0;
	ret = icm_write8(s, ICM426XX_REG_BANK_SEL, ICM426XX_BANK_SEL(0));
	if (ret)
		goto out_unlock;

	/* detect chip using whoami */
	ret = icm_read8(s, ICM426XX_REG_WHO_AM_I, &val);
	if (ret)
		goto out_unlock;

	for (i = 0; i < ARRAY_SIZE(supported_chips); ++i) {
		const struct icm_chip_desc *chip = &supported_chips[i];

		if (val == chip->whoami) {
			CPRINTS("%s chip detected", chip->name);
			st->chip = chip;
			break;
		}
	}
	if (i >= ARRAY_SIZE(supported_chips)) {
		ret = EC_ERROR_ACCESS_DENIED;
		goto out_unlock;
	}

	/* first time init done only for 1st sensor (accel) */
	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		/* reset the chip and verify it is ready */
		ret = icm_write8(s, ICM426XX_REG_DEVICE_CONFIG,
				 ICM426XX_SOFT_RESET_CONFIG);
		if (ret)
			goto out_unlock;
		msleep(1);

		ret = icm_read8(s, ICM426XX_REG_INT_STATUS, &val);
		if (ret)
			goto out_unlock;
		if (!(val & ICM426XX_RESET_DONE_INT)) {
			ret = EC_ERROR_ACCESS_DENIED;
			goto out_unlock;
		}

		/* configure sensor */
		ret = init_config(s);
		if (ret)
			goto out_unlock;

#ifdef CONFIG_ACCEL_INTERRUPTS
		ret = config_interrupt(s);
		if (ret)
			goto out_unlock;
#endif
	}

	for (i = X; i <= Z; i++)
		saved_data->scale[i] = MOTION_SENSE_DEFAULT_SCALE;
	/*
	 * The sensor is in Suspend mode at init,
	 * so set data rate to 0.
	 */
	saved_data->odr = 0;

	/* set sensor filter */
	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		mask = ICM426XX_ACCEL_UI_FILT_MASK;
		val = ICM426XX_ACCEL_UI_FILT_BW(conf->filter);
		if (IS_ENABLED(CONFIG_ACCEL_FIFO))
			st->accel = (struct motion_sensor_t *)s;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		mask = ICM426XX_GYRO_UI_FILT_MASK;
		val = ICM426XX_GYRO_UI_FILT_BW(conf->filter);
		if (IS_ENABLED(CONFIG_ACCEL_FIFO))
			st->gyro = (struct motion_sensor_t *)s;
		break;
	default:
		ret = EC_ERROR_INVAL;
		goto out_unlock;
	}
	ret = icm_field_update8(s, ICM426XX_REG_GYRO_ACCEL_CONFIG0, mask, val);
	if (ret != EC_SUCCESS)
		goto out_unlock;

	mutex_unlock(s->mutex);

	return sensor_init_done(s);

out_unlock:
	mutex_unlock(s->mutex);
	return ret;
}

const struct accelgyro_drv icm426xx_drv = {
	.init = init,
	.read = read,
	.read_temp = read_temp,
	.set_range = set_range,
	.get_range = icm_get_range,
	.get_resolution = icm_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = icm_get_data_rate,
	.set_offset = set_offset,
	.get_offset = get_offset,
	.set_scale = icm_set_scale,
	.get_scale = icm_get_scale,
	.perform_calib = NULL,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif
};
