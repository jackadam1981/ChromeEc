/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Silicon Image SI1141/SI1142 light sensor driver
 *
 * Started from linux si114x driver.
 */
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/als_si114x.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

static int init(const struct motion_sensor_t *s);

#ifndef CONFIG_ALS_SI114X_INT_EVENT
/* MEAS_RATE frequencies in milli Hz that result in losless compression */
int si114x_meas_rate_mHz[] = {
	1250, 2500, 5000, 10000, 12500, 20000, 25000, 50000,
};
#endif

static int debug;
/**
 * Read 8bit register from device.
 */
static inline int raw_read8(const int port, const int addr, const int reg,
			    int *data_ptr)
{
	return i2c_read8(port, addr, reg, data_ptr);
}

/**
 * Write 8bit register from device.
 */
static inline int raw_write8(const int port, const int addr, const int reg,
			     int data)
{
	return i2c_write8(port, addr, reg, data);
}

/**
 * Read 16bit register from device.
 */
static inline int raw_read16(const int port, const int addr, const int reg,
			     int *data_ptr)
{
	return i2c_read16(port, addr, reg, data_ptr);
}

/* helper function to operate on parameter values: op can be query/set/or/and */
static int si114x_param_op(const struct motion_sensor_t *s,
			   uint8_t op,
			   uint8_t param,
			   int *value)
{
	int ret;

	mutex_lock(s->mutex);

	if (op != SI114X_CMD_PARAM_QUERY) {
		ret = raw_write8(s->port, s->addr, SI114X_REG_PARAM_WR, *value);
		if (ret != EC_SUCCESS)
			goto error;
	}

	ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND,
			 op | (param & 0x1F));
	if (ret != EC_SUCCESS)
		goto error;

	ret = raw_read8(s->port, s->addr, SI114X_REG_PARAM_RD, value);
	if (ret != EC_SUCCESS)
		goto error;

	mutex_unlock(s->mutex);

	*value &= 0xff;
	return EC_SUCCESS;
error:
	mutex_unlock(s->mutex);
	return ret;
}

static int si114x_read_results(struct motion_sensor_t *s, int nb)
{
	int i, ret, val;
	struct si114x_drv_data_t *data = SI114X_GET_DATA(s);
	struct si114x_typed_data_t *type_data = SI114X_GET_TYPED_DATA(s);
#ifdef CONFIG_ACCEL_FIFO
	struct ec_response_motion_sensor_data vector;
#endif

	/* Read ALX result */
	for (i = 0; i < nb; i++) {
		ret = raw_read16(s->port, s->addr,
				 type_data->base_data_reg + i * 2,
				 &val);
		if (debug)
			ccprintf("si114x: Rd ALS[%x] = 0x%x\n",
				 type_data->base_data_reg + i * 2, val);
		if (ret)
			break;
		if (val == SI114X_OVERFLOW) {
			/* overflowing, try next time. */
			return EC_SUCCESS;
		} else if (val + type_data->offset <= 0) {
			/* No light */
			val = 1;
		} else {
			/* Add offset, calibration */
			val += type_data->offset;
		}
		/*
		 * Proximity sensor data is inverse of the distance.
		 * Return back something proportional to distance,
		 * we correct later with the scale parameter.
		 */
		if (s->type == MOTIONSENSE_TYPE_PROX)
			val = SI114X_PS_INVERSION(val);
		val = val * type_data->scale +
			val * type_data->uscale / 10000;
		s->raw_xyz[i] = val;
	}

	if (ret != EC_SUCCESS)
		return ret;

	if (s->type == MOTIONSENSE_TYPE_PROX)
		data->covered = (s->raw_xyz[0] < SI114X_COVERED_THRESHOLD);
	else if (data->covered)
		/*
		 * The sensor (proximity & light) is covered. The light data
		 * will most likely be incorrect (darker than expected), so
		 * ignore the measurement.
		 */
		return EC_SUCCESS;

	/* Add in fifo if changed only */
	for (i = 0; i < nb; i++) {
		if (s->raw_xyz[i] != s->xyz[i])
			break;
	}
	if (i == nb)
		return EC_ERROR_UNCHANGED;

#ifdef CONFIG_ACCEL_FIFO
	vector.flags = 0;
	for (i = 0; i < nb; i++)
		vector.data[i] = s->raw_xyz[i];
	for (i = nb; i < 3; i++)
		vector.data[i] = 0;
	vector.sensor_num = s - motion_sensors;
	motion_sense_fifo_add_unit(&vector, s, nb);
#else
	/* We need to copy raw_xyz into xyz with mutex */
#endif
	return EC_SUCCESS;
}

