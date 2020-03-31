/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI260 accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "console.h"
#include "driver/accelgyro_bmi_common.h"
#include "driver/accelgyro_bmi260_config_tbin.h"
#include "driver/accelgyro_bmi260.h"
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

/*
 * The gyro start-up time is 45ms in normal mode
 *                            2ms in fast start-up mode
 */
static int wakeup_time[] = {
	[MOTIONSENSE_TYPE_ACCEL] = 2,
	[MOTIONSENSE_TYPE_GYRO] = 45,
	[MOTIONSENSE_TYPE_MAG] = 1
};

static int enable_sensor(const struct motion_sensor_t *s, int enable)
{
	int ret;

	bmi_enable_reg8(s, BMI260_PWR_CTRL,
			BMI260_PWR_EN(s->type),
			enable);
	if (s->type == MOTIONSENSE_TYPE_GYRO) {
		/* switch to performance mode */
		ret = bmi_enable_reg8(s, BMI_CONF_REG(s->type),
				      BMI260_FILTER_PERF |
				      BMI260_GYR_NOISE_PERF,
				      enable);
	} else {
		ret = bmi_enable_reg8(s, BMI_CONF_REG(s->type),
				      BMI260_FILTER_PERF,
				      enable);
	}
	return ret;

}

static int set_data_rate(const struct motion_sensor_t *s,
			 int rate,
			 int rnd)
{
	int ret, normalized_rate;
	uint8_t reg_val;
	struct accelgyro_saved_data_t *data = BMI_GET_SAVED_DATA(s);

	if (rate == 0) {
		/* FIFO stop collecting events */
		if (IS_ENABLED(CONFIG_ACCEL_FIFO))
			bmi_enable_fifo(s, 0);
		/* disable sensor */
		ret = enable_sensor(s, 0);
		msleep(3);
		data->odr = 0;
		return ret;
	} else if (data->odr == 0) {
		/* enable sensor */
		ret = enable_sensor(s, 1);
		if (ret)
			return ret;
		/* Wait for accel/gyro to wake up */
		msleep(wakeup_time[s->type]);
	}

	ret = bmi_get_normalized_rate(s, rate, rnd,
				      &normalized_rate, &reg_val);
	if (ret)
		return ret;

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	ret = bmi_set_reg8(s, BMI_CONF_REG(s->type),
			   reg_val, BMI_ODR_MASK);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	/* Wait for the change to become effective */
	if (data->odr != 0)
		msleep(1000000 / MIN(data->odr, normalized_rate));
	/* Now that we have set the odr, update the driver's value. */
	data->odr = normalized_rate;

	/*
	 * FIFO start collecting events.
	 * They will be discarded if AP does not want them.
	 */
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		bmi_enable_fifo(s, 1);
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
	int i;
	intv3_t v;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		/*
		 * The offset of the accelerometer off_acc_[xyz] is a 8 bit
		 * two-complement number in units of 3.9 mg independent of the
		 * range selected for the accelerometer.
		 */
		bmi_accel_get_offset(s, v);
		break;
	case MOTIONSENSE_TYPE_GYRO:
		/*
		 * The offset of the gyroscope off_gyr_[xyz] is a 10 bit
		 * two-complement number in units of 0.061 °/s.
		 * Therefore a maximum range that can be compensated is
		 * -31.25 °/s to +31.25 °/s
		 */
		bmi_gyro_get_offset(s, v);
		break;
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
	int ret, val98, val_nv_conf;
	intv3_t v = { offset[X], offset[Y], offset[Z] };

	rotate_inv(v, *s->rot_standard_ref, v);

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI260_OFFSET_EN_GYR98, &val98);
	if (ret != 0)
		return ret;
	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI260_NV_CONF, &val_nv_conf);
	if (ret != 0)
		return ret;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		bmi_set_accel_offset(s, v);
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_NV_CONF,
				 val_nv_conf | BMI260_ACC_OFFSET_EN);
		break;
	case MOTIONSENSE_TYPE_GYRO:
		bmi_set_gyro_offset(s, v, &val98);
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_OFFSET_EN_GYR98,
				 val98 | BMI260_OFFSET_GYRO_EN);
		break;
	default:
		ret = EC_RES_INVALID_PARAM;
	}
	return ret;
}

static int wait_and_read_data(const struct motion_sensor_t *s,
			      intv3_t v, int try_cnt, int msec)
{
	uint8_t data[6];
	int ret, status = 0;

	/* Check if data is ready */
	while (try_cnt && !(status & BMI260_DRDY_ACC)) {
		msleep(msec);
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
				BMI260_STATUS, &status);
		if (ret)
			return ret;
		try_cnt -= 1;
	}
	if (!(status & BMI260_DRDY_ACC))
		return EC_ERROR_TIMEOUT;
	/* Read 6 bytes starting at xyz_reg */
	ret = bmi_read_n(s->port, s->i2c_spi_addr_flags,
			bmi_get_xyz_reg(s), data, 6);
	bmi_normalize(s, v, data);
	return ret;
}

