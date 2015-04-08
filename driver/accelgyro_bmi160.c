/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI160/BMC50 accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "driver/accelgyro_bmi160.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

/*
 * Struct for pairing an engineering value with the register value for a
 * parameter.
 */
struct accel_param_pair {
	int val; /* Value in engineering units. */
	int reg_val; /* Corresponding register value. */
};

/* List of range values in +/-G's and their associated register values. */
static const struct accel_param_pair g_ranges[] = {
	{2, BMI160_GSEL_2G},
	{4, BMI160_GSEL_4G},
	{8, BMI160_GSEL_8G},
	{16, BMI160_GSEL_8G}
};

/*
 * List of angular rate range values in +/-dps's
 * and their associated register values.
 */
const struct accel_param_pair dps_ranges[] = {
	{125, BMI160_DPS_SEL_125},
	{250, BMI160_DPS_SEL_250},
	{500, BMI160_DPS_SEL_500},
	{1000, BMI160_DPS_SEL_1000},
	{2000, BMI160_DPS_SEL_2000}
};

static inline const struct accel_param_pair *get_range_table(
		enum motionsensor_type type, int *psize)
{
	if (MOTIONSENSE_TYPE_ACCEL == type) {
		if (psize)
			*psize = ARRAY_SIZE(g_ranges);
		return g_ranges;
	} else {
		if (psize)
			*psize = ARRAY_SIZE(dps_ranges);
		return dps_ranges;
	}
}

/* List of ODR (gyro off) values in mHz and their associated register values.*/
/* The table is calculated from: value = 100 / (1 << (8 - reg_val)) */
const struct accel_param_pair accel_mag_odr[] = {
	{ 0, BMI160_ACCEL_ODR_0HZ},
	{ 780, BMI160_ACCEL_ODR_0_78HZ},
	{ 1560, BMI160_ACCEL_ODR_1_56HZ},
	{ 3120, BMI160_ACCEL_ODR_3_12HZ},
	{ 6250, BMI160_ACCEL_ODR_6_25HZ},
	{ 12500, BMI160_ACCEL_ODR_12_5HZ},
	{ 25000, BMI160_ACCEL_ODR_25HZ},
	{ 50000, BMI160_ACCEL_ODR_50HZ},
	{ 100000, BMI160_ACCEL_ODR_100HZ},
	{ 200000, BMI160_ACCEL_ODR_200HZ},
	{ 400000, BMI160_ACCEL_ODR_400HZ},
	{ 800000, BMI160_ACCEL_ODR_800HZ},
	{ 1600000, BMI160_ACCEL_ODR_1600HZ},
};

/* List of ODR (gyro on) values in mHz and their associated register values. */
/* The table is calculated from: value = 100 / (1 << (7 - reg_val)) */
const struct accel_param_pair gyro_odr[] = {
	{ 0, BMI160_GYRO_ODR_0HZ},
	{ 25000, BMI160_GYRO_ODR_25HZ},
	{ 50000, BMI160_GYRO_ODR_50HZ},
	{ 100000, BMI160_GYRO_ODR_100HZ},
	{ 200000, BMI160_GYRO_ODR_200HZ},
	{ 400000, BMI160_GYRO_ODR_400HZ},
	{ 800000, BMI160_GYRO_ODR_800HZ},
	{ 1600000, BMI160_GYRO_ODR_1600HZ},
	{ 3200000, BMI160_GYRO_ODR_3200HZ},
};

static inline const struct accel_param_pair *get_odr_table(
		enum motionsensor_type type, int *psize)
{
	if (MOTIONSENSE_TYPE_GYRO == type) {
		if (psize)
			*psize = ARRAY_SIZE(gyro_odr);
		return gyro_odr;
	} else {
		if (psize)
			*psize = ARRAY_SIZE(accel_mag_odr);
		return accel_mag_odr;
	}
}

