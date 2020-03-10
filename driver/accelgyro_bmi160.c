/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI160 accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_bmi_common.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/mag_bmm150.h"
#include "hwtimer.h"
#include "i2c.h"
#include "math_util.h"
#include "motion_sense_fifo.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

STATIC_IF(CONFIG_ACCEL_FIFO) volatile uint32_t last_interrupt_timestamp;

static int wakeup_time[] = {
	[MOTIONSENSE_TYPE_ACCEL] = 4,
	[MOTIONSENSE_TYPE_GYRO] = 80,
	[MOTIONSENSE_TYPE_MAG] = 1
};

#ifdef CONFIG_BMI_SEC_I2C
/**
 * Control access to the compass on the secondary i2c interface:
 * enable values are:
 * 1: manual access, we can issue i2c to the compass
 * 0: data access: BMI160 gather data periodically from the compass.
 */
static int bmi160_sec_access_ctrl(const int port,
				  const uint16_t i2c_spi_addr_flags,
				  const int enable)
{
	int mag_if_ctrl;
	bmi_read8(port, i2c_spi_addr_flags,
		  BMI160_MAG_IF_1, &mag_if_ctrl);
	if (enable) {
		mag_if_ctrl |= BMI160_MAG_MANUAL_EN;
		mag_if_ctrl &= ~BMI160_MAG_READ_BURST_MASK;
		mag_if_ctrl |= BMI160_MAG_READ_BURST_1;
	} else {
		mag_if_ctrl &= ~BMI160_MAG_MANUAL_EN;
		mag_if_ctrl &= ~BMI160_MAG_READ_BURST_MASK;
		mag_if_ctrl |= BMI160_MAG_READ_BURST_8;
	}
	return bmi_write8(port, i2c_spi_addr_flags,
			  BMI160_MAG_IF_1, mag_if_ctrl);
}

/**
 * Read register from compass.
 * Assuming we are in manual access mode, read compass i2c register.
 */
int bmi160_sec_raw_read8(const int port,
			 const uint16_t i2c_spi_addr_flags,
			 const uint8_t reg, int *data_ptr)
{
	/* Only read 1 bytes */
	bmi_write8(port, i2c_spi_addr_flags,
		   BMI160_MAG_I2C_READ_ADDR, reg);
	return bmi_read8(port, i2c_spi_addr_flags,
			 BMI160_MAG_I2C_READ_DATA, data_ptr);
}

/**
 * Write register from compass.
 * Assuming we are in manual access mode, write to compass i2c register.
 */
int bmi160_sec_raw_write8(const int port,
			  const uint16_t i2c_spi_addr_flags,
			  const uint8_t reg, int data)
{
	bmi_write8(port, i2c_spi_addr_flags,
		   BMI160_MAG_I2C_WRITE_DATA, data);
	return bmi_write8(port, i2c_spi_addr_flags,
			  BMI160_MAG_I2C_WRITE_ADDR, reg);
}
#endif

static int enable_fifo(const struct motion_sensor_t *s, int enable)
{
	struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);
	int ret, val;

	if (enable) {
		/* FIFO start collecting events */
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
				BMI160_FIFO_CONFIG_1, &val);
		val |= BMI160_FIFO_SENSOR_EN(s->type);
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_FIFO_CONFIG_1, val);
		if (ret == EC_SUCCESS)
			data->flags |= 1 << (s->type + BMI160_FIFO_FLAG_OFFSET);

	} else {
		/* FIFO stop collecting events */
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
				BMI160_FIFO_CONFIG_1, &val);
		val &= ~BMI160_FIFO_SENSOR_EN(s->type);
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_FIFO_CONFIG_1, val);
		if (ret == EC_SUCCESS)
			data->flags &=
				~(1 << (s->type + BMI160_FIFO_FLAG_OFFSET));
	}
	return ret;
}

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	int ret, val, normalized_rate;
	uint8_t ctrl_reg, reg_val;
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);
#ifdef CONFIG_MAG_BMI_BMM150
	struct mag_cal_t              *moc = BMM150_CAL(s);
#endif

	if (rate == 0) {
		/* FIFO stop collecting events */
		if (IS_ENABLED(CONFIG_ACCEL_FIFO))
			enable_fifo(s, 0);

		/* go to suspend mode */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_CMD_REG,
				 BMI160_CMD_MODE_SUSPEND(s->type));
		msleep(3);
		data->odr = 0;
#ifdef CONFIG_MAG_BMI_BMM150
		if (s->type == MOTIONSENSE_TYPE_MAG)
			moc->batch_size = 0;