static int calibrate_offset(const struct motion_sensor_t *s,
			    int range, intv3_t target, int16_t *offset)
{
	int ret = EC_ERROR_UNKNOWN;
	int i, n_sample = 32;
	int data_diff[3] = {0};

	/* Manually offset compensation */
	for (i = 0; i < n_sample; ++i) {
		intv3_t v;
		/* Wait data for at most 3 * 10 msec */
		ret = wait_and_read_data(s, v, 3, 10);
		if (ret)
			return ret;
		data_diff[X] += v[X] - target[X];
		data_diff[Y] += v[Y] - target[Y];
		data_diff[Z] += v[Z] - target[Z];
	}

	/* The data LSB: 1000 * range / 32768 (mdps | mg)*/
	for (i = X; i <= Z; ++i)
		offset[i] -= (int64_t)(data_diff[i] / n_sample) *
			     1000 * range / 32768;
	return ret;
}

static int perform_calib(const struct motion_sensor_t *s, int enable)
{
	int ret = EC_ERROR_UNKNOWN;
	int rate;
	int16_t temp;
	int16_t offset[3];
	intv3_t target = {0, 0, 0};
	/* Get sensor range for calibration*/
	int range = bmi_get_range(s);

	if (!enable)
		return EC_SUCCESS;
	rate = get_data_rate(s);
	ret = set_data_rate(s, 100000, 0);
	if (ret)
		return ret;

	ret = get_offset(s, offset, &temp);
	if (ret)
		goto end_perform_calib;

	switch (s->type) {
	case MOTIONSENSE_TYPE_ACCEL:
		target[Z] = BMI260_ACC_DATA_PLUS_1G(range);
		break;
	case MOTIONSENSE_TYPE_GYRO:
		break;
	default:
		/* Not supported on Magnetometer */
		ret = EC_RES_INVALID_PARAM;
		goto end_perform_calib;
	}

	/* Get the calibrated offset */
	ret = calibrate_offset(s, range, target, offset);
	if (ret)
		goto end_perform_calib;

	ret = set_offset(s, offset, temp);
	if (ret)
		goto end_perform_calib;

end_perform_calib:
	if (ret == EC_ERROR_TIMEOUT)
		CPRINTS("%s timeout", __func__);
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
	/* TODO(chingkang) */
	return EC_ERROR_UNIMPLEMENTED;
}

#ifdef CONFIG_GESTURE_HOST_DETECTION
int list_activities(const struct motion_sensor_t *s,
		    uint32_t *enabled,
		    uint32_t *disabled)
{
	struct bmi_drv_data_t *data = BMI_GET_DATA(s);
	*enabled = data->enabled_activities;
	*disabled = data->disabled_activities;
	return EC_RES_SUCCESS;
}
#endif

#ifdef CONFIG_ACCEL_INTERRUPTS

/**
 * bmi260_interrupt - called when the sensor activates the interrupt line.
 *
 * This is a "top half" interrupt handler, it just asks motion sense ask
 * to schedule the "bottom half", ->irq_handler().
 */
void bmi260_interrupt(enum gpio_signal signal)
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
		   BMI260_CMD_REG, BMI260_CMD_FIFO_FLUSH);

#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
	/* interrupt default is set to INT_D */
	bmi_write8(s->port, s->i2c_spi_addr_flags,
		   BMI260_FEAT_PAGE, 1);
	bmi_read16(s->port, s->i2c_spi_addr_flags,
		   BMI260_TAP_1, &tmp);
	val &= ~BMI260_TAP_1_SENSITIVITY_MASK;
	val |= BMI260_TAP_1_EN;
	bmi_write16(s->port, s->i2c_spi_addr_flags,
		    BMI260_TAP_1, tmp);

#endif
#ifdef CONFIG_BMI_ORIENTATION_SENSOR
	/* only use orientation sensor on the lid sensor */
	if (s->location == MOTIONSENSE_LOC_LID) {
		bmi_write8(s->port, s->i2c_spi_addr_flags,
			   BMI260_FEAT_PAGE, 1);
		bmi_read16(s->port, s->i2c_spi_addr_flags,
			   BMI260_ORIENT_1, &tmp);
		val |= BMI260_ORIENT_1_EN;
		val &= ~BMI260_ORIENT_1_UD_EN;
		val &= ~BMI260_ORIENT_1_MODE_MASK;
		bmi_write16(s->port, s->i2c_spi_addr_flags,
			    BMI260_ORIENT_1, tmp);
	}
#endif

	/* configure int1 as an interrupt */
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI260_INT1_IO_CTRL,
			 BMI260_INT1_OUTPUT_EN);