#ifdef CONFIG_ALS_SI114X_INT_EVENT
void si114x_interrupt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ALS_SI114X_INT_EVENT, 0);
}

/**
 * irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt.
 *
 * For now, we just print out. We should set a bitmask motion sense code will
 * act upon.
 */
static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	int ret = EC_SUCCESS, val;
	struct si114x_drv_data_t *data = SI114X_GET_DATA(s);
	struct si114x_typed_data_t *type_data = SI114X_GET_TYPED_DATA(s);

	if (!(*event & CONFIG_ALS_SI114X_INT_EVENT))
		return EC_ERROR_NOT_HANDLED;

	ret = raw_read8(s->port, s->addr, SI114X_REG_IRQ_STATUS, &val);
	if (ret)
		return ret;

	if (!(val & type_data->irq_flags))
		return EC_ERROR_INVAL;

	/* clearing IRQ */
	ret = raw_write8(s->port, s->addr, SI114X_REG_IRQ_STATUS,
			 val & type_data->irq_flags);
	if (ret != EC_SUCCESS)
		CPRINTS("clearing irq failed");

	switch (data->state) {
	case SI114X_ALS_IN_PROGRESS:
	case SI114X_ALS_IN_PROGRESS_PS_PENDING:
		/* We are only reading the visible light sensor */
		ret = si114x_read_results(s, 1);
		/* Fire pending requests */
		if (data->state == SI114X_ALS_IN_PROGRESS_PS_PENDING) {
			ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND,
					SI114X_CMD_PS_FORCE);
			data->state = SI114X_PS_IN_PROGRESS;
		} else {
			data->state = SI114X_IDLE;
		}
		break;
	case SI114X_PS_IN_PROGRESS:
	case SI114X_PS_IN_PROGRESS_ALS_PENDING:
		/* Read PS results */
		ret = si114x_read_results(s, SI114X_NUM_LEDS);
		if (data->state == SI114X_PS_IN_PROGRESS_ALS_PENDING) {
			ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND,
					SI114X_CMD_ALS_FORCE);
			data->state = SI114X_ALS_IN_PROGRESS;
		} else {
			data->state = SI114X_IDLE;
		}
		break;
	case SI114X_IDLE:
	default:
		CPRINTS("Invalid state");
	}
	return ret;
}