#endif
		return ret;
	} else if (data->odr == 0) {
		/* back from suspend mode. */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_CMD_REG,
				 BMI160_CMD_MODE_NORMAL(s->type));
		msleep(wakeup_time[s->type]);
	}
	ctrl_reg = BMI160_CONF_REG(s->type);
	reg_val = BMI_ODR_TO_REG(rate);
	normalized_rate = BMI_REG_TO_ODR(reg_val);
	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate = BMI_REG_TO_ODR(reg_val);
	}

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		if (normalized_rate > BMI160_ACCEL_MAX_FREQ ||
		    normalized_rate < BMI160_ACCEL_MIN_FREQ)
			return EC_RES_INVALID_PARAM;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		if (normalized_rate > BMI160_GYRO_MAX_FREQ ||
		    normalized_rate < BMI160_GYRO_MIN_FREQ)
			return EC_RES_INVALID_PARAM;
		break;
#ifdef CONFIG_MAG_BMI_BMM150
	case MOTIONSENSE_TYPE_MAG:
		/* We use the regular preset we can go about 100Hz */
		if (reg_val > BMI_ODR_100HZ || reg_val < BMI_ODR_0_78HZ)
			return EC_RES_INVALID_PARAM;
		break;
#endif

	default:
		return EC_RES_INVALID_PARAM;
	}

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags, ctrl_reg, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	val = (val & ~BMI160_ODR_MASK) | reg_val;
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags, ctrl_reg, val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	/* Now that we have set the odr, update the driver's value. */
	data->odr = normalized_rate;

#ifdef CONFIG_MAG_BMI_BMM150
	if (s->type == MOTIONSENSE_TYPE_MAG) {
		/* Reset the calibration */
		init_mag_cal(moc);
		/*
		 * We need at least MIN_BATCH_SIZE amd we must have collected
		 * for at least MIN_BATCH_WINDOW_US.
		 * Given odr is in mHz, multiply by 1000x
		 */
		moc->batch_size = MAX(
			MAG_CAL_MIN_BATCH_SIZE,
			(data->odr * 1000) / (MAG_CAL_MIN_BATCH_WINDOW_US));
		CPRINTS("Batch size: %d", moc->batch_size);
	}
#endif

	/*
	 * FIFO start collecting events.
	 * They will be discarded if AP does not want them.
	 */
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		enable_fifo(s, 1);

accel_cleanup:
	mutex_unlock(s->mutex);
	return ret;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

	return data->odr;
}
static int get_offset(const struct motion_sensor_t *s,
			int16_t    *offset,
			int16_t    *temp)
{
	int i, val, val98;
	intv3_t v;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		/*
		 * The offset of the accelerometer off_acc_[xyz] is a 8 bit
		 * two-complement number in units of 3.9 mg independent of the
		 * range selected for the accelerometer.
		 */
		for (i = X; i <= Z; i++) {
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_OFFSET_ACC70 + i, &val);
			if (val > 0x7f)
				val = -256 + val;
			v[i] = round_divide(
				(int64_t)val * BMI160_OFFSET_ACC_MULTI_MG,
				BMI160_OFFSET_ACC_DIV_MG);

		}
		break;
	case MOTIONSENSE_TYPE_GYRO:
		/* Read the MSB first */
		bmi_read8(s->port, s->i2c_spi_addr_flags,
			  BMI160_OFFSET_EN_GYR98, &val98);
		/*
		 * The offset of the gyroscope off_gyr_[xyz] is a 10 bit
		 * two-complement number in units of 0.061 °/s.
		 * Therefore a maximum range that can be compensated is
		 * -31.25 °/s to +31.25 °/s
		 */
		for (i = X; i <= Z; i++) {
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_OFFSET_GYR70 + i, &val);
			val |= ((val98 >> (2 * i)) & 0x3) << 8;
			if (val > 0x1ff)
				val = -1024 + val;
			v[i] = round_divide(
				(int64_t)val * BMI160_OFFSET_GYRO_MULTI_MDS,
				BMI160_OFFSET_GYRO_DIV_MDS);
		}
		break;
#ifdef CONFIG_MAG_BMI_BMM150
	case MOTIONSENSE_TYPE_MAG:
		bmm150_get_offset(s, v);
		break;
