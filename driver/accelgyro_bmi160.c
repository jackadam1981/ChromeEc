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
#include "hooks.h"
#include "hwtimer.h"
#include "i2c.h"
#include "math_util.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

#ifdef CONFIG_ACCEL_FIFO
static volatile uint32_t last_interrupt_timestamp;
#endif

static int wakeup_time[] = {
	[MOTIONSENSE_TYPE_ACCEL] = 4,
	[MOTIONSENSE_TYPE_GYRO] = 80,
	[MOTIONSENSE_TYPE_MAG] = 1
};

static inline int get_xyz_reg(enum motionsensor_type type)
{
	switch (type) {
	case MOTIONSENSE_TYPE_ACCEL:
		return BMI160_ACC_X_L_G;
	case MOTIONSENSE_TYPE_GYRO:
		return BMI160_GYR_X_L_G;
	case MOTIONSENSE_TYPE_MAG:
		return BMI160_MAG_X_L_G;
	default:
		return -1;
	}
}


#ifdef CONFIG_SPI_ACCEL_PORT
static inline int spi_raw_read(const int addr, const uint8_t reg,
			       uint8_t *data, const int len)
{
	uint8_t cmd = 0x80 | reg;

	return spi_transaction(&spi_devices[addr], &cmd, 1, data, len);
}
#endif
/**
 * Read 8bit register from accelerometer.
 */
static int raw_read8(const int port, const int addr, const int reg,
					 int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;

	if (BMI160_IS_SPI(addr)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		uint8_t val;
		rv = spi_raw_read(BMI160_SPI_ADDRESS(addr), reg, &val, 1);
		if (rv == EC_SUCCESS)
			*data_ptr = val;
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read8(port, BMI160_I2C_ADDRESS(addr),
			       reg, data_ptr);
#endif
	}
	return rv;
}

/**
 * Write 8bit register from accelerometer.
 */
static int raw_write8(const int port, const int addr, const int reg,
					  int data)
{
	int rv = -EC_ERROR_PARAM1;

	if (BMI160_IS_SPI(addr)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		uint8_t cmd[2] = { reg, data };
		rv = spi_transaction(&spi_devices[BMI160_SPI_ADDRESS(addr)],
				     cmd, 2, NULL, 0);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_write8(port, BMI160_I2C_ADDRESS(addr),
				reg, data);
#endif
	}
	/*
	 * From Bosch:  BMI160 needs a delay of 450us after each write if it
	 * is in suspend mode, otherwise the operation may be ignored by
	 * the sensor. Given we are only doing write during init, add
	 * the delay inconditionally.
	 */
	msleep(1);
	return rv;
}

#ifdef CONFIG_ACCEL_INTERRUPTS
/**
 * Read 32bit register from accelerometer.
 */
static int raw_read32(const int port, const int addr, const uint8_t reg,
					  int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;
	if (BMI160_IS_SPI(addr)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		rv = spi_raw_read(BMI160_SPI_ADDRESS(addr), reg,
				  (uint8_t *)data_ptr, 4);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read32(port, BMI160_I2C_ADDRESS(addr),
				reg, data_ptr);
#endif
	}
	return rv;
}
#endif /* defined(CONFIG_ACCEL_INTERRUPTS) */

/**
 * Read n bytes from accelerometer.
 */
static int raw_read_n(const int port, const int addr, const uint8_t reg,
		uint8_t *data_ptr, const int len)
{
	int rv = -EC_ERROR_PARAM1;

	if (BMI160_IS_SPI(addr)) {
#ifdef CONFIG_SPI_ACCEL_PORT
		rv = spi_raw_read(BMI160_SPI_ADDRESS(addr), reg, data_ptr, len);
#endif
	} else {
#ifdef I2C_PORT_ACCEL
		rv = i2c_read_block(port, BMI160_I2C_ADDRESS(addr), reg,
				data_ptr, len);
#endif
	}
	return rv;
}

#ifdef CONFIG_BMI160_SEC_I2C
/**
 * Control access to the compass on the secondary i2c interface:
 * enable values are:
 * 1: manual access, we can issue i2c to the compass
 * 0: data access: BMI160 gather data periodically from the compass.
 */
