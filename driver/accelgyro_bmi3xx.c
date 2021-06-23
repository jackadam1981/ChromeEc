/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI3XX accelerometer and gyroscope module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 */

#include "accelgyro.h"
#include "accelgyro_bmi3xx.h"
#include "accelgyro_bmi323.h"
#include "accelgyro_bmi_common.h"
#include "console.h"
#include "hwtimer.h"
#include "i2c.h"
#include "init_rom.h"
#include "math_util.h"
#include "motion_sense_fifo.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

/* Sensor definition */
STATIC_IF(CONFIG_ACCEL_FIFO) volatile uint32_t last_interrupt_timestamp;

static uint8_t bmi3_buffer[BMI3_FIFO_BUFFER];

static inline int bmi3_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, \
				const struct motion_sensor_t *s_ptr)
{
	return bmi_read_n(s_ptr->port, s_ptr->i2c_spi_addr_flags, reg_addr, reg_data, len);
}

static inline int bmi3_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, \
				 const struct motion_sensor_t *s_ptr)
{
	return bmi_write_n(s_ptr->port, s_ptr->i2c_spi_addr_flags, reg_addr, reg_data, length);
}

#ifdef CONFIG_ACCEL_INTERRUPTS
/**
 * bmi3xx_interrupt - called when the sensor activates the interrupt line.
 *
 * This is a "top half" interrupt handler, it just asks motion sense ask
 * to schedule the "bottom half", ->irq_handler().
 */
void bmi3xx_interrupt(enum gpio_signal signal)
{
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
	last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE, CONFIG_ACCELGYRO_BMI323_INT_EVENT);
}

static int config_interrupt(const struct motion_sensor_t *s)
{
	int comm_rslt;

	uint8_t reg_data[6] = {0};

	if (s->type != MOTIONSENSE_TYPE_ACCEL)
		return EC_SUCCESS;

	mutex_lock(s->mutex);

	/* Clear the FIFO using Flush command */
	reg_data[0] = BMI3_ENABLE;
	reg_data[1] = 0;
	comm_rslt = bmi3_i2c_write(BMI3_REG_FIFO_CTRL, reg_data, 2, s);

	if (comm_rslt == EC_SUCCESS) {
		/* Map FIFO water-mark and FIFO full to INT1 pin */
		/* TODO:CHECK IF WE HAVE TO READ BEFORE WRITE */

		comm_rslt |= bmi3_i2c_read(BMI3_REG_INT_MAP1, reg_data, 6, s);

		reg_data[5] = BMI3_SET_BITS(reg_data[5], BMI3_FWM_INT, \
					BMI3_INT1);

		reg_data[5] = BMI3_SET_BITS(reg_data[5], BMI3_FFULL_INT, \
					    BMI3_INT1);

		comm_rslt |= bmi3_i2c_write(BMI3_REG_INT_MAP1, &reg_data[2],\
					    4, s);

		if (comm_rslt == EC_SUCCESS) {
			/* Set FIFO water-mark to read data whenever available
			 */
			reg_data[0] = 1;
			reg_data[1] = 0;
			comm_rslt = bmi3_i2c_write(BMI3_REG_FIFO_WATERMARK, \
						   reg_data, 2, s);

			if (comm_rslt == EC_SUCCESS) {
				/* Get the previous configuration data */
				comm_rslt |= bmi3_i2c_read(BMI3_REG_IO_INT_CTRL, \
						reg_data, 6, s);
				reg_data[2] = BMI3_SET_BIT_POS0(reg_data[2], BMI3_INT1_LVL, BMI3_INT_ACTIVE_LOW);
				reg_data[2] = BMI3_SET_BITS(reg_data[2], BMI3_INT1_OD, BMI3_INT_PUSH_PULL);
				reg_data[2] = BMI3_SET_BITS(reg_data[2], BMI3_INT1_OUTPUT_EN, BMI3_INT_OUTPUT_ENABLE);
				reg_data[4] = BMI3_SET_BIT_POS0(reg_data[4], BMI3_INT_LATCH, BMI3_INT_LATCH_EN);

				/* Set the interrupt pin configurations and latch settings */
				comm_rslt |= bmi3_i2c_write(BMI3_REG_IO_INT_CTRL, &reg_data[2], 4, s);

				if (comm_rslt == EC_SUCCESS) {
					/* Set FIFO config to enable accel gyro data */
					reg_data[0] = 0;
					reg_data[1] = BMI3_FIFO_ACC_EN | BMI3_FIFO_GYR_EN;
					comm_rslt = bmi3_i2c_write(BMI3_REG_FIFO_CONF, reg_data, 2, s);
				}
			}
		}
	}

	mutex_unlock(s->mutex);

	return comm_rslt;
}