#endif /* defined(CONFIG_MAG_BMI160_BMM150) */
	default:
		for (i = X; i <= Z; i++)
			v[i] = 0;
	}
	rotate(v, *s->rot_standard_ref, v);
	offset[X] = v[X];
	offset[Y] = v[Y];
	offset[Z] = v[Z];
	/* Saving temperature at calibration not supported yet */
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	int ret, i, val, val98;
	intv3_t v = { offset[X], offset[Y], offset[Z] };

	rotate_inv(v, *s->rot_standard_ref, v);

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI160_OFFSET_EN_GYR98, &val98);
	if (ret != 0)
		return ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		for (i = X; i <= Z; i++) {
			val = round_divide(
				(int64_t)v[i] * BMI160_OFFSET_ACC_DIV_MG,
				BMI160_OFFSET_ACC_MULTI_MG);
			if (val > 127)
				val = 127;
			if (val < -128)
				val = -128;
			if (val < 0)
				val = 256 + val;
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_OFFSET_ACC70 + i, val);
		}
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_OFFSET_EN_GYR98,
				 val98 | BMI160_OFFSET_ACC_EN);
		break;
	case MOTIONSENSE_TYPE_GYRO:
		for (i = X; i <= Z; i++) {
			val = round_divide(
				(int64_t)v[i] * BMI160_OFFSET_GYRO_DIV_MDS,
				BMI160_OFFSET_GYRO_MULTI_MDS);
			if (val > 511)
				val = 511;
			if (val < -512)
				val = -512;
			if (val < 0)
				val = 1024 + val;
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_OFFSET_GYR70 + i, val & 0xFF);
			val98 &= ~(0x3 << (2 * i));
			val98 |= (val >> 8) << (2 * i);
		}
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_OFFSET_EN_GYR98,
				 val98 | BMI160_OFFSET_GYRO_EN);
		break;
#ifdef CONFIG_MAG_BMI_BMM150
	case MOTIONSENSE_TYPE_MAG:
		ret = bmm150_set_offset(s, v);
		break;
#endif /* defined(CONFIG_MAG_BMI160) */
	default:
		ret = EC_RES_INVALID_PARAM;
	}
	return ret;
}

static int perform_calib(const struct motion_sensor_t *s, int enable)
{
	int ret, val, en_flag, status, rate;
	timestamp_t deadline;

	if (!enable)
		return EC_SUCCESS;

	rate = get_data_rate(s);
	/*
	 * Temporary set frequency to 100Hz to get enough data in a short
	 * period of time.
	 */
	set_data_rate(s, 100000, 0);

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		/* We assume the device is laying flat for calibration */
		if (s->rot_standard_ref == NULL ||
		    (*s->rot_standard_ref)[2][2] > INT_TO_FP(0))
			val = BMI160_FOC_ACC_PLUS_1G;
		else
			val = BMI160_FOC_ACC_MINUS_1G;
		val = (BMI160_FOC_ACC_0G << BMI160_FOC_ACC_X_OFFSET) |
			(BMI160_FOC_ACC_0G << BMI160_FOC_ACC_Y_OFFSET) |
			(val << BMI160_FOC_ACC_Z_OFFSET);
		en_flag = BMI160_OFFSET_ACC_EN;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		val = BMI160_FOC_GYRO_EN;
		en_flag = BMI160_OFFSET_GYRO_EN;
		break;
	default:
		/* Not supported on Magnetometer */
		ret = EC_RES_INVALID_PARAM;
		goto end_perform_calib;
	}
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_FOC_CONF, val);
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_CMD_REG, BMI160_CMD_START_FOC);
	deadline.val = get_time().val + 400 * MSEC;
	do {
		if (timestamp_expired(deadline, NULL)) {
			ret = EC_RES_TIMEOUT;
			goto end_perform_calib;
		}
		msleep(50);
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
				BMI160_STATUS, &status);
		if (ret != EC_SUCCESS)
			goto end_perform_calib;
	} while ((status & BMI160_FOC_RDY) == 0);

	/* Calibration is successful, and loaded, use the result */
	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI160_OFFSET_EN_GYR98, &val);
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_OFFSET_EN_GYR98, val | en_flag);
end_perform_calib:
	set_data_rate(s, rate, 0);
	return ret;
}

/*
 * Manage gesture recognition.
 * Defined even if host interface is not defined, to enable double tap even
 * when the host does not deal with gesture.
 */