static int bmi160_sec_access_ctrl(const int port, const int addr,
				  const int enable)
{
	int mag_if_ctrl;
	raw_read8(port, addr, BMI160_MAG_IF_1, &mag_if_ctrl);
	if (enable) {
		mag_if_ctrl |= BMI160_MAG_MANUAL_EN;
		mag_if_ctrl &= ~BMI160_MAG_READ_BURST_MASK;
		mag_if_ctrl |= BMI160_MAG_READ_BURST_1;
	} else {
		mag_if_ctrl &= ~BMI160_MAG_MANUAL_EN;
		mag_if_ctrl &= ~BMI160_MAG_READ_BURST_MASK;
		mag_if_ctrl |= BMI160_MAG_READ_BURST_8;
	}
	return raw_write8(port, addr, BMI160_MAG_IF_1, mag_if_ctrl);
}

/**
 * Read register from compass.
 * Assuming we are in manual access mode, read compass i2c register.
 */
int bmi160_sec_raw_read8(const int port, const int addr, const uint8_t reg,
				  int *data_ptr)
{
	/* Only read 1 bytes */
	raw_write8(port, addr, BMI160_MAG_I2C_READ_ADDR, reg);
	return raw_read8(port, addr, BMI160_MAG_I2C_READ_DATA, data_ptr);
}

/**
 * Write register from compass.
 * Assuming we are in manual access mode, write to compass i2c register.
 */
int bmi160_sec_raw_write8(const int port, const int addr, const uint8_t reg,
			  int data)
{
	raw_write8(port, addr, BMI160_MAG_I2C_WRITE_DATA, data);
	return raw_write8(port, addr, BMI160_MAG_I2C_WRITE_ADDR, reg);
}
#endif

#ifdef CONFIG_ACCEL_FIFO
static int enable_fifo(const struct motion_sensor_t *s, int enable)
{
	struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);
	int ret, val;

	if (enable) {
		/* FIFO start collecting events */
		ret = raw_read8(s->port, s->addr, BMI160_FIFO_CONFIG_1, &val);
		val |= BMI160_FIFO_SENSOR_EN(s->type);
		ret = raw_write8(s->port, s->addr, BMI160_FIFO_CONFIG_1, val);
		if (ret == EC_SUCCESS)
			data->flags |= 1 << (s->type + BMI160_FIFO_FLAG_OFFSET);

	} else {
		/* FIFO stop collecting events */
		ret = raw_read8(s->port, s->addr, BMI160_FIFO_CONFIG_1, &val);
		val &= ~BMI160_FIFO_SENSOR_EN(s->type);
		ret = raw_write8(s->port, s->addr, BMI160_FIFO_CONFIG_1, val);
		if (ret == EC_SUCCESS)
			data->flags &=
				~(1 << (s->type + BMI160_FIFO_FLAG_OFFSET));
	}
	return ret;
}
#endif

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	int ret, val, normalized_rate;
	uint8_t ctrl_reg, reg_val;
	struct accelgyro_saved_data_t *data = BMI160_GET_SAVED_DATA(s);
#ifdef CONFIG_MAG_BMI160_BMM150
	struct mag_cal_t              *moc = BMM150_CAL(s);
#endif

	if (rate == 0) {
#ifdef CONFIG_ACCEL_FIFO
		/* FIFO stop collecting events */
		enable_fifo(s, 0);
#endif
		/* go to suspend mode */
		ret = raw_write8(s->port, s->addr, BMI160_CMD_REG,
				 BMI160_CMD_MODE_SUSPEND(s->type));
		msleep(3);
		data->odr = 0;
#ifdef CONFIG_MAG_BMI160_BMM150
		if (s->type == MOTIONSENSE_TYPE_MAG)
			moc->batch_size = 0;
#endif
		return ret;
	} else if (data->odr == 0) {
		/* back from suspend mode. */
		ret = raw_write8(s->port, s->addr, BMI160_CMD_REG,
				 BMI160_CMD_MODE_NORMAL(s->type));
		msleep(wakeup_time[s->type]);
	}
	ctrl_reg = BMI160_CONF_REG(s->type);
	reg_val = BMI160_ODR_TO_REG(rate);
	normalized_rate = BMI160_REG_TO_ODR(reg_val);
	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate = BMI160_REG_TO_ODR(reg_val);
	}

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		if (normalized_rate > MIN(BMI160_ACCEL_MAX_FREQ,
					CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ) ||
		    normalized_rate < BMI160_ACCEL_MIN_FREQ)
			return EC_RES_INVALID_PARAM;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		if (normalized_rate > MIN(BMI160_GYRO_MAX_FREQ,
					CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ) ||
		    normalized_rate < BMI160_GYRO_MIN_FREQ)
			return EC_RES_INVALID_PARAM;
		break;