int bmi3_parse_fifo_data(struct motion_sensor_t *s, struct bmi3_fifo_frame 
			 *fifo_frame, uint32_t last_ts)
{
	/* Start index for FIFO parsing after dummy byte removal */
	size_t fifo_index = 2;
	/* Variable to store LSB value */
	uint16_t data_lsb;
	/* Variables to store MSB value */
	uint16_t data_msb;
	/* Variable to store dummy data value which will get in FIFO data */
	uint16_t dummy_data;

	uint16_t fifo_size = 0;

	struct ec_response_motion_sensor_data vect;

	bool observed[2];

	struct bmi3_fifo_data raw_data[NUM_OF_PRIMARY_SENSOR];

	uint8_t sens_cnt = 0;

	if (s->type != MOTIONSENSE_TYPE_ACCEL)
	return EC_SUCCESS;

//	if (!(data->flags & (BMI_FIFO_ALL_MASK << BMI_FIFO_FLAG_OFFSET))) {
//		/*
//		 * The FIFO was disabled while we were processing it.
//		 *
//		 * Flush potential left over:
//		 * When sensor is resumed, we won't read old data.
//		 */
//		bmi_write8(s->port, s->i2c_spi_addr_flags, BMI_CMD_REG(V(s)),
//			   BMI_CMD_FIFO_FLUSH);
//		return EC_SUCCESS;
//	}

	/* Parse the length of data read excluding dummybyte */
	fifo_size = fifo_frame->available_fifo_len - 2;

	while (fifo_size > 0) {
		observed[SENSOR_ACCEL] = false;
		observed[SENSOR_GYRO] = false;

		//XXX: If we are reading some constant 64 bytes every time then 0x80 may even come after
		/* SENSOR ACCEL IS ENABLED */
		if ((fifo_frame->available_fifo_sens & BMI3_FIFO_ACC_EN) && (fifo_size != 0)) {

			/* In-case of FIFO read fail it has only 0x8000 */
			if (fifo_size >= 2) {
				if (bmi3_buffer[fifo_index] == 0x00 && bmi3_buffer[fifo_index+1] == 0x80) {
					/* No more data in FIFO buffer */
					CPRINTS("DBG_CHUNK:at%d=0x80\n", fifo_index);
					break;
				}
			} else {
				observed[SENSOR_ACCEL] = false;
				observed[SENSOR_GYRO] = false;
				fifo_size = 0;
				CPRINTS("EOF_FAIL \n");
			}

			if (fifo_size >= BMI3_LENGTH_FIFO_ACC) {
				CPRINTS("A[%d]: %x %x\n"
						,fifo_index , bmi3_buffer[fifo_index+4],bmi3_buffer[fifo_index+5]);

				/* Accelerometer raw x data */
				data_lsb = bmi3_buffer[fifo_index++];
				data_msb = bmi3_buffer[fifo_index++];

				/* To store the dummy data */
				dummy_data = (uint16_t)((data_msb << 8) \
					| data_lsb);

				if (dummy_data != BMI3_FIFO_ACCEL_DUMMY_FRAME) {
					observed[SENSOR_ACCEL] = true;
					raw_data[SENSOR_ACCEL].x = \
						(int16_t)((data_msb << 8) \
						| data_lsb);
					/* Accelerometer raw y data */
					data_lsb = bmi3_buffer[fifo_index++];
					data_msb = bmi3_buffer[fifo_index++];
					raw_data[SENSOR_ACCEL].y = \
						(int16_t)((data_msb << 8) \
						| data_lsb);

					/* Accelerometer raw z data */
					data_lsb = bmi3_buffer[fifo_index++];
					data_msb = bmi3_buffer[fifo_index++];
					raw_data[SENSOR_ACCEL].z = \
						(int16_t)((data_msb << 8) \
						| data_lsb);
				} else {
					fifo_index = fifo_index + 4;
					observed[SENSOR_ACCEL] = false;
				}

				fifo_size -= BMI3_LENGTH_FIFO_ACC;
			} else {
				observed[SENSOR_ACCEL] = false;
				observed[SENSOR_GYRO] = false;
				fifo_size = 0;
			}
		}

		if ((fifo_frame->available_fifo_sens & BMI3_FIFO_GYR_EN) \
		    && (fifo_size != 0)) {

			/* In-case of FIFO read fail it has only 0x8000 */
			if (fifo_size >= 2) {
				if (bmi3_buffer[fifo_index] == 0x00 && \
				    bmi3_buffer[fifo_index+1] == 0x80) {
					break;
				}
			} else {
				observed[SENSOR_ACCEL] = false;
				observed[SENSOR_GYRO] = false;
				fifo_size = 0;
				CPRINTS("EOF_FAIL \n");
			}

			if (fifo_size >= BMI3_LENGTH_FIFO_GYR) {

				data_lsb = bmi3_buffer[fifo_index++];
				data_msb = bmi3_buffer[fifo_index++];

				/* To store the dummy data */
				dummy_data = (uint16_t)((data_msb << 8) \
						| data_lsb);
				if (dummy_data != BMI3_FIFO_GYRO_DUMMY_FRAME) {

					observed[SENSOR_GYRO] = true;

					raw_data[SENSOR_GYRO].x = \
						(int16_t)((data_msb << 8) \
						| data_lsb);

					/* Accelerometer raw y data */
					data_lsb = bmi3_buffer[fifo_index++];
					data_msb = bmi3_buffer[fifo_index++];
					raw_data[SENSOR_GYRO].y = \
						(int16_t)((data_msb << 8) \
						| data_lsb);

					/* Accelerometer raw z data */
					data_lsb = bmi3_buffer[fifo_index++];
					data_msb = bmi3_buffer[fifo_index++];
					raw_data[SENSOR_GYRO].z = \
						(int16_t)((data_msb << 8) | \
						data_lsb);
				} else {
					fifo_index = fifo_index + 4;
					observed[SENSOR_GYRO] = false;
				}

				fifo_size -= BMI3_LENGTH_FIFO_GYR;
			} else {
				observed[SENSOR_ACCEL] = false;
				observed[SENSOR_GYRO] = false;
				fifo_size = 0;
			}
		}

		for (sens_cnt = 0; sens_cnt < NUM_OF_PRIMARY_SENSOR; \
		     sens_cnt++) {

			if (observed[sens_cnt]) {

				struct motion_sensor_t *sens_output = s + \
								sens_cnt;

				//TODO:NORMALISE

				vect.data[X] = raw_data[sens_cnt].x;
				vect.data[Y] = raw_data[sens_cnt].y;
				vect.data[Z] = raw_data[sens_cnt].z;

				vect.flags = 0;

				/* TODO:check this s-motion_sensors */
				vect.sensor_num = sens_cnt;

				motion_sense_fifo_stage_data(&vect, \
						sens_output, 3, last_ts);
			}
		}
	}

	return EC_SUCCESS;
}
/**
 * irq_handler - bottom half of the interrupt stack.
 * Ran from the motion_sense task, finds the events that raised the interrupt.
 *
 * For now, we just print out. We should set a bitmask motion sense code will
 * act upon.
 */
