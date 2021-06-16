/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Modifications Copyright (C) 2021 Bosch Sensortec GmbH
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * BMA422 accelerometer module for Chrome EC
 * 3D digital accelerometer
 *
 * @file       accel_bma422.c
 * @date       05-11-2021
 * @version    v0.0.0.1
 */

#include "accelgyro.h"
#include "console.h"
#include "accelgyro_bmi_common.h"

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

#include "bma422.h"

#define CPUTS(outstr) cputs(CC_ACCEL, outstr)
#define CPRINTF(format, args...) cprintf(CC_ACCEL, format, ## args)
#define CPRINTS(format, args...) cprints(CC_ACCEL, format, ## args)

/******************************************************************************/
/*!                Sensor definition                                          */

/* Supported maximum read/write length for communication */
#define READ_WRITE_LEN  UINT8_C(8)

/*! Sensor initialization configuration. */
struct bma4_dev bma4_device;

/*!
 * @details Delay function in microseconds
 *
 * @param[in]  period   : time period in microseconds
 * @param[in]  intf_ptr : interface pointer from API wrapper
 *
 * @return None
 */
void bma4_delay_us(uint32_t period, void *intf_ptr)
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
 * @retval BMA4_OK for Success
 * @retval Non-zero for Failure
 */
BMA4_INTF_RET_TYPE bma4_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
		uint32_t len, void *intf_ptr)
{
	int8_t rslt;

	struct motion_sensor_t *s_ptr = (struct motion_sensor_t *)intf_ptr;

	rslt = bmi_read_n(s_ptr->port, s_ptr->i2c_spi_addr_flags, reg_addr, reg_data, len );

	if (rslt) {
		/* We have to actually return the error from the integrated environment and API
		 * will give out the COM_FAIL error here we already made use of API error code */
		return BMA4_E_COM_FAIL;
	}

	return BMA4_OK;
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
 * @retval BMA4_OK for Success
 * @retval Non-zero for Failure
 */
BMA4_INTF_RET_TYPE bma4_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
		uint32_t len, void *intf_ptr)
{
	int8_t rslt;

	struct motion_sensor_t *s_ptr = (struct motion_sensor_t *)intf_ptr;

	rslt = bmi_write_n(s_ptr->port, s_ptr->i2c_spi_addr_flags, reg_addr, reg_data, len);

	if (rslt) {
		/* We have to actually return the error from the integrated environment and API
		 * will give out the COM_FAIL error here we already made use of API error code */
		return BMA4_E_COM_FAIL;
	}

	return BMA4_OK;
}
/****************************************************************/
/*!
 * @details This API is used Read the sensor's current internal temperature.
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[out]    temp  :Pointer to store temperature in degrees Kelvin.
 *
 * @return EC_SUCCESS if successful
 * @return non zero value -> Fail
 */
int bma4_read_temp(const struct motion_sensor_t *s, int *temp)
{
	int8_t rslt;

	rslt = bma4_get_temperature(temp, BMA4_KELVIN, &bma4_device);

	if(rslt == BMA4_OK)
		return EC_SUCCESS;
	else
		return EC_ERROR_UNKNOWN;
}

/*!
 * @details API to request performing/entering calibration.
 *
 * @param[in,out] s  : Pointer to sensor data pointer.
 * @param[in] offset : offset to apply to raw data.
 * @param[in] temp   : temperature when calibration was done.
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bma4_perform_calib(struct motion_sensor_t *s,
			int enable)
{
	uint8_t index, device_position;
	int8_t rslt;
	struct bma4_accel_foc_g_value g_value_foc = {0};

	if (!enable)
		return EC_SUCCESS;

	/* We assume the device is laying flat for calibration */
	if (s->rot_standard_ref == NULL ||
	    (*s->rot_standard_ref)[2][2] > INT_TO_FP(0))
		device_position = BMA4_FOC_TARGET_POSITIVE_1G; //0
	else
		device_position = BMA4_FOC_TARGET_NEGATIVE_1G; //1

	for (index = X; index <= Z; index++) {

		//TODO:CHECK THIS REMAPPING AXES VALUES
		if (index == X) {
			g_value_foc.x = BMA4_ENABLE;
			g_value_foc.y = BMA4_DISABLE;
			g_value_foc.z = BMA4_DISABLE;
			g_value_foc.sign = device_position;
		}

		if (index == Y) {
			g_value_foc.x = BMA4_DISABLE;
			g_value_foc.y = BMA4_ENABLE;
			g_value_foc.z = BMA4_DISABLE;
			g_value_foc.sign = device_position;
		}

		if (index == Z) {
			g_value_foc.x = BMA4_DISABLE;
			g_value_foc.y = BMA4_DISABLE;
			g_value_foc.z = BMA4_ENABLE;
			g_value_foc.sign = device_position;
		}

	    /* Perform accelerometer FOC */
	    rslt = bma4_perform_accel_foc(&g_value_foc, &bma4_device);

	    if (rslt != BMA4_OK) {
	    	return rslt;
	    }
	    /* Delay after performing Accel FOC */
	    bma4_device.delay_us(30000, &bma4_device.intf_ptr);
	}

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
int bma4_get_offset(const struct motion_sensor_t *s,
			int16_t    *offset,
			int16_t    *temp)
{
	int8_t rslt;

	int offset_val[3];

	/* Offset values are written in the offset register */
    rslt = bma4_read_regs(BMA4_OFFSET_0_ADDR, (uint8_t *)offset_val, 3, &bma4_device);

	rotate(offset_val, *s->rot_standard_ref, offset_val);

	offset[X] = offset_val[X];
	offset[Y] = offset_val[Y];
	offset[Z] = offset_val[Z];

	*temp = (int16_t)EC_MOTION_SENSE_INVALID_CALIB_TEMP;

	if (rslt != BMA4_OK)
		return rslt;

	return EC_SUCCESS;
}

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
int bma4_set_offset(const struct motion_sensor_t *s,
			const int16_t    *offset,
			int16_t    temp)
{
	int8_t rslt;

	// TODO:CHECK WE CAN SET OFFSET LIKE THIS
	intv3_t offset_val = { offset[X], offset[Y], offset[Z] };

	rotate_inv(offset_val, *s->rot_standard_ref, offset_val);

    /* Offset values are written in the offset register */
    rslt = bma4_write_regs(BMA4_OFFSET_0_ADDR, (uint8_t *)offset_val, 3, &bma4_device);

	if (rslt != BMA4_OK)
		return rslt;

    return EC_SUCCESS;
}