/* Just trigger a measurement */
static int read_force(const struct motion_sensor_t *s, vector_3_t v)
{
	int ret = 0;
	uint8_t cmd;

	struct si114x_drv_data_t *data = SI114X_GET_DATA(s);

	switch (data->state) {
	case SI114X_ALS_IN_PROGRESS:
		if (s->type == MOTIONSENSE_TYPE_PROX)
			data->state = SI114X_ALS_IN_PROGRESS_PS_PENDING;
#if 0
		else
			CPRINTS("Invalid state");
#endif
		ret = EC_ERROR_BUSY;
		break;
	case SI114X_PS_IN_PROGRESS:
		if (s->type == MOTIONSENSE_TYPE_LIGHT)
			data->state = SI114X_PS_IN_PROGRESS_ALS_PENDING;
#if 0
		else
			CPRINTS("Invalid state");
#endif
		ret = EC_ERROR_BUSY;
		break;
	case SI114X_IDLE:
		switch (s->type) {
		case MOTIONSENSE_TYPE_LIGHT:
			cmd = SI114X_CMD_ALS_FORCE;
			data->state = SI114X_ALS_IN_PROGRESS;
			break;
		case MOTIONSENSE_TYPE_PROX:
			cmd = SI114X_CMD_PS_FORCE;
			data->state = SI114X_PS_IN_PROGRESS;
			break;
		default:
			CPRINTS("Invalid sensor type");
			return EC_ERROR_INVAL;
		}
		ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND, cmd);
		ret = EC_RES_IN_PROGRESS;
		break;
	case SI114X_ALS_IN_PROGRESS_PS_PENDING:
	case SI114X_PS_IN_PROGRESS_ALS_PENDING:
		ret = EC_ERROR_ACCESS_DENIED;
		break;
	case SI114X_NOT_READY:
		ret = EC_ERROR_NOT_POWERED;
	}
	if (ret == EC_ERROR_ACCESS_DENIED &&
	    s->type == MOTIONSENSE_TYPE_LIGHT) {
		timestamp_t ts_now = get_time();

		/*
		 * We were unable to access the sensor for THRES time.
		 * We should reset the sensor to clear the interrupt register
		 * and the state machine.
		 */
		if (time_after(ts_now.le.lo,
			       s->last_collection + SI114X_DENIED_THRESHOLD)) {
			int ret, val;

			ret = raw_read8(s->port, s->addr,
					SI114X_REG_IRQ_STATUS, &val);
			CPRINTS("%d stuck IRQ_STATUS 0x%02x - ret %d",
				s->name, val, ret);
			init(s);
		}
	}
	return ret;
}

#else /* CONFIG_ALS_SI114X_INT_EVENT */

static int read_auto(const struct motion_sensor_t *s, vector_3_t v)
{
	int ret = 0;
	int val;
	int irq = 0;

	/* Read IRQ register to check if new measurement is ready */
	ret = raw_read8(s->port, s->addr, SI114X_REG_IRQ_STATUS, &val);
	if (debug)
		ccprintf("Si Auto READ: IRQ = 0x%x\n", val);

	/* Read ALS measuremnet if new value is ready */
	if (val & SI114X_ALS_INT_FLAG && s->type == MOTIONSENSE_TYPE_LIGHT) {
		ret = si114x_read_results((struct motion_sensor_t *)s, 1);
		irq |= SI114X_ALS_INT_FLAG;
	}

	/* Read PS measurement if new values are ready */
	if (val & SI114X_PS_INT_FLAG && s->type == MOTIONSENSE_TYPE_PROX) {
		ret = si114x_read_results((struct motion_sensor_t *)s,
					  SI114X_NUM_LEDS);
		irq |= SI114X_PS_INT_FLAG;
	}

	/* Clear IRQ status register */
	ret = raw_write8(s->port, s->addr, SI114X_REG_IRQ_STATUS, irq);

	return ret;
}
#endif

static int si114x_set_chlist(const struct motion_sensor_t *s)
{
	int reg = 0;

	/* Not interested in temperature (AUX nor IR) */
	reg = SI114X_CHLIST_EN_ALSVIS;
	switch (SI114X_NUM_LEDS) {
	case 3:
		reg |= SI114X_CHLIST_EN_PS3;
	case 2:
		reg |= SI114X_CHLIST_EN_PS2;
	case 1:
		reg |= SI114X_CHLIST_EN_PS1;
		break;
	}

	return si114x_param_op(s, SI114X_CMD_PARAM_SET,
			SI114X_PARAM_CHLIST, &reg);
}

#ifdef CONFIG_ALS_SI114X_CHECK_REVISION
static int si114x_revisions(const struct motion_sensor_t *s)
{
	int val;
	int ret = raw_read8(s->port, s->addr, SI114X_REG_PART_ID, &val);
	if (ret != EC_SUCCESS)
		return ret;

	if (val != CONFIG_ALS_SI114X) {
		CPRINTS("invalid part");
		return EC_ERROR_ACCESS_DENIED;
	}

	ret = raw_read8(s->port, s->port, s->addr, SI114X_REG_SEQ_ID, &val);
	if (ret != EC_SUCCESS)
		return ret;

	if (val < SI114X_SEQ_REV_A03)
		CPRINTS("WARNING: old sequencer revision");

	return 0;
}
#endif