static int irq_handler(struct motion_sensor_t *s,
		uint32_t *event)
{
	int8_t has_read_fifo = 0;
	int comm_rslt = 0;
	uint8_t reg_data[4];
	uint16_t int_status;
	uint16_t fifo_fill_level;

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
			(!(*event & CONFIG_ACCELGYRO_BMI323_INT_EVENT)))
	return EC_ERROR_NOT_HANDLED;

	/* Get the interrupt status */
	comm_rslt = bmi3_i2c_read(BMI3_REG_INT_STATUS_INT1, reg_data, 4, s);
	int_status = (uint16_t) reg_data[2] | ((uint16_t) reg_data[3] << 8);

	//TODO:implement FIFO parsers
	if ((comm_rslt == EC_SUCCESS) && ((int_status & BMI3_INT_STATUS_FWM) ||
					(int_status & BMI3_INT_STATUS_FFULL))) {

		struct bmi3_fifo_frame fifo_frame;
		fifo_frame.data = bmi3_buffer;
		fifo_frame.length = BMI3_FIFO_BUFFER;

		/* Get the FIFO frame configurations */
		comm_rslt = bmi3_i2c_read(BMI3_REG_FIFO_CONF, reg_data, 4, s);
		fifo_frame.available_fifo_sens = reg_data[3] & BMI3_FIFO_ALL_EN;

		/* Get the FIFO fill level in words */
		comm_rslt = bmi3_i2c_read(BMI3_REG_FIFO_FILL_LVL, \
					  reg_data, 4, s);

		reg_data[3] = BMI3_GET_BIT_POS0(reg_data[3], \
						BMI3_FIFO_FILL_LVL);

		fifo_fill_level = ((uint16_t)reg_data[3] << 8 | reg_data[2]);
		/* fifo_fill_level is in word count so (x2) also we add 2 more
		 * bytes for I2C dummy transaction
		 */
		fifo_frame.available_fifo_len = (fifo_fill_level * 2) + 2;

		/* Read FIFO data */
		comm_rslt = bmi3_i2c_read(BMI3_REG_FIFO_DATA, bmi3_buffer, \
					  fifo_fill_level, s);

		bmi3_parse_fifo_data(s, &fifo_frame, last_interrupt_timestamp);
		has_read_fifo = 1;
	}

	if (IS_ENABLED(CONFIG_ACCEL_FIFO) && has_read_fifo)
	motion_sense_fifo_commit_data();

	return EC_SUCCESS;
}
#endif /* CONFIG_ACCEL_INTERRUPTS */