#ifdef CONFIG_MAG_BMI160_BMM150
	case MOTIONSENSE_TYPE_MAG:
		/* We use the regular preset we can go about 100Hz */
		if (reg_val > BMI160_ODR_100HZ || reg_val < BMI160_ODR_0_78HZ)
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

	ret = raw_read8(s->port, s->addr, ctrl_reg, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	val = (val & ~BMI160_ODR_MASK) | reg_val;
	ret = raw_write8(s->port, s->addr, ctrl_reg, val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	/* Now that we have set the odr, update the driver's value. */
	data->odr = normalized_rate;

#ifdef CONFIG_MAG_BMI160_BMM150
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

#ifdef CONFIG_ACCEL_FIFO
	/*
	 * FIFO start collecting events.
	 * They will be discarded if AP does not want them.
	 */
	enable_fifo(s, 1);
#endif

accel_cleanup:
	mutex_unlock(s->mutex);
	return ret;
}

static int set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	int ret, i, val, val98;
	intv3_t v = { offset[X], offset[Y], offset[Z] };

	rotate_inv(v, *s->rot_standard_ref, v);

	ret = raw_read8(s->port, s->addr, BMI160_OFFSET_EN_GYR98, &val98);
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
			raw_write8(s->port, s->addr, BMI160_OFFSET_ACC70 + i,
				   val);
		}
		ret = raw_write8(s->port, s->addr, BMI160_OFFSET_EN_GYR98,
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
			raw_write8(s->port, s->addr, BMI160_OFFSET_GYR70 + i,
					val & 0xFF);
			val98 &= ~(0x3 << (2 * i));
			val98 |= (val >> 8) << (2 * i);
		}
		ret = raw_write8(s->port, s->addr, BMI160_OFFSET_EN_GYR98,
				 val98 | BMI160_OFFSET_GYRO_EN);
		break;
#ifdef CONFIG_MAG_BMI160_BMM150
	case MOTIONSENSE_TYPE_MAG:
		ret = bmm150_set_offset(s, v);
		break;
#endif /* defined(CONFIG_MAG_BMI160) */
	default:
		ret = EC_RES_INVALID_PARAM;
	}
	return ret;
}