#ifdef CONFIG_ACCELGYRO_BMI_INT2_OUTPUT
	/* TODO(chingkang): Test it if we want int2 as an interrupt */
	/* configure int2 as an interrupt */
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI260_INT2_IO_CTRL,
			 BMI260_INT2_OUTPUT_EN);
#else
	/* configure int2 as an external input. */
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI260_INT2_IO_CTRL,
			 BMI260_INT2_INPUT_EN);
#endif

	/* Map activity interrupt to int 1 */
	tmp = 0;
#ifdef CONFIG_GESTURE_SIGMO
	tmp |= BMI260_MAP_ANY_MOTION_OUT;
	tmp |= BMI260_MAP_SIG_MOTION_OUT;
#endif
#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
	tmp |= BMI260_MAP_TAP_OUT;
#endif
#ifdef CONFIG_BMI_ORIENTATION_SENSOR
	/* enable orientation interrupt for lid sensor only */
	if (s->location == MOTIONSENSE_LOC_LID)
		tmp |= BMI260_MAP_ORIENTATION_OUT;
#endif
	ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
			 BMI260_INT1_MAP_FEAT, tmp);

	if (IS_ENABLED(CONFIG_ACCEL_FIFO)) {
		/* map fifo water mark to int 1 */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_INT_MAP_DATA,
				 BMI260_INT_MAP_DATA_REG(1, FWM) |
				 BMI260_INT_MAP_DATA_REG(1, FFULL));

		/*
		 * Configure fifo watermark to int whenever there's any data in
		 * there
		 */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_FIFO_WTM_0, 1);
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_FIFO_WTM_1, 0);
#ifdef CONFIG_ACCELGYRO_BMI_INT2_OUTPUT
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_FIFO_CONFIG_1,
				 BMI260_FIFO_HEADER_EN);
#else
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_FIFO_CONFIG_1,
				 (BMI260_FIFO_TAG_INT_LEVEL <<
				 BMI260_FIFO_TAG_INT2_EN_OFFSET) |
				 BMI260_FIFO_HEADER_EN);
#endif
		/* disable FIFO sensortime frame */
		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_FIFO_CONFIG_0, 0);
	}
	mutex_unlock(s->mutex);
	return ret;
}