int bmi3_read_temp(const struct motion_sensor_t *s, int *temp_ptr)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static int bmi3_perform_calib(struct motion_sensor_t *s, int enable)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int bmi3_get_offset(const struct motion_sensor_t *s, int16_t *offset,
		    int16_t *temp)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static int bmi3_set_offset(const struct motion_sensor_t *s,
		      const int16_t *offset,
		      int16_t    temp)
{
	return EC_ERROR_UNIMPLEMENTED;
}

#ifdef CONFIG_BODY_DETECTION
int bmi3_get_rms_noise(const struct motion_sensor_t *s)
{
	return EC_ERROR_UNIMPLEMENTED;
}
#endif

int bmi3_set_scale(const struct motion_sensor_t *s, const uint16_t *scale,
        int16_t temp)
{
	struct accelgyro_saved_data_t *saved_data = 
		(struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	saved_data->scale[X] = scale[X];
	saved_data->scale[Y] = scale[Y];
	saved_data->scale[Z] = scale[Z];
	return EC_SUCCESS;
}

int bmi3_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
		   int16_t *temp)
{
	struct accelgyro_saved_data_t *saved_data = 
		(struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	scale[X] = saved_data->scale[X];
	scale[Y] = saved_data->scale[Y];
	scale[Z] = saved_data->scale[Z];
	*temp = (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}


static int bmi3_get_data_rate(const struct motion_sensor_t *s)
{
	struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	return saved_data->odr;
}

static int bmi3_set_data_rate(const struct motion_sensor_t *s, \
			      int rate,int rnd)
{
	int normalized_rate;
	uint8_t reg_data[4];
	uint8_t reg_val;
	int ret;

	struct accelgyro_saved_data_t *saved_data = BMI_GET_SAVED_DATA(s);

	ret = bmi_get_normalized_rate(s, rate, rnd, &normalized_rate, &reg_val);

	if (ret == EC_SUCCESS) {
		/*
		 * Lock accel resource to prevent another task from attempting
		 * to write accel parameters until we are done.
		 */
		mutex_lock(s->mutex);

		/* Get default configurations for the type of feature selected. */
		ret = bmi3_i2c_read(BMI3_REG_ACC_CONF + s->type, reg_data, 4, 
				    s);

		if (ret == EC_SUCCESS) {
			if (s->type == MOTIONSENSE_TYPE_ACCEL) {
				if (rate == 0) {
					/* Set the sensor in suspend mode */
					reg_data[3] = 
						BMI3_SET_BITS(reg_data[3], \
						BMI3_POWER_MODE, \
						BMI3_ACC_MODE_LOW_PWR);

					saved_data->odr = 0;
				} else if (saved_data->odr == 0) {
					/* Power mode changed from suspend to
					 * normal
					 */
					reg_data[3] = BMI3_SET_BITS(reg_data[3],
						      BMI3_POWER_MODE,
						      BMI3_ACC_MODE_NORMAL);
				}
				/* Set accelerometer ODR */
				reg_data[2] = BMI3_SET_BIT_POS0(reg_data[2], \
						BMI3_SENS_ODR, reg_val);

			}
			if (s->type == MOTIONSENSE_TYPE_GYRO) {
				if (rate == 0) {
					/* Set the sensor in suspend mode */
					reg_data[3] = BMI3_SET_BITS(reg_data[3],
						      BMI3_POWER_MODE,
						      BMI3_GYR_MODE_SUSPEND);
					saved_data->odr = 0;
				} else if (saved_data->odr == 0) {
					/* Power mode changed from suspend to
					 * normal
					 */
					reg_data[3] = BMI3_SET_BITS(reg_data[3],
						      BMI3_POWER_MODE, 
						      BMI3_GYR_MODE_NORMAL);
				}
				reg_data[2] = BMI3_SET_BIT_POS0(reg_data[2], \
							BMI3_SENS_ODR, reg_val);
			}

			/* Set the accel/gyro configurations. */
			ret = bmi3_i2c_write(BMI3_REG_ACC_CONF + \
						s->type, &reg_data[2], 2, s);

			if (ret == EC_SUCCESS) {
				saved_data->odr = normalized_rate;
			}
		}

		mutex_unlock(s->mutex);
	}

	return ret;
}

int bmi3_get_resolution(const struct motion_sensor_t *s)
{
	return BMI3_16_BIT_RESOLUTION;
}

int bmi3_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	int ret;
	uint8_t index, sens_size = 0;
	uint8_t reg_data[4] = { 0 };
	int (*sensor_range)[2];

	int acc_sensor_range[5][2] = {
	        { 2, BMI3_ACC_RANGE_2G },
	        { 4, BMI3_ACC_RANGE_4G },
	        { 8, BMI3_ACC_RANGE_8G },
	        { 16, BMI3_ACC_RANGE_16G },
	        { 32, BMI3_ACC_RANGE_32G },
	};

	int gyr_sensor_range[8][2] = {
	        { 125, BMI3_GYR_RANGE_125DPS },
	        { 250, BMI3_GYR_RANGE_250DPS },
	        { 500, BMI3_GYR_RANGE_500DPS },
	        { 1000, BMI3_GYR_RANGE_1000DPS },
	        { 2000, BMI3_GYR_RANGE_2000DPS },
	        { 4000, BMI3_GYR_RANGE_4000DPS },
	        { 8000, BMI3_GYR_RANGE_8000DPS },
	        { 16000, BMI3_GYR_RANGE_16000DPS },
	};

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		sens_size = sizeof(acc_sensor_range) / \
			    sizeof(acc_sensor_range[0]);
		sensor_range = acc_sensor_range;
	} else {
		sens_size = sizeof(gyr_sensor_range) / \
			    sizeof(gyr_sensor_range[0]);
		sensor_range = gyr_sensor_range;
	}

	for (index = 0; index < sens_size; index++) {
		if (range == sensor_range[index][0]) {
			break;
		}
		if (range < sensor_range[index + 1][0]) {
			if (rnd) {
				index += 1;
				break;
			} else {
				break;
			}
		}
	}

	mutex_lock(s->mutex);

	/* 
	 * Read the range register from sensor for accelerometer/gyroscope
	 * s->type should have MOTIONSENSE_TYPE_ACCEL = 0 ;
	 * MOTIONSENSE_TYPE_GYRO = 1 
	 */
	ret = bmi_read_n(s->port, s->i2c_spi_addr_flags, 
			 BMI3_REG_ACC_CONF + s->type, reg_data, 4);

	if (ret == EC_SUCCESS) {
		/* Set accelerometer/Gyroscope range */
		/* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
		reg_data[2] = BMI3_SET_BITS(reg_data[2], BMI3_SENS_RANGE, \
					    sensor_range[index][1]);

		reg_data[3] = BMI3_SET_BITS(reg_data[3], BMI3_POWER_MODE, \
					    BMI3_ACC_MODE_NORMAL);

		/* Set the accel/gyro configurations. */
		ret = bmi_write_n(s->port, s->i2c_spi_addr_flags,
				  BMI3_REG_ACC_CONF + s->type,
				  &reg_data[2], 2);

		/* Now that we have set the range, update the driver's value. */
		if (ret == EC_SUCCESS)
			s->current_range = sensor_range[index][0];
	}

	mutex_unlock(s->mutex);  

	return ret;
}