static int si114x_initialize(const struct motion_sensor_t *s)
{
	int ret, val;

	/* send reset command */
	ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND,
			 SI114X_CMD_RESET);
	if (ret != EC_SUCCESS)
		return ret;
	msleep(20);

	/* hardware key, magic value */
	ret = raw_write8(s->port, s->addr, SI114X_REG_HW_KEY, 0x17);
	if (ret != EC_SUCCESS)
		return ret;
	msleep(20);

	/* enable interrupt for certain activities */
	ret = raw_write8(s->port, s->addr, SI114X_REG_IRQ_ENABLE,
		SI114X_PS3_IE | SI114X_PS2_IE | SI114X_PS1_IE |
		SI114X_ALS_INT0_IE);
	if (ret != EC_SUCCESS)
		return ret;

	/* interrupt configuration, interrupt output enable */
	ret = raw_write8(s->port, s->addr, SI114X_REG_INT_CFG,
			 SI114X_INT_CFG_OE);
	if (ret != EC_SUCCESS)
		return ret;

#ifdef CONFIG_ALS_SI114X_INT_EVENT
	/* Only forced mode when interrupts are enabled */
	ret = raw_write8(s->port, s->addr, SI114X_REG_MEAS_RATE, 0);
	if (ret != EC_SUCCESS)
		return ret;

	/* measure ALS every time device wakes up */
	ret = raw_write8(s->port, s->addr, SI114X_REG_ALS_RATE, 0);
	if (ret != EC_SUCCESS)
		return ret;

	/* measure proximity every time device wakes up */
	ret = raw_write8(s->port, s->addr, SI114X_REG_PS_RATE, 0);
	if (ret != EC_SUCCESS)
		return ret;
#endif

	/* set LED currents to maximum */
	switch (SI114X_NUM_LEDS) {
	case 3:
		ret = raw_write8(s->port, s->addr,
			SI114X_REG_PS_LED3, 0x0f);
		if (ret != EC_SUCCESS)
			return ret;
		ret = raw_write8(s->port, s->addr,
			SI114X_REG_PS_LED21, 0xff);
		break;
	case 2:
		ret = raw_write8(s->port, s->addr,
			SI114X_REG_PS_LED21, 0xff);
		break;
	case 1:
		ret = raw_write8(s->port, s->addr,
			SI114X_REG_PS_LED21, 0x0f);
		break;
	}
	if (ret != EC_SUCCESS)
		return ret;

	ret = si114x_set_chlist(s);
	if (ret != EC_SUCCESS)
		return ret;

	/* set normal proximity measurement mode, set high signal range
	 * PS measurement */
	val = SI114X_PARAM_PS_ADC_MISC_NORMAL_MODE;
	ret = si114x_param_op(s, SI114X_CMD_PARAM_SET,
			      SI114X_PARAM_PS_ADC_MISC, &val);
	return ret;
}

static int set_resolution(const struct motion_sensor_t *s,
				int res,
				int rnd)
{
	int ret, reg1, reg2, val;
	/* override on resolution: set the gain. between 0 to 7 */
	if (s->type == MOTIONSENSE_TYPE_PROX) {
		if (res < 0 || res > 5)
			return EC_ERROR_PARAM2;
		reg1 = SI114X_PARAM_PS_ADC_GAIN;
		reg2 = SI114X_PARAM_PS_ADC_COUNTER;
	} else {
		if (res < 0 || res > 7)
			return EC_ERROR_PARAM2;
		reg1 = SI114X_PARAM_ALSVIS_ADC_GAIN;
		reg2 = SI114X_PARAM_ALSVIS_ADC_COUNTER;
	}

	val = res;
	ret = si114x_param_op(s, SI114X_CMD_PARAM_SET, reg1, &val);
	if (ret != EC_SUCCESS)
		return ret;
	/* set recovery period to one's complement of gain */
	val = (~res & 0x07) << 4;
	ret = si114x_param_op(s, SI114X_CMD_PARAM_SET, reg2, &val);
	return ret;
}

