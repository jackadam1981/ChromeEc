/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Modifications Copyright (C) 2021 Bosch Sensortec GmbH
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMI323 accelerometer and gyroscope module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 *
 * @file       accel_bmi323.c
 * @date       05-19-2021
 * @version    v0.0.0.1
 */

#include "accelgyro.h"
#include "console.h"
#include "accelgyro_bmi_common.h"

//#include "bmi3/accelgyro_bmi3_config_tbin.h"
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

#include "bmi3x0.h"
#include "driver/accelgyro_bmi323_public.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

/******************************************************************************/
/*!                Sensor definition                                           */

#define READ_WRITE_LEN  UINT8_C(8)

/* Sensor initialization configuration. */
static struct bmi3x0_dev bmi3_device = { 0 };

STATIC_IF(CONFIG_ACCEL_FIFO) volatile uint32_t last_interrupt_timestamp;


/*!
 * @details Delay function in microseconds
 *
 * @param[in]  period   : time period in microseconds
 * @param[in]  intf_ptr : interface pointer from API wrapper
 *
 * @return None
 */
void bmi3_delay_us(uint32_t period, void *intf_ptr)
{
	usleep(period);
}

/*!
 * @brief Bus communication function for i2c reads which should be mapped to API
 *
 * @param[in] reg_addr       : Register address from which data is read.
 * @param[out] read_data     : Pointer to data buffer where read data is stored.
 * @param[in] len            : Number of bytes of data to be read.
 * @param[in, out] intf_ptr  : Void pointer that can enable the linking of descriptors
 *                             for interface related call backs.
 *
 * @retval BMI3X0_INTF_RET_SUCCESS for Success
 * @retval BMI3X0_E_COM_FAIL for Failure
 */
BMI3X0_INTF_RET_TYPE bmi3_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len,
		void *intf_ptr)
{
	int8_t ret;

	struct motion_sensor_t *s_ptr = (struct motion_sensor_t *)intf_ptr;

	ret = bmi_read_n(s_ptr->port, s_ptr->i2c_spi_addr_flags, reg_addr, reg_data, len );

	if (ret) {
		/* We have to actually return the error from the integrated environment and API
		 * will give out the COM_FAIL error here we already made use of API error code */
		return BMI3X0_E_COM_FAIL;
	}

	return BMI3X0_INTF_RET_SUCCESS;
}

/*!
 * @brief Bus communication function for i2c writes which should be mapped to API
 *
 * @param[in] reg_addr      : Register address to which the data is written.
 * @param[in] read_data     : Pointer to data buffer in which data to be written
 *                            is stored.
 * @param[in] len           : Number of bytes of data to be written.
 * @param[in, out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                            for interface related call backs
 *
 * @retval BMI3X0_INTF_RET_SUCCESS for Success
 * @retval BMI3X0_E_COM_FAIL for Failure
 */
BMI3X0_INTF_RET_TYPE bmi3_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
		void *intf_ptr)
{
	int8_t ret;

	struct motion_sensor_t *s_ptr = (struct motion_sensor_t *)intf_ptr;

	ret = bmi_write_n(s_ptr->port, s_ptr->i2c_spi_addr_flags, reg_addr, reg_data, length);

	if (ret) {
		/* We have to actually return the error from the integrated environment and API
		 * will give out the COM_FAIL error here we already made use of API error code */
		return BMI3X0_E_COM_FAIL;
	}

	return BMI3X0_INTF_RET_SUCCESS;

}
/****************************************************************/
#ifdef CONFIG_ACCEL_INTERRUPTS
/**
 * bmi323_interrupt - called when the sensor activates the interrupt line.
 *
 * This is a "top half" interrupt handler, it just asks motion sense ask
 * to schedule the "bottom half", ->irq_handler().
 */
void bmi323_interrupt(enum gpio_signal signal)
{
	if (IS_ENABLED(CONFIG_ACCEL_FIFO))
		last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE, CONFIG_ACCELGYRO_BMI323_INT_EVENT);
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

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
			(!(*event & CONFIG_ACCELGYRO_BMI323_INT_EVENT)))
		return EC_ERROR_NOT_HANDLED;

	//TODO:implement FIFO parsers


	if (IS_ENABLED(CONFIG_ACCEL_FIFO) && has_read_fifo)
		motion_sense_fifo_commit_data();

	return EC_SUCCESS;

}

#endif /* CONFIG_ACCEL_INTERRUPTS */