int manage_activity(const struct motion_sensor_t *s,
		    enum motionsensor_activity activity,
		    int enable,
		    const struct ec_motion_sense_activity *param)
{
	int ret;
	struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);

	switch (activity) {
#ifdef CONFIG_GESTURE_SIGMO
	case MOTIONSENSE_ACTIVITY_SIG_MOTION: {
		int tmp;
		ret = bmi_read8(s->port, s->addr, BMI160_INT_EN_0, &tmp);
		if (ret)
			return ret;
		if (enable) {
			/* We should use parameters from caller */
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				BMI160_INT_MOTION_3,
				BMI160_MOTION_PROOF_TIME(
					CONFIG_GESTURE_SIGMO_PROOF_MS) <<
				BMI160_MOTION_PROOF_OFF |
				BMI160_MOTION_SKIP_TIME(
					CONFIG_GESTURE_SIGMO_SKIP_MS) <<
				BMI160_MOTION_SKIP_OFF |
				BMI160_MOTION_SIG_MOT_SEL);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				BMI160_INT_MOTION_1,
				BMI160_MOTION_TH(s,
					CONFIG_GESTURE_SIGMO_THRES_MG));
			tmp |= BMI160_INT_ANYMO_X_EN |
				BMI160_INT_ANYMO_Y_EN |
				BMI160_INT_ANYMO_Z_EN;
		} else {
			tmp &= ~(BMI160_INT_ANYMO_X_EN |
				 BMI160_INT_ANYMO_Y_EN |
				 BMI160_INT_ANYMO_Z_EN);
		}
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_INT_EN_0, tmp);
		if (ret)
			ret = EC_RES_UNAVAILABLE;
		break;
	}
#endif
#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
	case MOTIONSENSE_ACTIVITY_DOUBLE_TAP: {
		int tmp;
		/* Set double tap interrupt */
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
				BMI160_INT_EN_0, &tmp);
		if (ret)
			return ret;
		if (enable)
			tmp |= BMI160_INT_D_TAP_EN;
		else
			tmp &= ~BMI160_INT_D_TAP_EN;
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_INT_EN_0, tmp);
		if (ret)
			ret = EC_RES_UNAVAILABLE;
		break;
	}
#endif
	default:
		ret = EC_RES_INVALID_PARAM;
	}
	if (ret == EC_RES_SUCCESS) {
		if (enable) {
			data->enabled_activities |= 1 << activity;
			data->disabled_activities &= ~BIT(activity);
		} else {
			data->enabled_activities &= ~BIT(activity);
			data->disabled_activities |= 1 << activity;
		}
	}
	return ret;
}

#ifdef CONFIG_GESTURE_HOST_DETECTION
int list_activities(const struct motion_sensor_t *s,
		    uint32_t *enabled,
		    uint32_t *disabled)
{
	struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);
	*enabled = data->enabled_activities;
	*disabled = data->disabled_activities;
	return EC_RES_SUCCESS;
}
#endif

#ifdef CONFIG_ACCEL_INTERRUPTS

enum fifo_state {
	FIFO_HEADER,
	FIFO_DATA_SKIP,
	FIFO_DATA_TIME,
	FIFO_DATA_CONFIG,
};


#define BMI160_FIFO_BUFFER 64
static uint8_t bmi160_buffer[BMI160_FIFO_BUFFER];

/**
 * Retrieve hardware FIFO from sensor,
 * - put data in Sensor Hub fifo.
 * - update sensor raw_xyz vector with the last information.
 * We put raw data in hub fifo and process data from there.
 * @s: Pointer to sensor data.
 * @last_ts: The last timestamp of fifo interrupt.
 *
 * Read only up to  bmi160_buffer. If more reads are needed, we will be called
 * again by the interrupt routine.
 *
 * NOTE: If a new driver supports this function, be sure to add a check
 * for spoof_mode in order to load the sensor stack with the spoofed
 * data.  See accelgyro_bmi160.c::load_fifo for an example.
 */