static int perform_calib(const struct motion_sensor_t *s)
{
	int ret, val, en_flag, status, rate;
	timestamp_t deadline;

	rate = bmi_get_data_rate(s);
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
	ret = raw_write8(s->port, s->addr, BMI160_FOC_CONF, val);
	ret = raw_write8(s->port, s->addr, BMI160_CMD_REG,
			 BMI160_CMD_START_FOC);
	deadline.val = get_time().val + 400 * MSEC;
	do {
		if (timestamp_expired(deadline, NULL)) {
			ret = EC_RES_TIMEOUT;
			goto end_perform_calib;
		}
		msleep(50);
		ret = raw_read8(s->port, s->addr, BMI160_STATUS, &status);
		if (ret != EC_SUCCESS)
			goto end_perform_calib;
	} while ((status & BMI160_FOC_RDY) == 0);

	/* Calibration is successful, and loaded, use the result */
	ret = raw_read8(s->port, s->addr, BMI160_OFFSET_EN_GYR98, &val);
	ret = raw_write8(s->port, s->addr, BMI160_OFFSET_EN_GYR98,
			 val | en_flag);
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
		ret = raw_read8(s->port, s->addr, BMI160_INT_EN_0, &tmp);
		if (ret)
			return ret;
		if (enable) {
			/* We should use parameters from caller */
			raw_write8(s->port, s->addr, BMI160_INT_MOTION_3,
				BMI160_MOTION_PROOF_TIME(
					CONFIG_GESTURE_SIGMO_PROOF_MS) <<
				BMI160_MOTION_PROOF_OFF |
				BMI160_MOTION_SKIP_TIME(
					CONFIG_GESTURE_SIGMO_SKIP_MS) <<
				BMI160_MOTION_SKIP_OFF |
				BMI160_MOTION_SIG_MOT_SEL);
			raw_write8(s->port, s->addr, BMI160_INT_MOTION_1,
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
		ret = raw_write8(s->port, s->addr, BMI160_INT_EN_0, tmp);
		if (ret)
			ret = EC_RES_UNAVAILABLE;
		break;
	}
#endif
#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	case MOTIONSENSE_ACTIVITY_DOUBLE_TAP: {
		int tmp;
		/* Set double tap interrupt */
		ret = raw_read8(s->port, s->addr, BMI160_INT_EN_0, &tmp);
		if (ret)
			return ret;
		if (enable)
			tmp |= BMI160_INT_D_TAP_EN;
		else
			tmp &= ~BMI160_INT_D_TAP_EN;
		ret = raw_write8(s->port, s->addr, BMI160_INT_EN_0, tmp);
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
			data->disabled_activities &= ~(1 << activity);
		} else {
			data->enabled_activities &= ~(1 << activity);
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

#ifdef CONFIG_ACCEL_FIFO
enum fifo_state {
	FIFO_HEADER,
	FIFO_DATA_SKIP,
	FIFO_DATA_TIME,
	FIFO_DATA_CONFIG,
};

#endif  /* CONFIG_ACCEL_FIFO */

/**
 * bmi160_interrupt - called when the sensor activates the interrupt line.
 *
 * This is a "top half" interrupt handler, it just asks motion sense ask
 * to schedule the "bottom half", ->irq_handler().
 */
void bmi160_interrupt(enum gpio_signal signal)
{
#ifdef CONFIG_ACCEL_FIFO
	last_interrupt_timestamp = __hw_clock_source_read();
#endif
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCELGYRO_BMI160_INT_EVENT, 0);
}


static int config_interrupt(const struct motion_sensor_t *s)
{
	int ret, tmp;

	if (s->type != MOTIONSENSE_TYPE_ACCEL)
		return EC_SUCCESS;

	mutex_lock(s->mutex);
	raw_write8(s->port, s->addr, BMI160_CMD_REG, BMI160_CMD_FIFO_FLUSH);
	raw_write8(s->port, s->addr, BMI160_CMD_REG, BMI160_CMD_INT_RESET);

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	raw_write8(s->port, s->addr, BMI160_INT_TAP_0,
		BMI160_TAP_DUR(s, CONFIG_GESTURE_TAP_MAX_INTERSTICE_T));
	ret = raw_write8(s->port, s->addr, BMI160_INT_TAP_1,
		BMI160_TAP_TH(s, CONFIG_GESTURE_TAP_THRES_MG));
#endif
#ifdef CONFIG_BMI160_ORIENTATION_SENSOR
	/* only use orientation sensor on the lid sensor */
	if (s->location == MOTIONSENSE_LOC_LID) {
		ret = raw_write8(s->port, s->addr, BMI160_INT_ORIENT_0,
			BMI160_INT_ORIENT_0_INIT_VAL);
		ret = raw_write8(s->port, s->addr, BMI160_INT_ORIENT_1,
			BMI160_INT_ORIENT_1_INIT_VAL);
	}
#endif

#ifdef CONFIG_ACCELGYRO_BMI160_INT2_OUTPUT
	ret = raw_write8(s->port, s->addr, BMI160_INT_LATCH, BMI160_LATCH_5MS);
#else
	/* Also, configure int2 as an external input. */
	ret = raw_write8(s->port, s->addr, BMI160_INT_LATCH,
		BMI160_INT2_INPUT_EN | BMI160_LATCH_5MS);
#endif

	/* configure int1 as an interrupt */
	ret = raw_write8(s->port, s->addr, BMI160_INT_OUT_CTRL,
		BMI160_INT_CTRL(1, OUTPUT_EN));

	/* Map activity interrupt to int 1 */
	tmp = 0;
#ifdef CONFIG_GESTURE_SIGMO
	tmp |= BMI160_INT_ANYMOTION;
#endif
#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	tmp |= BMI160_INT_D_TAP;
#endif
#ifdef CONFIG_BMI160_ORIENTATION_SENSOR
	/* enable orientation interrupt for lid sensor only */
	if (s->location == MOTIONSENSE_LOC_LID)
		tmp |= BMI160_INT_ORIENT;
#endif
	ret = raw_write8(s->port, s->addr, BMI160_INT_MAP_REG(1), tmp);

#ifdef CONFIG_ACCEL_FIFO
	/* map fifo water mark to int 1 */
	ret = raw_write8(s->port, s->addr, BMI160_INT_FIFO_MAP,
			BMI160_INT_MAP(1, FWM) |
			BMI160_INT_MAP(1, FFULL));

	/* configure fifo watermark to int whenever there's any data in there */
	ret = raw_write8(s->port, s->addr, BMI160_FIFO_CONFIG_0, 1);
#ifdef CONFIG_ACCELGYRO_BMI160_INT2_OUTPUT
	ret = raw_write8(s->port, s->addr, BMI160_FIFO_CONFIG_1,
			BMI160_FIFO_HEADER_EN);
#else
	ret = raw_write8(s->port, s->addr, BMI160_FIFO_CONFIG_1,
			BMI160_FIFO_TAG_INT2_EN |
			BMI160_FIFO_HEADER_EN);
#endif

	/* Set fifo*/
	ret = raw_read8(s->port, s->addr, BMI160_INT_EN_1, &tmp);
	tmp |= BMI160_INT_FWM_EN | BMI160_INT_FFUL_EN;
	ret = raw_write8(s->port, s->addr, BMI160_INT_EN_1, tmp);
#endif
	mutex_unlock(s->mutex);
	return ret;
}

#ifdef CONFIG_BMI160_ORIENTATION_SENSOR
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
	int rv;

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
			(!(*event & CONFIG_ACCELGYRO_BMI160_INT_EVENT)))
		return EC_ERROR_NOT_HANDLED;

	do {
		rv = raw_read32(s->port, s->addr, BMI160_INT_STATUS_0,
				&interrupt);
		/*
		 * Bail out of this loop there was an error reading the register
		 */
		if (rv)
			return rv;

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
		if (interrupt & BMI160_D_TAP_INT)
			*event |= TASK_EVENT_MOTION_ACTIVITY_INTERRUPT(
					MOTIONSENSE_ACTIVITY_DOUBLE_TAP);
#endif
#ifdef CONFIG_GESTURE_SIGMO
		if (interrupt & BMI160_SIGMOT_INT)
			*event |= TASK_EVENT_MOTION_ACTIVITY_INTERRUPT(
					MOTIONSENSE_ACTIVITY_SIG_MOTION);
#endif
#ifdef CONFIG_ACCEL_FIFO
		if (interrupt & (BMI160_FWM_INT | BMI160_FFULL_INT))
			bmi_load_fifo(s, last_interrupt_timestamp);
#endif
#ifdef CONFIG_BMI160_ORIENTATION_SENSOR
		irq_set_orientation(s, interrupt);
#endif
	} while (interrupt != 0);

	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_INTERRUPTS */

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;
	struct accelgyro_saved_data_t *data = BMI160_GET_SAVED_DATA(s);

	ret = raw_read8(s->port, s->addr, BMI160_CHIP_ID, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (tmp != BMI160_CHIP_ID_MAJOR && tmp != BMI168_CHIP_ID_MAJOR) {
		/* The device may be lock on paging mode. Try to unlock it. */
		raw_write8(s->port, s->addr, BMI160_CMD_REG,
				BMI160_CMD_EXT_MODE_EN_B0);
		raw_write8(s->port, s->addr, BMI160_CMD_REG,
				BMI160_CMD_EXT_MODE_EN_B1);
		raw_write8(s->port, s->addr, BMI160_CMD_REG,
				BMI160_CMD_EXT_MODE_EN_B2);
		raw_write8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
				BMI160_CMD_PAGING_EN);
		raw_write8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR, 0);
		return EC_ERROR_ACCESS_DENIED;
	}


	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);

		/* Reset the chip to be in a good state */
		raw_write8(s->port, s->addr, BMI160_CMD_REG,
				BMI160_CMD_SOFT_RESET);
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
#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
		data->disabled_activities |=
			1 << MOTIONSENSE_ACTIVITY_DOUBLE_TAP;