#ifdef BST_UNDER_DEV
/*!
 * @details API to set sensor offset
 *
 * @param[in,out] s  : Pointer to sensor data pointer.
 * @param[in] offset : offset to apply to raw data.
 * @param[in] temp   : temperature when calibration was done.
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bmi3_set_offset(const struct motion_sensor_t *s,
	       const int16_t *offset, int16_t temp)
{
	struct bmi323_drv_data *saved_data = (struct bmi323_drv_data *)(&s->drv_data);

	saved_data->offset[X] = offset[X];
	saved_data->offset[Y] = offset[Y];
	saved_data->offset[Z] = offset[Z];

	return EC_SUCCESS;
}

/*!
 * @details API to get sensor offset
 *
 * @param[in,out] s  : Pointer to sensor data pointer.
 * @param[in] offset : offset to apply to raw data.
 * @param[in] temp   : temperature when calibration was done.
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bmi3_get_offset(const struct motion_sensor_t *s,
	       int16_t *offset, int16_t *temp)
{
	struct bmi323_drv_data *saved_data = (struct bmi323_drv_data *)(&s->drv_data);

	offset[X] = saved_data->offset[X];
	offset[Y] = saved_data->offset[Y];
	offset[Z] = saved_data->offset[Z];
	*temp = (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP;

	return EC_SUCCESS;
}
#endif

/*!
 * @details This API is used set the scaling value
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[in]  scale    : scale to apply to raw data.
 * @param[in]  temp     : temperature when calibration was done.
 *
 * @return EC_SUCCESS if successful
 * @return non zero value -> Fail
 */
int bmi3_set_scale(const struct motion_sensor_t *s, const uint16_t *scale,
		  int16_t temp)
{
	struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	saved_data->scale[X] = scale[X];
	saved_data->scale[Y] = scale[Y];
	saved_data->scale[Z] = scale[Z];
	return EC_SUCCESS;
}

/*!
 * @details This API is used get the scaling value
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[out]  scale   : scale to apply to raw data.
 * @param[out]  temp    : temperature when calibration was done.
 *
 * @return EC_SUCCESS if successful
 * @return non zero value -> Fail
 */