static int load_fifo(struct motion_sensor_t *s, uint32_t last_ts)
{
	struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);
	uint16_t length;
	enum fifo_state state = FIFO_HEADER;
	uint8_t *bp = bmi160_buffer;
	uint8_t *ep;
	uint32_t beginning;


	if (s->type != MOTIONSENSE_TYPE_ACCEL)
		return EC_SUCCESS;

	if (!(data->flags &
	     (BMI160_FIFO_ALL_MASK << BMI160_FIFO_FLAG_OFFSET))) {
		/*
		 * The FIFO was disabled while we were processing it.
		 *
		 * Flush potential left over:
		 * When sensor is resumed, we won't read old data.
		 */
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_REG, BMI160_CMD_FIFO_FLUSH);
		return EC_SUCCESS;
	}

	bmi_read_n(s->port, s->i2c_spi_addr_flags,
		   BMI160_FIFO_LENGTH_0,
		   (uint8_t *)&length, sizeof(length));
	length &= BMI160_FIFO_LENGTH_MASK;

	/*
	 * We have not requested timestamp, no extra frame to read.
	 * if we have too much to read, read the whole buffer.
	 */
	if (length == 0) {
		CPRINTS("unexpected empty FIFO");
		return EC_SUCCESS;
	}

	/* Add one byte to get an empty FIFO frame.*/
	length++;

	if (length > sizeof(bmi160_buffer))
		CPRINTS("unexpected large FIFO: %d", length);
	length = MIN(length, sizeof(bmi160_buffer));


	bmi_read_n(s->port, s->i2c_spi_addr_flags,
		   BMI160_FIFO_DATA, bmi160_buffer, length);
	beginning = *(uint32_t *)bmi160_buffer;
	ep = bmi160_buffer + length;
	/*
	 * FIFO is invalid when reading while the sensors are all
	 * suspended.
	 * Instead of returning the empty frame, it can return a
	 * pattern that looks like a valid header: 84 or 40.
	 * If we see those, assume the sensors have been disabled
	 * while this thread was running.
	 */
	if (beginning == 0x84848484 ||
			(beginning & 0xdcdcdcdc) == 0x40404040) {
		CPRINTS("Suspended FIFO: accel ODR/rate: %d/%d: 0x%08x",
				BASE_ODR(s->config[SENSOR_CONFIG_AP].odr),
				get_data_rate(s),
				beginning);
		return EC_SUCCESS;
	}

	while (bp < ep) {
		switch (state) {
		case FIFO_HEADER: {
			enum fifo_header hdr = *bp++;

			if (bmi_decode_header(s, hdr, last_ts, &bp, ep))
				continue;
			/* Other cases */
			hdr &= 0xdc;
			switch (hdr) {
			case BMI_FH_EMPTY:
				return EC_SUCCESS;
			case BMI_FH_SKIP:
				state = FIFO_DATA_SKIP;
				break;
			case BMI_FH_TIME:
				state = FIFO_DATA_TIME;
				break;
			case BMI_FH_CONFIG:
				state = FIFO_DATA_CONFIG;
				break;
			default:
				CPRINTS("Unknown header: 0x%02x @ %zd",
						hdr, bp - bmi160_buffer);
				bmi_write8(s->port, s->i2c_spi_addr_flags,
						BMI160_CMD_REG,
						BMI160_CMD_FIFO_FLUSH);
				return EC_ERROR_NOT_HANDLED;
			}
			break;
		}
		case FIFO_DATA_SKIP:
			CPRINTS("@ %zd - %d, skipped %d frames",
					bp - bmi160_buffer, length, *bp);
			bp++;
			state = FIFO_HEADER;
			break;
		case FIFO_DATA_CONFIG:
			CPRINTS("@ %zd - %d, config change: 0x%02x",
					bp - bmi160_buffer, length, *bp);
			bp++;
			state = FIFO_HEADER;
			break;
		case FIFO_DATA_TIME:
			if (bp + 3 > ep) {
				bp = ep;
				continue;
			}
			/* We are not requesting timestamp */
			CPRINTS("timestamp %d", (bp[2] << 16) |
					(bp[1] << 8) | bp[0]);
			state = FIFO_HEADER;
			bp += 3;
			break;
		default:
			CPRINTS("Unknown data: 0x%02x", *bp++);
			state = FIFO_HEADER;
		}
	}
	return EC_SUCCESS;
}

/**
 * bmi160_interrupt - called when the sensor activates the interrupt line.
 *
 * This is a "top half" interrupt handler, it just asks motion sense ask
 * to schedule the "bottom half", ->irq_handler().
 */
void bmi160_interrupt(enum gpio_signal signal)
{
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCELGYRO_BMI_INT_EVENT, 0);
}


static int config_interrupt(const struct motion_sensor_t *s)
{
	int ret, tmp;

	if (s->type != MOTIONSENSE_TYPE_ACCEL)
		return EC_SUCCESS;

	mutex_lock(s->mutex);
	bmi_write8(s->port, s->i2c_spi_addr_flags,
		   BMI160_CMD_REG, BMI160_CMD_FIFO_FLUSH);
	bmi_write8(s->port, s->i2c_spi_addr_flags,
		   BMI160_CMD_REG, BMI160_CMD_INT_RESET);

#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
	bmi_write8(s->port, s->i2c_spi_addr_flags,
		   BMI160_INT_TAP_0,
		   BMI160_TAP_DUR(s, CONFIG_GESTURE_TAP_MAX_INTERSTICE_T));
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_INT_TAP_1,
			 BMI160_TAP_TH(s, CONFIG_GESTURE_TAP_THRES_MG));