int bmi3_read(const struct motion_sensor_t *s, intv3_t v)
{
	int ret;
	uint8_t reg_data[8] = { 0 };
	uint16_t status_val = 0;

	mutex_lock(s->mutex);

	/* Read the status register */
	ret = bmi_read_n(s->port, s->i2c_spi_addr_flags, BMI3_REG_STATUS,
			 reg_data, 4);

	if (ret == EC_SUCCESS) {
		status_val = (reg_data[2] | ((uint16_t)reg_data[3] << 8));
		/*
		 * If sensor data is not ready, return the previous read data.
		 * Note: return success so that motion sensor task can read
		 * again to get the latest updated sensor data quickly.
		 */
		if (!(status_val & BMI3_DRDY_MASK(s->type))) {
			if (v != s->raw_xyz)
				memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));

			mutex_unlock(s->mutex);

			return EC_SUCCESS;
		}

		if (s->type == MOTIONSENSE_TYPE_ACCEL) {
			/* Read the sensor data */
			ret = bmi_read_n(s->port, s->i2c_spi_addr_flags, \
					    BMI3_REG_ACC_DATA_X,reg_data, 8);
		} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
			/* Read the sensor data */
			ret = bmi_read_n(s->port, s->i2c_spi_addr_flags, \
					 BMI3_REG_GYR_DATA_X, reg_data, 8);
		}

		if (ret == EC_SUCCESS) {
			v[0] = ((int16_t)((reg_data[3] << 8) | reg_data[2]));
			v[1] = ((int16_t)((reg_data[5] << 8) | reg_data[4]));
			v[2] = ((int16_t)((reg_data[7] << 8) | reg_data[6]));

			rotate(v, *s->rot_standard_ref, v);
		}
	}

	mutex_unlock(s->mutex);

	return ret;
}