int bmi3_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
		  int16_t *temp)
{
	struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	scale[X] = saved_data->scale[X];
	scale[Y] = saved_data->scale[Y];
	scale[Z] = saved_data->scale[Z];
	*temp = (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

/*!
 * @details This API is used to get the output data rate of the sensor
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 *
 * @return odr from driver data
 */
static int bmi3_get_data_rate(const struct motion_sensor_t *s)
{
	struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	return saved_data->odr;
}

/*!
 * @details This API is used to set the output data rate of the sensor
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[in]    rate   : Output data rate (units are milli-Hz)
 * @param[in]    rnd    : If true, it rounds up to nearest valid value
 *                        Otherwise, it rounds down
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
static int bmi3_set_data_rate(const struct motion_sensor_t *s,
			 int rate,
			 int rnd)
{
	int normalized_rate;
	uint8_t reg_val;
	int8_t rslt;
    /* Structure to define accelerometer configuration. */
    struct bmi3x0_sens_config config;

    struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	/* Configure the type of feature. */
    config.type = s->type;
    //TODO:check whether we can use from accelgyro_bmi_common files
	rslt = bmi_get_normalized_rate(s, rate, rnd,
				      &normalized_rate, &reg_val);

	if (rslt == BMI3X0_OK) {
		/*
		 * Lock accel resource to prevent another task from attempting
		 * to write accel parameters until we are done.
		 */
		mutex_lock(s->mutex);

		/* Get default configurations for the type of feature selected. */
		rslt = bmi3x0_get_sensor_config(&config, 1, &bmi3_device);

		if (rslt == BMI3X0_OK)
		{
			if (config.type == BMI3X0_ACCEL) {
				if (rate == 0) {
				/* Set the sensor in suspend mode */
					config.cfg.acc.acc_mode = BMI3X0_ACC_MODE_LOW_PWR;
					saved_data->odr = 0;
				} else if (saved_data->odr == 0) {
					/* Power mode changed from suspend to normal */
					config.cfg.acc.acc_mode = BMI3X0_ACC_MODE_NORMAL;
				}
				config.cfg.acc.odr = reg_val;
			}
			if (config.type == BMI3X0_GYRO) {
				if (rate == 0) {
					/* Set the sensor in suspend mode */
					config.cfg.gyr.gyr_mode = BMI3X0_GYR_MODE_SUSPEND;
					saved_data->odr = 0;
				} else if (saved_data->odr == 0) {
					/* Power mode changed from suspend to normal */
					config.cfg.gyr.gyr_mode = BMI3X0_GYR_MODE_NORMAL;
				}
				config.cfg.gyr.odr = reg_val;
			}

			/* Set the accel/gyro configurations. */
			rslt = bmi3x0_set_sensor_config(&config, 1, &bmi3_device);

			if (rslt == BMI3X0_OK) {
				saved_data->odr = normalized_rate;
			}
		}

		mutex_unlock(s->mutex);
	}

	return rslt;
}

/*!
 * @details getter method for sensor resolution
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 *
 * @return sensor resolution value
 */
int bmi3_get_resolution(const struct motion_sensor_t *s)
{
	return BMI3X0_16_BIT_RESOLUTION;
}

/*!
 * @details Setter method for the sensor range
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[in]    range  : Range of accel/gyro sensor (+/- G's)/(dps)
 * @param[in]    rnd    : If true, it rounds up to nearest valid value
 *                        Otherwise, it rounds down
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bmi3_set_range(struct motion_sensor_t *s, int range, int rnd)
{
    /* Structure to define accelerometer configuration. */
    struct bmi3x0_sens_config config;
    uint8_t index , sens_size = 0;
    int8_t rslt;

    int (*sensor_range)[2];

	int acc_sensor_range[5][2] = {
			{2,BMI3X0_ACC_RANGE_2G} ,
			{4,BMI3X0_ACC_RANGE_4G} ,
			{8,BMI3X0_ACC_RANGE_8G} ,
			{16,BMI3X0_ACC_RANGE_16G} ,
			{32,BMI3X0_ACC_RANGE_32G} ,
	};

	int gyr_sensor_range[8][2] = {
			{125 , BMI3X0_GYR_RANGE_125DPS } ,
			{250 , BMI3X0_GYR_RANGE_250DPS } ,
			{500 , BMI3X0_GYR_RANGE_500DPS } ,
			{1000 , BMI3X0_GYR_RANGE_1000DPS} ,
			{2000 , BMI3X0_GYR_RANGE_2000DPS} ,
			{4000 , BMI3X0_GYR_RANGE_4000DPS} ,
			{8000 , BMI3X0_GYR_RANGE_8000DPS} ,
			{16000 , BMI3X0_GYR_RANGE_16000DPS} ,
	};

	mutex_lock(s->mutex);

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		sens_size = sizeof(acc_sensor_range) / sizeof(acc_sensor_range[0]);
		sensor_range = acc_sensor_range;
	} else { //if (s->type == MOTIONSENSE_TYPE_GYRO) {
		sens_size = sizeof(gyr_sensor_range) / sizeof(gyr_sensor_range[0]);
		sensor_range = gyr_sensor_range;
	}

	for (index = 0 ; index < sens_size; index++) {
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

    /* Configure the type of feature. */
    config.type = s->type;

    /* Get default configurations for the type of feature selected. */
    rslt = bmi3x0_get_sensor_config(&config, 1, &bmi3_device);

    if (rslt == BMI3X0_OK)
    {
    	if (config.type == BMI3X0_ACCEL) {
    		/* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
    		config.cfg.acc.range = sensor_range[index][1];
    	}
    	if (config.type == BMI3X0_GYRO) {
    		config.cfg.gyr.range = sensor_range[index][1];
    	}

        /* Set the accel configurations. */
        rslt = bmi3x0_set_sensor_config(&config, 1, &bmi3_device);
    }

	/* Now that we have set the range, update the driver's value. */
	if (rslt == BMI3X0_OK)
		s->current_range = sensor_range[index][0];

	mutex_unlock(s->mutex);

	return rslt;
}

/*!
 * @details This API reads the three axis data of accel and gyro
 *
 * @param[in,out] s  : Pointer to sensor data pointer.
 * @param[out]    v  : Vector to store data (in units of LSB)
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bmi3_read(const struct motion_sensor_t *s, intv3_t v)
{
//	uint8_t data[6];
    uint8_t index_count;

	uint16_t status = 0;
	int8_t rslt;
	struct bmi3x0_sensor_data sensor_data = {0};

	struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	mutex_lock(s->mutex);

    rslt = bmi3x0_get_sensor_status(&status, &bmi3_device);

	if (rslt == BMI3X0_OK) {
		/*
		 * If sensor data is not ready, return the previous read data.
		 * Note: return success so that motion sensor task can read again
		 * to get the latest updated sensor data quickly.
		 */
		if (!(status & BMI3_DRDY_MASK(s->type))) {
			if (v != s->raw_xyz) {
				/* Copy the previous data*/
				memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
			}
			mutex_unlock(s->mutex);
			return EC_SUCCESS;
		}

		/* Select accel sensor */
		sensor_data.type = s->type; //BMI3X0_ACCEL; BMI3X0_GYRO

		/* Read 6 bytes starting at xyz_reg */
		rslt = bmi3x0_get_sensor_data(&sensor_data, 1, &bmi3_device);
	}

    mutex_unlock(s->mutex);

	if (rslt != BMI3X0_OK) {
		(void)CPRINTS("%s: type:0x%X RD XYZ Error %d", s->name, s->type, rslt);
		return rslt;
	}

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		v[0] = sensor_data.sens_data.acc.x;
		v[1] = sensor_data.sens_data.acc.y;
		v[2] = sensor_data.sens_data.acc.z;

	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		v[0] = sensor_data.sens_data.gyr.x;
		v[1] = sensor_data.sens_data.gyr.y;
		v[2] = sensor_data.sens_data.gyr.z;
	}

	rotate(v, *s->rot_standard_ref, v);

	for (index_count = X; index_count <= Z; index_count++)
		v[index_count] = SENSOR_APPLY_SCALE(v[index_count], saved_data->scale[index_count]);

	return EC_SUCCESS;
}