#endif
#ifdef CONFIG_BMI_ORIENTATION_SENSOR
	/* only use orientation sensor on the lid sensor */
	if (s->location == MOTIONSENSE_LOC_LID) {
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_INT_ORIENT_0,
				 BMI160_INT_ORIENT_0_INIT_VAL);
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_INT_ORIENT_1,
				 BMI160_INT_ORIENT_1_INIT_VAL);
	}
#endif

#ifdef CONFIG_ACCELGYRO_BMI_INT2_OUTPUT
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_INT_LATCH, BMI160_LATCH_5MS);
#else
	/* Also, configure int2 as an external input. */
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_INT_LATCH,
			 BMI160_INT2_INPUT_EN | BMI160_LATCH_5MS);
#endif

	/* configure int1 as an interrupt */
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_INT_OUT_CTRL,
			 BMI160_INT_CTRL(1, OUTPUT_EN));

	/* Map activity interrupt to int 1 */
	tmp = 0;
#ifdef CONFIG_GESTURE_SIGMO
	tmp |= BMI160_INT_ANYMOTION;
#endif
#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
	tmp |= BMI160_INT_D_TAP;
#endif
#ifdef CONFIG_BMI_ORIENTATION_SENSOR
	/* enable orientation interrupt for lid sensor only */
	if (s->location == MOTIONSENSE_LOC_LID)
		tmp |= BMI160_INT_ORIENT;
#endif
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI160_INT_MAP_REG(1), tmp);

	if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
		/* map fifo water mark to int 1 */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_INT_FIFO_MAP,
				 BMI160_INT_MAP(1, FWM) |
				 BMI160_INT_MAP(1, FFULL));

		/*
		 * Configure fifo watermark to int whenever there's any data in
		 * there
		 */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_FIFO_CONFIG_0, 1);
#ifdef CONFIG_ACCELGYRO_BMI_INT2_OUTPUT
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_FIFO_CONFIG_1,
				 BMI160_FIFO_HEADER_EN);
#else
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_FIFO_CONFIG_1,
				 BMI160_FIFO_TAG_INT2_EN |
				 BMI160_FIFO_HEADER_EN);
#endif

		/* Set fifo*/
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
				BMI160_INT_EN_1, &tmp);
		tmp |= BMI160_INT_FWM_EN | BMI160_INT_FFUL_EN;
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI160_INT_EN_1, tmp);
	}
	mutex_unlock(s->mutex);
	return ret;
}

#ifdef CONFIG_BMI_ORIENTATION_SENSOR
static void irq_set_orientation(struct motion_sensor_t *s,
				int interrupt)
{
	int shifted_masked_orientation =
		(interrupt >> 24) & BMI160_ORIENT_XY_MASK;
	if (BMI160_GET_DATA(s)->raw_orientation != shifted_masked_orientation) {
		enum motionsensor_orientation orientation =
			MOTIONSENSE_ORIENTATION_UNKNOWN;

		BMI160_GET_DATA(s)->raw_orientation =
			shifted_masked_orientation;

		switch (shifted_masked_orientation) {
		case BMI160_ORIENT_PORTRAIT:
			orientation = MOTIONSENSE_ORIENTATION_PORTRAIT;
			break;
		case BMI160_ORIENT_PORTRAIT_INVERT:
			orientation =
				MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_PORTRAIT;
			break;
		case BMI160_ORIENT_LANDSCAPE:
			orientation = MOTIONSENSE_ORIENTATION_LANDSCAPE;
			break;
		case BMI160_ORIENT_LANDSCAPE_INVERT:
			orientation =
				MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_LANDSCAPE;
			break;
		default:
			break;
		}
		orientation = motion_sense_remap_orientation(s, orientation);
		SET_ORIENTATION(s, orientation);
	}
}
#endif
/**
 * irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt.
 *
 * For now, we just print out. We should set a bitmask motion sense code will
 * act upon.
 */