static inline enum bmi160_sensor_type get_bmi160_type(
		enum motionsensor_type type)
{
	switch (type) {
	case MOTIONSENSE_TYPE_ACCEL:
		return BMI160_ACC;
	case MOTIONSENSE_TYPE_GYRO:
		return BMI160_GYR;
	case MOTIONSENSE_TYPE_MAG:
		return BMI160_MAG;
	}
}

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
/**
 * @return reg value that matches the given engineering value passed in.
 * The round_up flag is used to specify whether to round up or down.
 * Note, this function always returns a valid reg value. If the request is
 * outside the range of values, it returns the closest valid reg value.
 */
static int get_reg_val(const int eng_val, const int round_up,
		const struct accel_param_pair *pairs, const int size)
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
static int get_engineering_val(const int reg_val,
		const struct accel_param_pair *pairs, const int size)
{
	int i;
	for (i = 0; i < size; i++) {
		if (reg_val == pairs[i].reg_val)
			break;
	}
	return pairs[i].val;
}

/**
 * Read register from accelerometer.
 */
static inline int raw_read8(const int addr, const int reg, int *data_ptr)
{
	return i2c_read8(I2C_PORT_ACCEL, addr, reg, data_ptr);
}

/**
 * Write register from accelerometer.
 */
static inline int raw_write8(const int addr, const int reg, int data)
{
	return i2c_write8(I2C_PORT_ACCEL, addr, reg, data);
}

static int set_range(const struct motion_sensor_t *s,
				int range,
				int rnd)
{
	int ret, range_tbl_size;
	uint8_t reg_val, ctrl_reg;
	const struct accel_param_pair *ranges;

	ctrl_reg = BMI160_RANGE_REG(s->type);
	ranges = get_range_table(s->type, &range_tbl_size);
	reg_val = get_reg_val(range, rnd, ranges, range_tbl_size);

	ret = raw_write8(s->i2c_addr, ctrl_reg, reg_val);
	return ret;
}

static int get_range(const struct motion_sensor_t *s,
				int *range)
{
	int ret, ctrl_val, range_tbl_size;
	uint8_t ctrl_reg;
	const struct accel_param_pair *ranges;
	ranges = get_range_table(s->type, &range_tbl_size);
	ctrl_reg = BMI160_RANGE_REG(s->type);
	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	*range = get_engineering_val(ctrl_val, ranges, range_tbl_size);
	return ret;
}

static int set_resolution(const struct motion_sensor_t *s,
				int res,
				int rnd)
{
	/* Only one resolution, BMI160_RESOLUTION, so nothing to do. */
	return EC_SUCCESS;
}

static int get_resolution(const struct motion_sensor_t *s,
				int *res)
{
	*res = BMI160_RESOLUTION;
	return EC_SUCCESS;
}

static int set_data_rate(const struct motion_sensor_t *s,
				int rate,
				int rnd)
{
	int ret, val, odr_tbl_size;
	uint8_t ctrl_reg, reg_val;
	const struct accel_param_pair *data_rates;

	ctrl_reg = BMI160_RANGE_REG(s->type);
	data_rates = get_odr_table(s->type, &odr_tbl_size);
	reg_val = get_reg_val(rate, rnd, data_rates, odr_tbl_size);

	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &val);
	if (ret != EC_SUCCESS)
		goto accel_cleanup;

	val = (val & ~BMI160_ODR_MASK) | reg_val;
	ret = raw_write8(s->i2c_addr, ctrl_reg, val);


accel_cleanup:
	mutex_unlock(s->mutex);
	return EC_SUCCESS;
}

static int get_data_rate(const struct motion_sensor_t *s,
				int *rate)
{
	int ret, ctrl_val, odr_tbl_size;
	uint8_t ctrl_reg;
	const struct accel_param_pair *data_rates;
	ctrl_reg = BMI160_RANGE_REG(s->type);

	ret = raw_read8(s->i2c_addr, ctrl_reg, &ctrl_val);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	data_rates = get_odr_table(s->type, &odr_tbl_size);
	*rate = get_engineering_val(ctrl_val & BMI160_ODR_MASK,
			data_rates, odr_tbl_size);
	return EC_SUCCESS;
}