/*!
 * @details This API is used set the scaling value
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[in]  scale   : scale to apply to raw data.
 * @param[in]  temp     : temperature when calibration was done.
 *
 * @return EC_SUCCESS if successful
 * @return non zero value -> Fail
 */
int bma4_set_scale(const struct motion_sensor_t *s, const uint16_t *scale,
		  int16_t temp)
{
	struct accelgyro_saved_data_t *data = BMA_GET_SAVED_DATA(s);

	data->scale[X] = scale[X];
	data->scale[Y] = scale[Y];
	data->scale[Z] = scale[Z];
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
int bma4_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
		  int16_t *temp)
{
	struct accelgyro_saved_data_t *data = BMA_GET_SAVED_DATA(s);

	scale[X] = data->scale[X];
	scale[Y] = data->scale[Y];
	scale[Z] = data->scale[Z];
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
static int bma4_get_data_rate(const struct motion_sensor_t *s)
{
	struct accelgyro_saved_data_t *data = BMA_GET_SAVED_DATA(s);

	return data->odr;
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
static int bma4_set_data_rate(const struct motion_sensor_t *s,
			int rate, int rnd)
{
	int normalized_rate;
	uint8_t reg_val;
	int8_t rslt;

	struct accelgyro_saved_data_t *data = BMA_GET_SAVED_DATA(s);

    /* Structure to define accelerometer configuration. */
	struct bma4_accel_config accel_conf;

	rslt = bmi_get_normalized_rate(s, rate, rnd,
				      &normalized_rate, &reg_val);
	/*
	 * Lock accel resource to prevent another task from attempting
	 * to write accel parameters until we are done.
	 */
	mutex_lock(s->mutex);

	/* Get the accel configurations */
    rslt = bma4_get_accel_config(&accel_conf, &bma4_device);

    if (rslt == BMA4_OK)
    {
        /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G) */
        accel_conf.odr = reg_val;

        /* Set the accel configurations */
        rslt = bma4_set_accel_config(&accel_conf, &bma4_device);

    	if (rslt == BMA4_OK) {
    		data->odr = normalized_rate;
    	}
    }

	mutex_unlock(s->mutex);

	return rslt;
}

/*!
 * @details getter method for sensor resolution
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 *
 * @return sensor resolution value
 */
int bma4_get_resolution(const struct motion_sensor_t *s)
{
	return bma4_device.resolution;
}

/*!
 * @details Setter method for the sensor range
 *
 * @param[in,out] s     : Pointer to sensor data pointer.
 * @param[in]    range  : Range of accel sensor (+/- G's)
 * @param[in]    rnd    : If true, it rounds up to nearest valid value
 *                        Otherwise, it rounds down
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bma4_set_range(struct motion_sensor_t *s, int range, int rnd)
{
    /* Structure to define accelerometer configuration. */
	struct bma4_accel_config accel_conf;
    uint8_t index;
    uint8_t sens_size = 0;
    int8_t rslt;

    /* Accel sensor range configuration */
	uint8_t acc_sensor_range[4][2] = {
			{2,BMA4_ACCEL_RANGE_2G} ,
			{4,BMA4_ACCEL_RANGE_4G} ,
			{8,BMA4_ACCEL_RANGE_8G} ,
			{16,BMA4_ACCEL_RANGE_16G} ,
	};

	mutex_lock(s->mutex);

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		sens_size = sizeof(acc_sensor_range) / sizeof(acc_sensor_range[0]);
	}

	for (index = 0 ; index < sens_size; index++) {
		if (range == acc_sensor_range[index][0]) {
			break;
		}
		if ((range < acc_sensor_range[index + 1][0]) ) {
			if (rnd) {
				index += 1 ;
				break;
			} else {
				break;
			}
		}
	}

	/* Get the accelerometer configurations */
	rslt = bma4_get_accel_config(&accel_conf, &bma4_device);

	if (rslt == BMA4_OK)
	{
		/* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G) */
		accel_conf.range = acc_sensor_range[index][1];

		/* Set the accel configuration */
		rslt = bma4_set_accel_config(&accel_conf, &bma4_device);
	}

	/* Now that we have set the range, update the driver's value. */
	if (rslt == BMA4_OK)
		s->current_range = acc_sensor_range[index][0];

	mutex_unlock(s->mutex);

	return rslt;
}