static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	uint32_t interrupt;
	int8_t has_read_fifo = 0;
	int rv;

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
			(!(*event & CONFIG_ACCELGYRO_BMI_INT_EVENT)))
		return EC_ERROR_NOT_HANDLED;

	do {
		rv = bmi_read32(s->port, s->i2c_spi_addr_flags,
				BMI160_INT_STATUS_0, &interrupt);
		/*
		 * Bail out of this loop there was an error reading the register
		 */
		if (rv)
			return rv;

#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
		if (interrupt & BMI160_D_TAP_INT) {
			*event |= TASK_EVENT_MOTION_ACTIVITY_INTERRUPT(
					MOTIONSENSE_ACTIVITY_DOUBLE_TAP);
		}
#endif
#ifdef CONFIG_GESTURE_SIGMO
		if (interrupt & BMI160_SIGMOT_INT) {
			*event |= TASK_EVENT_MOTION_ACTIVITY_INTERRUPT(
					MOTIONSENSE_ACTIVITY_SIG_MOTION);
		}
#endif
		if (IS_ENABLED(CONFIG_ACCEL_FIFO) &&
		    interrupt & (BMI160_FWM_INT | BMI160_FFULL_INT)) {
			load_fifo(s, last_interrupt_timestamp);
			has_read_fifo = 1;
		}
#ifdef CONFIG_BMI_ORIENTATION_SENSOR
		irq_set_orientation(s, interrupt);
#endif
	} while (interrupt != 0);

	if (IS_ENABLED(CONFIG_ACCEL_FIFO) && has_read_fifo)
		motion_sense_fifo_commit_data();

	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_INTERRUPTS */


static int read(const struct motion_sensor_t *s, intv3_t v)
{
	uint8_t data[6];
	int ret, status = 0;

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI160_STATUS, &status);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion senor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (!(status & BMI160_DRDY_MASK(s->type))) {
		if (v != s->raw_xyz)
			memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
		return EC_SUCCESS;
	}

	/* Read 6 bytes starting at xyz_reg */
	ret = bmi_read_n(s->port, s->i2c_spi_addr_flags,
			 get_xyz_reg(s->type), data, 6);

	if (ret != EC_SUCCESS) {
		CPRINTS("%s: type:0x%X RD XYZ Error %d", s->name, s->type, ret);
		return ret;
	}
	bmi_normalize(s, v, data);
	return EC_SUCCESS;
}

static int read_temp(const struct motion_sensor_t *s, int *temp_ptr)
{
	return bmi160_get_sensor_temp(s - motion_sensors, temp_ptr);
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp, i;
	struct accelgyro_saved_data_t *saved_data = BMI_GET_SAVED_DATA(s);

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI160_CHIP_ID, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (tmp != BMI160_CHIP_ID_MAJOR && tmp != BMI168_CHIP_ID_MAJOR) {
		/* The device may be lock on paging mode. Try to unlock it. */
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_REG, BMI160_CMD_EXT_MODE_EN_B0);
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_REG, BMI160_CMD_EXT_MODE_EN_B1);
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_REG, BMI160_CMD_EXT_MODE_EN_B2);
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_EXT_MODE_ADDR, BMI160_CMD_PAGING_EN);
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_EXT_MODE_ADDR, 0);
		return EC_ERROR_ACCESS_DENIED;
	}


	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);

		/* Reset the chip to be in a good state */
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_REG, BMI160_CMD_SOFT_RESET);
		msleep(1);
		data->flags &= ~(BMI160_FLAG_SEC_I2C_ENABLED |
				(BMI160_FIFO_ALL_MASK <<
				 BMI160_FIFO_FLAG_OFFSET));
#ifdef CONFIG_GESTURE_HOST_DETECTION
		data->enabled_activities = 0;
		data->disabled_activities = 0;
#ifdef CONFIG_GESTURE_SIGMO
		data->disabled_activities |=
			1 << MOTIONSENSE_ACTIVITY_SIG_MOTION;
#endif
#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
		data->disabled_activities |=
			1 << MOTIONSENSE_ACTIVITY_DOUBLE_TAP;
#endif
#endif
		/* To avoid gyro wakeup */
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_PMU_TRIGGER, 0);
	}