int normalize(const struct motion_sensor_t *s, vector_3_t v, uint8_t *data)
{
	int ret, range = 0;

	v[0] = ((int16_t)((data[1] << 8) | data[0]));
	v[1] = ((int16_t)((data[3] << 8) | data[2]));
	v[2] = ((int16_t)((data[5] << 8) | data[4]));

	ret = get_range(s, &range);
	if (ret)
		return EC_ERROR_UNKNOWN;

	v[0] *= range;
	v[1] *= range;
	v[2] *= range;

	/* normalize the accel scale: 1G = 1024 */
	if (MOTIONSENSE_TYPE_ACCEL == s->type) {
		v[0] >>= 5;
		v[1] >>= 5;
		v[2] >>= 5;
	} else {
		v[0] >>= 8;
		v[1] >>= 8;
		v[2] >>= 8;
	}
	return EC_SUCCESS;
}

#ifdef CONFIG_ACCEL_INTERRUPTS

#define TASK_EVENT_SENSOR_PENDING TASK_EVENT_CUSTOM(1)

void bmi160_interrupt(enum gpio_signal signal)
{
	CPRINTS("Interrupt from sensor: %d, %d!", signal,
			gpio_get_level(signal));
	task_set_event(TASK_ID_BMI160SENSE, TASK_EVENT_SENSOR_PENDING, 0);
}

static int config_interrupt(const struct motion_sensor_t *s)
{
	int ret, tmp;
	mutex_lock(s->mutex);

	/* configure int2 as an external input */
	/* Latch until intterupts */
	ret = raw_read8(s->i2c_addr, BMI160_INT_LATCH, &tmp);
	tmp |= BMI160_INT2_INPUT_EN | 0xF;
	ret = raw_write8(s->i2c_addr, BMI160_INT_LATCH, tmp);

	/* set flat interrupt */
	gpio_enable_interrupt(GPIO_BMI160_INT1_L);

	/* configure int1 as an interupt */
	ret = raw_write8(s->i2c_addr, BMI160_INT_OUT_CTRL,
		BMI160_INT_CTRL(1, OUTPUT_EN) |
		BMI160_INT_CTRL(1, EDGE_CTRL));

	/* Map Simple/Double Tap to int 1
	 * Map Flat interrupt to int 1
	 */
	ret = raw_write8(s->i2c_addr, BMI160_INT_MAP_REG(1),
		BMI160_INT_FLAT | BMI160_INT_D_TAP | BMI160_INT_S_TAP);

#if 0
	/* map fifo water mark to int 1 */
	ret = raw_write8(s->i2c_addr, BMI160_INT_FIFO_MAP,
		BMI160_INT_MAP(1, FWM));

	/* configure fifo watermark at 50% */
	ret = raw_write8(s->i2c_addr, BMI160_FIFO_CONFIG_0,
			512 / sizeof(uint32_t));
	ret = raw_write8(s->i2c_addr, BMI160_FIFO_CONFIG_1,
			BMI160_FIFO_TAG_TIME_EN |
			BMI160_FIFO_TAG_INT1_EN |
			BMI160_FIFO_TAG_INT2_EN |
			BMI160_FIFO_HEADER_EN |
			BMI160_FIFO_MAG_EN |
			BMI160_FIFO_ACC_EN |
			BMI160_FIFO_GYR_EN);
#endif

	/* Set double tap interrupt and fifo*/
	ret = raw_read8(s->i2c_addr, BMI160_INT_EN_0, &tmp);
	tmp |= BMI160_INT_FLAT_EN | BMI160_INT_D_TAP_EN | BMI160_INT_S_TAP_EN;
	ret = raw_write8(s->i2c_addr, BMI160_INT_EN_0, tmp);

#if 0
	ret = raw_read8(s->i2c_addr, BMI160_INT_EN_1, &tmp);
	tmp |= BMI160_INT_FWM_EN;
	ret = raw_write8(s->i2c_addr, BMI160_INT_EN_1, tmp);
#endif

	mutex_unlock(s->mutex);
	return ret;
}
static int set_interrupt(const struct motion_sensor_t *s,
			       unsigned int threshold)
{
	/* Currently unsupported. */
	return EC_ERROR_UNKNOWN;
}