static int get_resolution(const struct motion_sensor_t *s)
{
	int ret, reg, val;
	if (s->type == MOTIONSENSE_TYPE_PROX)
		reg = SI114X_PARAM_PS_ADC_GAIN;
	else
		/* ignore IR led */
		reg = SI114X_PARAM_ALSVIS_ADC_GAIN;

	val = 0;
	ret = si114x_param_op(s, SI114X_CMD_PARAM_QUERY, reg, &val);
	if (ret != EC_SUCCESS)
		return -1;

	return val & 0x07;
}

static int set_range(const struct motion_sensor_t *s,
				int range,
				int rnd)
{
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	data->scale = range >> 16;
	data->uscale = range & 0xffff;
	return EC_SUCCESS;
}

static int get_range(const struct motion_sensor_t *s)
{
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	return (data->scale << 16) | (data->uscale);
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	/* Sensor in forced mode, rate is used by motion_sense */
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	return data->rate;
}

#ifndef CONFIG_ALS_SI114X_INT_EVENT
static int si114x_uncompress(int *raw, int compress)
{
	int frac;
	int exp;

	/* Exponent is the upper nibble */
	exp = compress >> 4;
	/* 4 fractional bits */
	frac = compress & 0xf;
	/* If exp > 0, then frac is 1.frac */
	if (exp)
		frac |= 0x10;
	/* uncompressed = round(exp * frac) >> 8 */
	*raw = ((1 << (exp + 4)) * frac + 0x80) >> 8;

	return EC_SUCCESS;
}

static int si114x_compress(int raw, int *compress)
{
	int exp;
	int frac;
	int left;

	/* Ensure that raw number is in range (0 - 0xffff) */
	if (raw & 0xffff0000)
		return EC_ERROR_INVAL;

	/* Check if input is 0 */
	if (!(raw & 0xffff)) {
		*compress = 0;
		return EC_SUCCESS;
	}

	/* Determine exponent by finding MSB */
	left = 0;
	while (!(raw & 0x8000)) {
		raw <<= 1;
		left++;
	}
	exp = 15 - left;

	/* If exp != 0, then it's 1.frac */
	if (exp)
		raw <<= 1;
	/* Keep 4 fractional bits */
	frac = (raw & 0xf000) >> 12;
	/* Compressed value uppper nibble = exp, lower nibble = frac */
	*compress = exp << 4 | frac;

	return EC_SUCCESS;
}

static int compute_meas_rate(int odr_mHz, int *meas, int *als)
{
	int si_clocks;
	int meas_rate;
	int als_rate;
	int mod, div;
	int ret;
	int rate_index;
	int raw_meas;
	int raw_als;
	int meas_freq_mHz;

	/* Find the slowest MEAS_RATE that is an integer sub-multiple of the
	 * desired sensor rate and still result in a lossless 16->8 bit
	 * compression value.
	 */
	for (rate_index = 0; rate_index < ARRAY_SIZE(si114x_meas_rate_mHz);
	     rate_index++) {
		div = si114x_meas_rate_mHz[rate_index] / odr_mHz;
		mod = si114x_meas_rate_mHz[rate_index] % odr_mHz;
		if (div && !mod) {
			meas_freq_mHz = si114x_meas_rate_mHz[rate_index];
			break;
		}
	}

	/*
	 * If reached end of the table, then couldn't find a MEAS_RATE value
	 * that will result in lossless compression.
	 */
	if (rate_index == ARRAY_SIZE(si114x_meas_rate_mHz)) {
		ccprintf("Si114x: odr %d can't set exact\n", odr_mHz);
		/* assume MEAS_RATE is desired rate / 8 */
		div = 8;
		meas_freq_mHz = odr_mHz * div;
	}

	/* Compute number of 32 kHz clock cycles between measurements */
	si_clocks = SI114x_CLOCK_HZ * 1000 / meas_freq_mHz;
	/* Get compressed vesrion of this number */
	ret = si114x_compress(si_clocks, &meas_rate);
	if (ret)
		return EC_ERROR_INVAL;
	/* Number of MEAS_RATE per sensor measurement */
	ret = si114x_compress(div, &als_rate);
	if (ret)
		return EC_ERROR_INVAL;
	/* Sanity check */
	ccprintf("odr = %d, meas_rate = %d, als_rate = %d\n", odr_mHz,
		 meas_freq_mHz, div);
	ccprintf("meas_rate = 0x%x, als_rate = 0x%x\n", meas_rate, als_rate);
	si114x_uncompress(&raw_meas, meas_rate);
	si114x_uncompress(&raw_als, als_rate);
	ccprintf("meas_rate = %d, als_rate = %d\n", raw_meas, raw_als);
	ccprintf("actual rate = %d mHz\n", (SI114x_CLOCK_HZ * 1000) /
		 (raw_meas * div));

	*meas = meas_rate;
	*als = als_rate;

	return EC_SUCCESS;
}