#ifdef CONFIG_BMI_SEC_I2C
	if (s->type == MOTIONSENSE_TYPE_MAG) {
		struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);

		/*
		 * To be able to configure the real magnetometer, we must set
		 * the BMI160 magnetometer part (a pass through) in normal mode.
		 */
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_CMD_REG, BMI160_CMD_MODE_NORMAL(s->type));
		msleep(wakeup_time[s->type]);

		if ((data->flags & BMI160_FLAG_SEC_I2C_ENABLED) == 0) {
			int ext_page_reg, pullup_reg;
			/* Enable secondary interface */
			/*
			 * This is not part of the normal configuration but from
			 * code on Bosh github repo:
			 * https://github.com/BoschSensortec/BMI160_driver
			 *
			 * Magic command sequences
			 */
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_CMD_REG, BMI160_CMD_EXT_MODE_EN_B0);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_CMD_REG, BMI160_CMD_EXT_MODE_EN_B1);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_CMD_REG, BMI160_CMD_EXT_MODE_EN_B2);

			/*
			 * Change the register page to target mode, to change
			 * the internal pull ups of the secondary interface.
			 */
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_CMD_EXT_MODE_ADDR, &ext_page_reg);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_CMD_EXT_MODE_ADDR,
				   ext_page_reg | BMI160_CMD_TARGET_PAGE);
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_CMD_EXT_MODE_ADDR, &ext_page_reg);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_CMD_EXT_MODE_ADDR,
				   ext_page_reg | BMI160_CMD_PAGING_EN);
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_COM_C_TRIM_ADDR, &pullup_reg);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_COM_C_TRIM_ADDR,
				   pullup_reg | BMI160_COM_C_TRIM);
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_CMD_EXT_MODE_ADDR, &ext_page_reg);
			bmi_write8(s->port, s->i2c_spi_addr_flags,
				   BMI160_CMD_EXT_MODE_ADDR,
				   ext_page_reg & ~BMI160_CMD_TARGET_PAGE);
			bmi_read8(s->port, s->i2c_spi_addr_flags,
				  BMI160_CMD_EXT_MODE_ADDR, &ext_page_reg);

			/* Set the i2c address of the compass */
			ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
					 BMI160_MAG_IF_0,
					 I2C_GET_ADDR(
					     CONFIG_ACCELGYRO_SEC_ADDR_FLAGS)
					 << 1);

			/* Enable the secondary interface as I2C */
			ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
					 BMI160_IF_CONF,
					 BMI160_IF_MODE_AUTO_I2C <<
					     BMI160_IF_MODE_OFF);
			data->flags |= BMI160_FLAG_SEC_I2C_ENABLED;
		}


		bmi160_sec_access_ctrl(s->port, s->i2c_spi_addr_flags, 1);

		ret = bmm150_init(s);
		if (ret)
			/* Leave the compass open for tinkering. */
			return ret;

		/* Leave the address for reading the data */
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI160_MAG_I2C_READ_ADDR, BMM150_BASE_DATA);
		/*
		 * Put back the secondary interface in normal mode.
		 * BMI160 will poll based on the configure ODR.
		 */
		bmi160_sec_access_ctrl(s->port, s->i2c_spi_addr_flags, 0);

		/*
		 * Clean interrupt event that may have occurred while the
		 * BMI160 was in management mode.
		 */
		task_set_event(TASK_ID_MOTIONSENSE,
				CONFIG_ACCELGYRO_BMI_INT_EVENT, 0);
	}
#endif

	for (i = X; i <= Z; i++)
		saved_data->scale[i] = MOTION_SENSE_DEFAULT_SCALE;
	/*
	 * The sensor is in Suspend mode at init,
	 * so set data rate to 0.
	 */
	saved_data->odr = 0;
	bmi_set_range(s, s->default_range, 0);

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
#ifdef CONFIG_ACCEL_INTERRUPTS
		ret = config_interrupt(s);
#endif
	}

	return sensor_init_done(s);
}

const struct accelgyro_drv bmi160_drv = {
	.init = init,
	.read = read,
	.set_range = bmi_set_range,
	.get_range = bmi_get_range,
	.get_resolution = bmi_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = set_offset,
	.get_scale = bmi_get_scale,
	.set_scale = bmi_set_scale,
	.get_offset = get_offset,
	.perform_calib = perform_calib,
	.read_temp = read_temp,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif
#ifdef CONFIG_GESTURE_HOST_DETECTION
	.manage_activity = manage_activity,
	.list_activities = list_activities,
#endif
};

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
struct i2c_stress_test_dev bmi160_i2c_stress_test_dev = {
	.reg_info = {
		.read_reg = BMI160_CHIP_ID,
		.read_val = BMI160_CHIP_ID_MAJOR,
		.write_reg = BMI160_PMU_TRIGGER,
	},
	.i2c_read = &bmi_read8,
	.i2c_write = &bmi_write8,
};
#endif /* CONFIG_CMD_I2C_STRESS_TEST_ACCEL */

int bmi160_get_sensor_temp(int idx, int *temp_ptr)
{
	struct motion_sensor_t *s = &motion_sensors[idx];
	int16_t temp;
	int ret;

	ret = bmi_read_n(s->port, s->i2c_spi_addr_flags,
			 BMI160_TEMPERATURE_0,
			 (uint8_t *)&temp, sizeof(temp));

	if (ret || temp == BMI160_INVALID_TEMP)
		return EC_ERROR_NOT_POWERED;

	*temp_ptr = C_TO_K(23 + ((temp + 256) >> 9));
	return 0;
}