enum fifo_state {
	FIFO_HEADER,
	FIFO_DATA_SKIP,
	FIFO_DATA_TIME,
	FIFO_DATA_CONFIG,
};


#define BMI160_FIFO_BUFFER 64
uint8_t buffer[BMI160_FIFO_BUFFER];
int buffer_time;
/*
 * Decode the header from the fifo.
 * Return 0 if we need further processing.
 */
int bmi160_decode_header(enum fifo_header hdr, uint8_t *bp, int *nb)
{
	if ((hdr & BMI160_FH_MODE_MASK) == BMI160_EMPTY &&
			(hdr & BMI160_FH_PARM_MASK) != 0) {
		int i;
		/* Fast past, we have data */
		/* do our own __builtin_popcount,
		 * __popcountsi2 missing
		 */
		int x = (hdr & 0x1c) >> 2;
		x = x - ((x >> 1) & 0x55555555);
		/* Every 2 bits holds the sum of every pair of bits */
		x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
		if ((bp + x * 6) > (buffer + BMI160_FIFO_BUFFER)) {
			/* frame is not complete, it
			 * will be retransmitted.
			 */
			bp = buffer + BMI160_FIFO_BUFFER;
			return 1;
		}
		for (i = BMI160_MAG; i <= BMI160_ACC; i++) {
			if (hdr & (1 << (i + 2))) {
				vector_3_t v;
				/* normalize: need a proper conversion */
				normalize(&motion_sensors[1 - i], v, bp);
				/*
				 * CPRINTF("%d: X: %4d, Y: %4d, Z: %4d ",
				 * i, v[X], v[Y], v[Z]);
				 */
				bp += 6;
			}
		}
		(*nb)++;
		if (hdr & BMI160_FH_EXT_MASK)
			CPRINTF("%s%s\n",
				(hdr & 0x1 ? "INT1" : ""),
				(hdr & 0x2 ? "INT2" : ""));
		return 1;
	} else {
		return 0;
	}
}

void bmi160_load_fifo(const struct motion_sensor_t *s)
{
	int tmp, fifo_length, done = 0;
	int nb = 0;
	/* Read fifo */
	raw_read8(s->i2c_addr, BMI160_FIFO_LENGTH_1, &tmp);
	fifo_length = tmp << 8;
	raw_read8(s->i2c_addr, BMI160_FIFO_LENGTH_0, &tmp);
	fifo_length |= tmp;
	CPRINTS("FIFO byte to read: %d", fifo_length);
	/* we want to collect the timestamp and last byte */
	fifo_length += 1 + 3 + 1;
	do {
		const uint8_t fifo = BMI160_FIFO_DATA;
		enum fifo_state state = FIFO_HEADER;
		uint8_t *bp = buffer;
		i2c_lock(I2C_PORT_ACCEL, 1);
		i2c_xfer(I2C_PORT_ACCEL, s->i2c_addr,
				&fifo, 1, buffer,
				BMI160_FIFO_BUFFER, I2C_XFER_SINGLE);
		i2c_lock(I2C_PORT_ACCEL, 0);
		while (!done && bp != buffer + BMI160_FIFO_BUFFER) {
			switch (state) {
			case FIFO_HEADER: {
				enum fifo_header hdr = *bp++;
				if (bmi160_decode_header(hdr, bp, &nb))
					continue;
				/* Other cases */
				hdr &= 0xdc;
				switch (hdr) {
				case BMI160_EMPTY:
					done = 1;
					break;
				case BMI160_SKIP:
					state = FIFO_DATA_SKIP;
					break;
				case BMI160_TIME:
					state = FIFO_DATA_TIME;
					break;
				case BMI160_CONFIG:
					state = FIFO_DATA_CONFIG;
					break;
				default:
					CPRINTS("Unknown header: 0x%02x", hdr);
				}
				break;
			}
			case FIFO_DATA_SKIP:
				CPRINTF("skipped %d frames\n", *bp++);
				state = FIFO_HEADER;
				break;
			case FIFO_DATA_CONFIG:
				CPRINTF("config change: 0x%02x\n", *bp++);
				state = FIFO_HEADER;
				break;
			case FIFO_DATA_TIME:
				if (bp + 3 > buffer + BMI160_FIFO_BUFFER) {
					bp = buffer + BMI160_FIFO_BUFFER;
					continue;
				}
				buffer_time = (bp[2] << 16) | (bp[1] << 8) |
					bp[0];
				/* CPRINTF("timestamp %d\n", buffertime); */
				state = FIFO_HEADER;
				bp += 3;
				break;
			default:
				CPRINTS("Unknown data: 0x%02x\n", *bp++);
				state = FIFO_HEADER;
			}
		}
	} while (!done);
	CPRINTS("%d vect", nb);
}