#ifdef CONFIG_BMI_ORIENTATION_SENSOR
/* The irq_set_orientation was not tested yet. */
static void irq_set_orientation(struct motion_sensor_t *s)
{
	int new_orientation;
	/* read orientation */
	bmi_write8(s->port, s->i2c_spi_addr_flags,
		   BMI260_FEAT_PAGE, 0);
	bmi_read8(s->port, s->i2c_spi_addr_flags,
		  BMI260_ORIENT_OUT, &new_orientation);

	if (BMI_GET_DATA(s)->raw_orientation != new_orientation) {
		enum motionsensor_orientation orientation =
			MOTIONSENSE_ORIENTATION_UNKNOWN;

		BMI_GET_DATA(s)->raw_orientation = new_orientation;

		switch (new_orientation) {
		case BMI260_ORIENT_PORTRAIT:
			orientation = MOTIONSENSE_ORIENTATION_PORTRAIT;
			break;
		case BMI260_ORIENT_PORTRAIT_INVERT:
			orientation =
				MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_PORTRAIT;
			break;
		case BMI260_ORIENT_LANDSCAPE:
			orientation = MOTIONSENSE_ORIENTATION_LANDSCAPE;
			break;
		case BMI260_ORIENT_LANDSCAPE_INVERT:
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
	/* use uint16_t interrupt can cause error. */
	uint32_t interrupt = 0;
	int8_t has_read_fifo = 0;
	int rv;

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
			(!(*event & CONFIG_ACCELGYRO_BMI_INT_EVENT)))
		return EC_ERROR_NOT_HANDLED;

	do {
		rv = bmi_read16(s->port, s->i2c_spi_addr_flags,
				BMI260_INT_STATUS_0, &interrupt);
		/*
		 * Bail out of this loop there was an error reading the register
		 */
		if (rv)
			return rv;

#ifdef CONFIG_GESTURE_SENSOR_DOUBLE_TAP
		if (interrupt & BMI260_TAP_OUT) {
			*event |= TASK_EVENT_MOTION_ACTIVITY_INTERRUPT(
					MOTIONSENSE_ACTIVITY_DOUBLE_TAP);
		}
#endif
#ifdef CONFIG_GESTURE_SIGMO
		if (interrupt & BMI260_SIG_MOTION_OUT) {
			*event |= TASK_EVENT_MOTION_ACTIVITY_INTERRUPT(
					MOTIONSENSE_ACTIVITY_SIG_MOTION);
		}
#endif
		if (IS_ENABLED(CONFIG_ACCEL_FIFO) &&
			interrupt & (BMI260_FWM_INT | BMI260_FFULL_INT)) {
			bmi_load_fifo(s, last_interrupt_timestamp);
			has_read_fifo = 1;
		}
#ifdef CONFIG_BMI_ORIENTATION_SENSOR
		if (interrupt & BMI260_ORIENTATION_OUT)
			irq_set_orientation(s);
#endif
	} while (interrupt != 0);

	if (IS_ENABLED(CONFIG_ACCEL_FIFO) && has_read_fifo)
		motion_sense_fifo_commit_data();

	return EC_SUCCESS;
}
#endif  /* CONFIG_ACCEL_INTERRUPTS */

static int init_config(const struct motion_sensor_t *s)
{
	int init_status, ret;
	uint16_t i;
	const int burst_write_len = 2048;

	/* disable advance power save but remain fifo self wakeup*/
	bmi_write8(s->port, s->i2c_spi_addr_flags, BMI260_PWR_CONF, 2);
	msleep(1);
	/* prepare for config load */
	bmi_write8(s->port, s->i2c_spi_addr_flags, BMI260_INIT_CTRL, 0);

	/* load config file to INIT_DATA */
	for (i = 0; i < g_bmi260_config_tbin_len; i += burst_write_len) {
		uint8_t addr[2];

		addr[0] = (i / 2) & 0xF;
		addr[1] = (i / 2) >> 4;
		ret = bmi_write_n(s->port, s->i2c_spi_addr_flags,
				  BMI260_INIT_ADDR_0, addr, 2);
		if (ret)
			break;
		ret = bmi_write_n(s->port, s->i2c_spi_addr_flags,
				  BMI260_INIT_DATA, &g_bmi260_config_tbin[i],
				  burst_write_len);
		if (ret)
			break;
	}

	if (ret)
		CPRINTS("ret: 0x%x", ret);
	/* finish config load */
	bmi_write8(s->port, s->i2c_spi_addr_flags, BMI260_INIT_CTRL, 1);
	/* wait INTERNAL_STATUS.message to be 0x1 which take at most 150ms */
	for (i = 0; i < 15; ++i) {
		msleep(10);
		ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI260_INTERNAL_STATUS, &init_status);
		if (ret)
			break;
		init_status &= BMI260_MESSAGE_MASK;
		if (init_status == BMI260_INIT_OK)
			break;
	}
	if (ret || init_status != BMI260_INIT_OK)
		return EC_ERROR_INVALID_CONFIG;
	return EC_SUCCESS;
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp, i;
	struct accelgyro_saved_data_t *saved_data = BMI_GET_SAVED_DATA(s);

	ret = bmi_read8(s->port, s->i2c_spi_addr_flags,
			BMI260_CHIP_ID, &tmp);
	if (ret) {
		CPRINTS("failed to read chip id");
		return EC_ERROR_UNKNOWN;
	}

	if (tmp != BMI260_CHIP_ID_MAJOR) {
		CPRINTS("%s failed", __func__);
		return EC_ERROR_ACCESS_DENIED;
	}

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		struct bmi_drv_data_t *data = BMI_GET_DATA(s);

		ret = bmi_write8(s->port, s->i2c_spi_addr_flags,
				 BMI260_CMD_REG, BMI260_CMD_SOFT_RESET);
		/* Reset the chip to be in a good state */
		msleep(2);
		if (init_config(s)) {
			CPRINTS("init_config() failed");
			return EC_ERROR_INVALID_CONFIG;
		}

		data->flags &= ~(BMI_FLAG_SEC_I2C_ENABLED |
				(BMI_FIFO_ALL_MASK <<
				 BMI_FIFO_FLAG_OFFSET));
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
	}

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

const struct accelgyro_drv bmi260_drv = {
	.init = init,
	.read = bmi_read,
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
	.read_temp = bmi_read_temp,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif
#ifdef CONFIG_GESTURE_HOST_DETECTION
	.manage_activity = manage_activity,
	.list_activities = list_activities,
#endif
};

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
struct i2c_stress_test_dev bmi260_i2c_stress_test_dev = {
	.reg_info = {
		.read_reg = BMI260_CHIP_ID,
		.read_val = BMI260_CHIP_ID_MAJOR,
		.write_reg = BMI260_PMU_TRIGGER,
	},
	.i2c_read = &bmi_read8,
	.i2c_write = &bmi_write8,
};
#endif /* CONFIG_CMD_I2C_STRESS_TEST_ACCEL */

/*
 * TODO(chingkang): Replace bmi260_get_sensor_temp in some board config to
 *                  bmi_get_sensor_temp. Then, remove this definition.
 */
int bmi260_get_sensor_temp(int idx, int *temp_ptr)
{
	return bmi_get_sensor_temp(idx, temp_ptr);
}