/*!
 * @details This API reads the three axis accelerations of an accelerometer
 *
 * @param[in,out] s  : Pointer to sensor data pointer.
 * @param[out]    v  : Vector to store acceleration (in units of LSB)
 *
 * @return EC_SUCCESS -> Success
 * @return non zero value -> Fail
 */
int bma4_read(const struct motion_sensor_t *s, intv3_t v)
{
	uint8_t drdy_status = 0;
	int8_t rslt;
	struct bma4_accel sensor_data;

	mutex_lock(s->mutex);

    rslt = bma4_get_accel_data_rdy(&drdy_status, &bma4_device);

	if (rslt != BMA4_OK)
		return rslt;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion sensor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (drdy_status != BMA4_ENABLE) {
		if (v != s->raw_xyz)
			memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
		return EC_SUCCESS;
	}

	/* Read accelerometer sensor data */
    rslt = bma4_read_accel_xyz(&sensor_data, &bma4_device);

	mutex_unlock(s->mutex);

	if (rslt != BMA4_OK) {
		(void)CPRINTS("%s: type:0x%X RD XYZ Error %d", s->name, s->type, rslt);
		return rslt;
	}

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		v[0] = sensor_data.x;
		v[1] = sensor_data.y;
		v[2] = sensor_data.z;
	}

	rotate(v, *s->rot_standard_ref, v);

	return EC_SUCCESS;
}

/*!
 * @details Initialize accelerometer BMA422
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

	/* This driver requires a mutex */
	ASSERT(s->mutex);

	/* Configure sensor interface parameters */
	bma4_device.intf = BMA4_I2C_INTF;
	bma4_device.bus_read = bma4_i2c_read;
	bma4_device.bus_write = bma4_i2c_write;
	bma4_device.delay_us = bma4_delay_us;
	bma4_device.variant = BMA42X_VARIANT;
	/* Assign device address to interface pointer */
	bma4_device.intf_ptr = s;
	/* Configure max read/write length (in bytes) (Supported length depends on target machine) */
	bma4_device.read_write_len = READ_WRITE_LEN; //TODO:Check this READ_WRITE_LEN

    /* Initialize bma422 sensor */
    rslt = bma422_init(&bma4_device);

    if (rslt == BMA4_OK)
    {
    	/* Chip ID read success */
        if (s->type == MOTIONSENSE_TYPE_ACCEL) {
        	/* TODO:Firmware download is done here but not
        	 * sure about rom_map and use of ram_buffers */
        	/* TODO: No need of features for lid sensor ?*/
//        	rslt = bma422_write_config_file(&bma4_device);
//        	if (rslt == BMA4_OK) {
                /* Enable the accelerometer */
                 rslt = bma4_set_accel_enable(BMA4_ENABLE, &bma4_device);
                 if (rslt != BMA4_OK){
                	 return EC_ERROR_HW_INTERNAL;
                 }
//        	}
        }

    } else if (rslt == BMA4_E_INVALID_SENSOR){
		return EC_ERROR_ACCESS_DENIED;
    } else {
		return EC_ERROR_UNKNOWN;
    }

	return sensor_init_done(s);
}

/* Accelerometer Gyroscope base driver structure */
const struct accelgyro_drv bma422_drv = {
	.init = init,
	.read = bma4_read,
	.set_range = bma4_set_range,
	.get_resolution = bma4_get_resolution,
	.set_data_rate = bma4_set_data_rate,
	.get_data_rate = bma4_get_data_rate,
	.get_scale = bma4_get_scale,
	.set_scale = bma4_set_scale,
	.set_offset = bma4_set_offset,
	.get_offset = bma4_get_offset,
	.perform_calib = bma4_perform_calib,
	.read_temp = bma4_read_temp,
};