#endif
#endif
		/* To avoid gyro wakeup */
		raw_write8(s->port, s->addr, BMI160_PMU_TRIGGER, 0);
	}

#ifdef CONFIG_BMI160_SEC_I2C
	if (s->type == MOTIONSENSE_TYPE_MAG) {
		struct bmi160_drv_data_t *data = BMI160_GET_DATA(s);

		/*
		 * To be able to configure the real magnetometer, we must set
		 * the BMI160 magnetometer part (a pass through) in normal mode.
		 */
		raw_write8(s->port, s->addr, BMI160_CMD_REG,
				BMI160_CMD_MODE_NORMAL(s->type));
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
			raw_write8(s->port, s->addr, BMI160_CMD_REG,
					BMI160_CMD_EXT_MODE_EN_B0);
			raw_write8(s->port, s->addr, BMI160_CMD_REG,
					BMI160_CMD_EXT_MODE_EN_B1);
			raw_write8(s->port, s->addr, BMI160_CMD_REG,
					BMI160_CMD_EXT_MODE_EN_B2);

			/*
			 * Change the register page to target mode, to change
			 * the internal pull ups of the secondary interface.
			 */
			raw_read8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					&ext_page_reg);
			raw_write8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					ext_page_reg | BMI160_CMD_TARGET_PAGE);
			raw_read8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					&ext_page_reg);
			raw_write8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					ext_page_reg | BMI160_CMD_PAGING_EN);
			raw_read8(s->port, s->addr, BMI160_COM_C_TRIM_ADDR,
					&pullup_reg);
			raw_write8(s->port, s->addr, BMI160_COM_C_TRIM_ADDR,
					pullup_reg | BMI160_COM_C_TRIM);
			raw_read8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					&ext_page_reg);
			raw_write8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					ext_page_reg & ~BMI160_CMD_TARGET_PAGE);
			raw_read8(s->port, s->addr, BMI160_CMD_EXT_MODE_ADDR,
					&ext_page_reg);

			/* Set the i2c address of the compass */
			ret = raw_write8(s->port, s->addr, BMI160_MAG_IF_0,
					CONFIG_ACCELGYRO_SEC_ADDR);

			/* Enable the secondary interface as I2C */
			ret = raw_write8(s->port, s->addr, BMI160_IF_CONF,
				BMI160_IF_MODE_AUTO_I2C << BMI160_IF_MODE_OFF);
			data->flags |= BMI160_FLAG_SEC_I2C_ENABLED;
		}


		bmi160_sec_access_ctrl(s->port, s->addr, 1);

		ret = bmm150_init(s);
		if (ret)
			/* Leave the compass open for tinkering. */
			return ret;

		/* Leave the address for reading the data */
		raw_write8(s->port, s->addr, BMI160_MAG_I2C_READ_ADDR,
				BMM150_BASE_DATA);
		/*
		 * Put back the secondary interface in normal mode.
		 * BMI160 will poll based on the configure ODR.
		 */
		bmi160_sec_access_ctrl(s->port, s->addr, 0);
	}
#endif

	/*
	 * The sensor is in Suspend mode at init,
	 * so set data rate to 0.
	 */
	data->odr = 0;
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
	.read = bmi_read,
	.set_range = bmi_set_range,
	.get_range = bmi_get_range,
	.get_resolution = bmi_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = bmi_get_data_rate,
	.set_offset = set_offset,
	.get_scale = bmi_get_scale,
	.set_scale = bmi_set_scale,
	.get_offset = bmi_get_offset,
	.perform_calib = perform_calib,
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
	.i2c_read = &raw_read8,
	.i2c_write = &raw_write8,
};
#endif /* CONFIG_CMD_I2C_STRESS_TEST_ACCEL */

int bmi160_get_sensor_temp(int idx, int *temp_ptr)
{
	struct motion_sensor_t *s = &motion_sensors[idx];
	int16_t temp;
	int ret;

	ret = raw_read_n(s->port, s->addr, BMI160_TEMPERATURE_0,
			 (uint8_t *)&temp, sizeof(temp));

	if (ret || temp == BMI160_INVALID_TEMP)
		return EC_ERROR_NOT_POWERED;

	*temp_ptr = C_TO_K(23 + ((temp + 256) >> 9));
	return 0;
}