void bmi160_sense_task(void)
{
	int i, tmp, interrupt, iter;
	const struct motion_sensor_t *s = &motion_sensors[0];
	do {
		if (task_wait_event_mask(TASK_EVENT_SENSOR_PENDING, -1) ==
				TASK_EVENT_SENSOR_PENDING) {
			iter = 1;
			do {
				interrupt = 0;
				/* order is important */
				for (i = 0; i < 1; i++) {
					raw_read8(s->i2c_addr,
						  BMI160_INT_STATUS_0 + i,
						  &tmp);
					if (tmp)
						raw_write8(
							s->i2c_addr,
							BMI160_INT_STATUS_0 + i,
							0);
					interrupt |= (tmp & 0xFF) << (i * 8);
				}
				CPRINTS("INT_STATUS: 0x%08x - %d", interrupt,
					iter);
				bmi160_load_fifo(s);
				iter++;
			} while (interrupt != 0);
		}
	} while (1);
}
#endif

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->i2c_addr, BMI160_STATUS, &tmp);

	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	*ready = tmp & BMI160_DRDY_MASK(s->type);
	return EC_SUCCESS;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t data[6];
	uint8_t xyz_reg;
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
		v[0] = s->raw_xyz[0];
		v[1] = s->raw_xyz[1];
		v[2] = s->raw_xyz[2];
		return EC_SUCCESS;
	}

	xyz_reg = get_xyz_reg(s->type);

	/* Read 6 bytes starting at xyz_reg */
	i2c_lock(I2C_PORT_ACCEL, 1);
	ret = i2c_xfer(I2C_PORT_ACCEL, s->i2c_addr,
			&xyz_reg, 1, data, 6, I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_ACCEL, 0);

	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RD XYZ Error %d]",
			s->name, s->type, ret);
		return ret;
	}
	return normalize(s, v, data);
}

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;

	ret = raw_read8(s->i2c_addr, BMI160_CHIP_ID, &tmp);
	if (ret)
		return EC_ERROR_UNKNOWN;

	if (tmp != BMI160_CHIP_ID_MAJOR)
		return EC_ERROR_ACCESS_DENIED;


	/* To avoid gyro wakeup */
	raw_write8(s->i2c_addr, BMI160_PMU_TRIGGER, 0);

	raw_write8(s->i2c_addr, BMI160_CMD_REG,
			BMI150_CMD_MODE_NORMAL(s->type));
	msleep(30);
	/* set acc bandwith average 4 */
	/* set gyr bandwith normal */

	/* 100Hz */
	set_data_rate(s, 100000, 0);

	/* Fifo setup is done elsewhere */
	raw_write8(s->i2c_addr, BMI160_CMD_REG, BMI160_CMD_FIFO_FLUSH);
	msleep(30);

#ifdef CONFIG_ACCEL_INTERRUPTS
	config_interrupt(s);
#endif

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d odr:%d]\n",
			s->name, s->type, s->range, s->odr);
	return ret;
}

const struct accelgyro_drv bmi160_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = set_resolution,
	.get_resolution = get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.set_interrupt = set_interrupt,
#endif
};