/*!
 * @details Initialize BMI323 sensor
 *
 * @param[in,out] s : Pointer to sensor data pointer.
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
static int init(struct motion_sensor_t *s)
{
    /* Status of API are returned to this variable. */
    int8_t rslt;
    uint8_t index_count;
//  struct bmi3x0_config_version version = { 0 };

	/* Store the sensor configurations */
    struct accelgyro_saved_data_t *saved_data = (struct accelgyro_saved_data_t *)BMI3_GET_SAVED_DATA(s);

	/* This driver requires a mutex */
	ASSERT(s->mutex);

	/* Configure sensor interface parameters */
	bmi3_device.intf = BMI3X0_I2C_INTF;
	bmi3_device.read = bmi3_i2c_read;
	bmi3_device.write = bmi3_i2c_write;
	/* Configure delay in microseconds */
	bmi3_device.delay_us = bmi3_delay_us;
	/* Assign device address to interface pointer */
	bmi3_device.intf_ptr = s;
	/* Configure max read/write length (in bytes) (Supported length depends on target machine) */
	bmi3_device.read_write_len = READ_WRITE_LEN;

    /* Initialize bmi3x0. */
    rslt = bmi3x0_init(&bmi3_device);

    if (rslt == BMI3X0_OK)
    {
    	/* Chip ID read success */
        if (s->type == MOTIONSENSE_TYPE_ACCEL) {
        	/* TODO:Config file upload is done here but not
        	 * sure about rom_map and use of ram_buffers */
        	/* TODO: Check if needed to do this
        	 rslt = bmi3x0_configure_enhanced_flexibility(&bmi3_device);

        	if (rslt == BMI3X0_OK) {
                rslt = bmi3x0_get_config_version(&version, &bmi3_device);
        	}
        	*/
        }

    } else if (rslt == BMI3X0_E_DEV_NOT_FOUND){
		return EC_ERROR_ACCESS_DENIED;
    } else {
		return EC_ERROR_UNKNOWN;
    }

	for (index_count = X; index_count <= Z; index_count++)
		saved_data->scale[index_count] = MOTION_SENSE_DEFAULT_SCALE;

	/*
	 * The sensor is in Suspend mode at init,
	 * so set data rate to 0.
	 */
	saved_data->odr = 0;

	//TODO: CONFIG_ACCEL_INTERRUPTS

	return sensor_init_done(s);
}

/* Accelerometer Gyroscope base driver structure */
const struct accelgyro_drv bmi3_drv = {
	.init = init,
	.read = bmi3_read,
	.set_range = bmi3_set_range,
	.get_resolution = bmi3_get_resolution,
	.set_data_rate = bmi3_set_data_rate,
	.get_data_rate = bmi3_get_data_rate,
//	.set_offset = bmi3_set_offset,
	.get_scale = bmi3_get_scale,
	.set_scale = bmi3_set_scale,
//	.get_offset = bmi3_get_offset,
//	.perform_calib = perform_calib,
//	.read_temp = bmi_read_temp,
#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif
#ifdef CONFIG_BODY_DETECTION
//	.get_rms_noise = bmi_get_rms_noise,
#endif
};