#endif

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
#ifndef CONFIG_ALS_SI114X_INT_EVENT
	int meas;
	int val;
	int ret;
	int cmd;
#endif
	data->rate = rate;

#ifndef CONFIG_ALS_SI114X_INT_EVENT
	/*
	 * If in polling mode, then check rate to see if sensor measurements
	 * need to be enabled. If so, then set measure rate and either ALS or
	 * PROX rate to match the odr rate. MEAS_RATE is the number of 32 kHz
	 * cycles to wake up the sensor. ALS_RATE and PROX_RATE control the
	 * number of times the sensor wakes up before making a particular
	 * measurement. Because MEAS_RATE is a compressed value of the desired
	 * 16 bit value, can't represent every desired value.
	 */
	if (data->rate) {
		ret = compute_meas_rate(data->rate * 2, &meas, &val);
		if (ret)
			return ret;
		/* Write measure rate register */
		ret = raw_write8(s->port, s->addr, SI114X_REG_MEAS_RATE, meas);
		if (ret)
			return ret;

		if (s->type == MOTIONSENSE_TYPE_LIGHT) {
			cmd = SI114X_CMD_ALS_AUTO;
			ret = raw_write8(s->port, s->addr, SI114X_REG_ALS_RATE,
					 val);
			if (ret)
				return ret;
		} else if (s->type == MOTIONSENSE_TYPE_PROX) {
			cmd = SI114X_CMD_PS_AUTO;
			ret = raw_write8(s->port, s->addr, SI114X_REG_PS_RATE,
					 val);
			if (ret)
				return ret;
		} else {
			return EC_ERROR_INVAL;
		}

		ccprintf("Si114x: Enabling ALS auto mode\n");
		/* Enable auto measurements at the rate specified above */
		ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND, cmd);
		if (ret)
			return ret;
	} else if (s->type == MOTIONSENSE_TYPE_LIGHT) {
		ccprintf("Si114x: Disabling ALS auto mode\n");
		/* Disable auto ALS measurements */
		ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND,
				 SI114X_CMD_ALS_PAUSE);
	} else if (s->type == MOTIONSENSE_TYPE_PROX) {
		/* Disable auto Proximity measurements */
		ret = raw_write8(s->port, s->addr, SI114X_REG_COMMAND,
				 SI114X_CMD_PS_PAUSE);
	}
#endif
	return EC_SUCCESS;
}

static int set_offset(const struct motion_sensor_t *s,
			const int16_t *offset,
			int16_t    temp)
{
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	data->offset = offset[X];
	return EC_SUCCESS;
}

static int get_offset(const struct motion_sensor_t *s,
			int16_t   *offset,
			int16_t    *temp)
{
	struct si114x_typed_data_t *data = SI114X_GET_TYPED_DATA(s);
	offset[X] = data->offset;
	offset[Y] = 0;
	offset[Z] = 0;
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

static int init(const struct motion_sensor_t *s)
{
	int ret, resol;
	struct si114x_drv_data_t *data = SI114X_GET_DATA(s);

	/* initialize only once: light must be declared first. */
	if (s->type == MOTIONSENSE_TYPE_LIGHT) {
#ifdef CONFIG_ALS_SI114X_CHECK_REVISION
		ret = si114x_revisions(s);
		if (ret != EC_SUCCESS)
			return ret;
#endif
		ret = si114x_initialize(s);
		if (ret != EC_SUCCESS)
			return ret;

		data->state = SI114X_IDLE;
		resol = 7;
	} else {
		if (data->state == SI114X_NOT_READY)
			return EC_ERROR_ACCESS_DENIED;
		resol = 5;
	}

	set_range(s, s->default_range, 0);
	/*
	 * Sensor is most likely behind a glass.
	 * Max out the gain to get correct measurement
	 */
	set_resolution(s, resol, 0);

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d]\n",
		s->name, s->type, get_range(s));

	debug = 0;
	return EC_SUCCESS;
}