static int init(struct motion_sensor_t *s)
{
	/* Status of communication result */
	int ret = 0;
	uint8_t i;
	uint8_t reg_data[4] = { 0 };

	/* Store the sensor configurations */
	struct accelgyro_saved_data_t *saved_data = BMI_GET_SAVED_DATA(s);

	/* This driver requires a mutex */
	ASSERT(s->mutex);

	/* Reset bmi3 device */
	reg_data[0] = (uint8_t)(BMI3_CMD_SOFT_RESET & BMI3_SET_LOW_BYTE);
	reg_data[1] = (uint8_t)((BMI3_CMD_SOFT_RESET & BMI3_SET_HIGH_BYTE) \
				>> 8);

	ret = bmi_write_n(s->port, s->i2c_spi_addr_flags, BMI3_REG_CMD, 
			  reg_data, 2);

	RETURN_ERROR(ret);
	
	/* Delay of 2ms after soft reset*/
	msleep(2);

	/* Enable feature engine bit */
	reg_data[0] = BMI3_ENABLE;
	reg_data[1] = 0;

	ret = bmi_write_n(s->port, s->i2c_spi_addr_flags, \
			   BMI3_REG_FEATURE_ENGINE_GLOB_CTRL, \
			   reg_data, 2);

	RETURN_ERROR(ret);

	/* Read chip id */
	ret = bmi_read_n(s->port, s->i2c_spi_addr_flags, BMI3_REG_CHIP_ID, \
			 reg_data, 4);

	RETURN_ERROR(ret);

	if (reg_data[2] != BMI323_CHIP_ID) {
		return EC_ERROR_HW_INTERNAL;
	}

	for (i = X; i <= Z; i++)
		saved_data->scale[i] = MOTION_SENSE_DEFAULT_SCALE;

	/* The sensor is in Suspend mode at init, so set data rate to 0*/
	saved_data->odr = 0;

	if (IS_ENABLED(CONFIG_ACCEL_INTERRUPTS) \
	    && (s->type == MOTIONSENSE_TYPE_ACCEL))
		ret = config_interrupt(s);

	return sensor_init_done(s);
}

/* Accelerometer/Gyroscope base driver structure */
const struct accelgyro_drv bmi3xx_drv = {
	.init = init,
	.read = bmi3_read,
	.set_range = bmi3_set_range,
	.get_resolution = bmi3_get_resolution,
	.set_data_rate = bmi3_set_data_rate,
	.get_data_rate = bmi3_get_data_rate,
	.get_scale = bmi3_get_scale,
	.set_scale = bmi3_set_scale,
	.set_offset = bmi3_set_offset,
	.get_offset = bmi3_get_offset,
	.perform_calib = bmi3_perform_calib,
	.read_temp = bmi3_read_temp,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif
#ifdef CONFIG_BODY_DETECTION
	.get_rms_noise = bmi3_get_rms_noise,
#endif
};