const struct accelgyro_drv si114x_drv = {
	.init = init,
#ifdef CONFIG_ALS_SI114X_INT_EVENT
	.read = read_force,
#else
	.read = read_auto,
#endif
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = set_resolution,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = set_offset,
	.get_offset = get_offset,
	.perform_calib = NULL,
#ifdef CONFIG_ALS_SI114X_INT_EVENT
	.irq_handler = irq_handler,
#endif
#ifdef CONFIG_ACCEL_FIFO
	.load_fifo = NULL,
#endif
};

struct si114x_drv_data_t g_si114x_data = {
	.state = SI114X_NOT_READY,
	.covered = 0,
	.type_data = {
		/* Proximity */
		{
			.base_data_reg = SI114X_REG_PS1_DATA0,
			.irq_flags = SI114X_PS_INT_FLAG,
			.scale = 1,
			.offset = -256,
		},
		/* light */
		{
			.base_data_reg = SI114X_REG_ALSVIS_DATA0,
			.irq_flags = SI114X_ALS_INT_FLAG,
			.scale = 1,
			.offset = -256,
		}
	}
};

#ifndef CONFIG_ALS_SI114X_INT_EVENT
/* SC DEBUG only */
static void compute_table(void)
{
	int odr;
	int meas_comp;
	int meas_raw;
	int si_clocks;
	int si_mod;
	int i;

	for (i = 0, odr = 50000; odr > 100; odr -= 10, i++) {
		si_clocks = SI114x_CLOCK_HZ * 1000 / odr;
		si_mod = (SI114x_CLOCK_HZ * 1000) % odr;
		if (si_clocks < 0xffff && !si_mod) {
			si114x_compress(si_clocks, &meas_comp);
			si114x_uncompress(&meas_raw, meas_comp);
			if (si_clocks == meas_raw) {
			ccprintf("[%d]: odr = %d, clk1 = %d, clk2 = %d: %d\n",
				 i, odr, si_clocks, meas_raw,
				 si_clocks == meas_raw);
			msleep(50);
			}
		}
	}
}

/* SC DEBUG only */
static int command_si(int argc, char **argv)
{
	int raw;
	int meas, als;
	char *e;

	if (argc < 1)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "odr")) {
		compute_table();
		return EC_SUCCESS;
	} else if (!strcasecmp(argv[1], "debug")) {
		debug ^= 1;
		return EC_SUCCESS;
	} else if (!strcasecmp(argv[1], "irq")) {
		raw = strtoi(argv[2], &e, 10);
		if (*e)
			return EC_ERROR_PARAM2;
		if (raw)
			/* interrupt configuration, interrupt output enable */
			raw_write8(I2C_PORT_ALS, SI114X_ADDR,
				   SI114X_REG_INT_CFG, SI114X_INT_CFG_OE);
		else
			raw_write8(I2C_PORT_ALS, SI114X_ADDR,
				   SI114X_REG_INT_CFG, 0);
		return EC_SUCCESS;
	}

	raw = strtoi(argv[1], &e, 10);
	if (*e)
		return EC_ERROR_PARAM2;

	ccprintf("odr = %d mHz\n", raw);
	compute_meas_rate(raw, &meas, &als);
	ccprintf("MEAS_RATE = 0x%x, ALS_RATE = 0x%x\n", meas, als);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(si, command_si,
			"<0 - 65535>",
			"Test si compression");
#endif
