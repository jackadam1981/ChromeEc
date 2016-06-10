/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/** \mainpage
*
****************************************************************************
* Copyright (C) 2012 - 2015 Bosch Sensortec GmbH
*
* File : bmp280.h
*
* Date : 2015/03/27
*
* Revision : 2.0.4(Pressure and Temperature compensation code revision is 1.1)
*
* Usage: Sensor Driver for BMP280 sensor
*
****************************************************************************
*
* \section License
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
*
*   Redistributions in binary form must reproduce the above copyright
*   notice, this list of conditions and the following disclaimer in the
*   documentation and/or other materials provided with the distribution.
*
*   Neither the name of the copyright holder nor the names of the
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
* OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
* OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*
* The information provided is believed to be accurate and reliable.
* The copyright holder assumes no responsibility
* for the consequences of use
* of such information nor for any infringement of patents or
* other rights of third parties which may result from its use.
* No license is granted by implication or otherwise under any patent or
* patent rights of the copyright holder.
**************************************************************************/
/* BMP280 pressure and temperature module for Chrome EC */

#ifndef __CROS_EC_BARO_BMP280_H
#define __CROS_EC_BARO_BMP280_H

#include "barometer.h"

/*
 * The addr field of barometer_sensor support both SPI and I2C:
 *
 * +-------------------------------+---+
 * |    7 bit i2c address          | 0 |
 * +-------------------------------+---+
 */

/*
 * Bit 1 of 7-bit address: 0 - If SDO is connected to GND
 * Bit 1 of 7-bit address: 1 - If SDO is connected to Vddio
 */
#define BMP280_I2C_ADDRESS1		((0x76) << 1)
#define BMP280_I2C_ADDRESS2		((0x77) << 1)
/************************************************/
/**\name	DELAY TIME DEFINITION       */
/***********************************************/
#define T_INIT_MAX					(20)
/* 20/16 = 1.25 ms */
#define T_MEASURE_PER_OSRS_MAX				(37)
/* 37/16 = 2.3125 ms*/
#define T_SETUP_PRESSURE_MAX				(10)
/* 10/16 = 0.625 ms */
/************************************************/
/**\name	CALIBRATION PARAMETERS DEFINITION       */

/* TEMPERATURE_CALIB_DIG_T1_LSB_REG             (0x88)
   TEMPERATURE_CALIB_DIG_T1_MSB_REG             (0x89)
   TEMPERATURE_CALIB_DIG_T2_LSB_REG             (0x8A)
   TEMPERATURE_CALIB_DIG_T2_MSB_REG             (0x8B)
   TEMPERATURE_CALIB_DIG_T3_LSB_REG             (0x8C)
   TEMPERATURE_CALIB_DIG_T3_MSB_REG             (0x8D)
   PRESSURE_CALIB_DIG_P1_LSB_REG                (0x8E)
   PRESSURE_CALIB_DIG_P1_MSB_REG                (0x8F)
   PRESSURE_CALIB_DIG_P2_LSB_REG                (0x90)
   PRESSURE_CALIB_DIG_P2_MSB_REG                (0x91)
   PRESSURE_CALIB_DIG_P3_LSB_REG                (0x92)
   PRESSURE_CALIB_DIG_P3_MSB_REG                (0x93)
   PRESSURE_CALIB_DIG_P4_LSB_REG                (0x94)
   PRESSURE_CALIB_DIG_P4_MSB_REG                (0x95)
   PRESSURE_CALIB_DIG_P5_LSB_REG                (0x96)
   PRESSURE_CALIB_DIG_P5_MSB_REG                (0x97)
   PRESSURE_CALIB_DIG_P6_LSB_REG                (0x98)
   PRESSURE_CALIB_DIG_P6_MSB_REG                (0x99)
   PRESSURE_CALIB_DIG_P7_LSB_REG                (0x9A)
   PRESSURE_CALIB_DIG_P7_MSB_REG                (0x9B)
   PRESSURE_CALIB_DIG_P8_LSB_REG                (0x9C)
   PRESSURE_CALIB_DIG_P8_MSB_REG                (0x9D)
   PRESSURE_CALIB_DIG_P9_LSB_REG                (0x9E)
   PRESSURE_CALIB_DIG_P9_MSB_REG                (0x9F) */
/*****************************************************/

/*calibration parameters */
#define BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG	(0x88)

/************************************************/
/**\name	REGISTER ADDRESS DEFINITION       */
/***********************************************/
#define BMP280_CHIP_ID_REG                   (0xD0)  /*Chip ID Register */
#define BMP280_RST_REG                       (0xE0)  /*Softreset Register */
#define BMP280_STAT_REG                      (0xF3)  /*Status Register */
#define BMP280_CTRL_MEAS_REG                 (0xF4)  /*Ctrl Measure Register */
#define BMP280_CONFIG_REG                    (0xF5)  /*Configuration Register */
#define BMP280_PRESSURE_MSB_REG              (0xF7)  /*Pressure MSB Register */
#define BMP280_PRESSURE_LSB_REG              (0xF8)  /*Pressure LSB Register */
#define BMP280_PRESSURE_XLSB_REG             (0xF9)  /*Pressure XLSB Register */
#define BMP280_TEMPERATURE_MSB_REG           (0xFA)  /*Temperature MSB Reg */
#define BMP280_TEMPERATURE_LSB_REG           (0xFB)  /*Temperature LSB Reg */
#define BMP280_TEMPERATURE_XLSB_REG          (0xFC)  /*Temperature XLSB Reg */
/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR TEMPERATURE OVERSAMPLING */
/***********************************************/
/* Control Measurement Register */
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMP__POS             (5)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMP__MSK             (0xE0)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMP__LEN             (3)
/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR PRESSURE OVERSAMPLING */
/***********************************************/
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRES__POS             (2)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRES__MSK             (0x1C)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRES__LEN             (3)
/************************************************/
/**\name	POWER MODE DEFINITION       */
/***********************************************/
/* Sensor Specific constants */
#define BMP280_SLEEP_MODE                    (0x00)
#define BMP280_FORCED_MODE                   (0x01)
#define BMP280_NORMAL_MODE                   (0x03)
#define BMP280_SOFT_RESET_CODE               (0xB6)
/************************************************/
/**\name	STANDBY TIME DEFINITION       */
/***********************************************/
#define BMP280_STANDBY_TIME_1_MS              (0x00)
#define BMP280_STANDBY_TIME_63_MS             (0x01)
#define BMP280_STANDBY_TIME_125_MS            (0x02)
#define BMP280_STANDBY_TIME_250_MS            (0x03)
#define BMP280_STANDBY_TIME_500_MS            (0x04)
#define BMP280_STANDBY_TIME_1000_MS           (0x05)
#define BMP280_STANDBY_TIME_2000_MS           (0x06)
#define BMP280_STANDBY_TIME_4000_MS           (0x07)
/************************************************/
/**\name	OVERSAMPLING DEFINITION       */
/***********************************************/
#define BMP280_OVERSAMP_SKIPPED          (0x00)
#define BMP280_OVERSAMP_1X               (0x01)
#define BMP280_OVERSAMP_2X               (0x02)
#define BMP280_OVERSAMP_4X               (0x03)
#define BMP280_OVERSAMP_8X               (0x04)
#define BMP280_OVERSAMP_16X              (0x05)
/************************************************/
/**\name	WORKING MODE DEFINITION       */
/***********************************************/
#define BMP280_ULTRA_LOW_POWER_MODE          (0x00)
#define BMP280_LOW_POWER_MODE	             (0x01)
#define BMP280_STANDARD_RESOLUTION_MODE      (0x02)
#define BMP280_HIGH_RESOLUTION_MODE          (0x03)
#define BMP280_ULTRA_HIGH_RESOLUTION_MODE    (0x04)
/*************************************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION FOR POWER MODE */
/**************************************************************************/
#define BMP280_CTRL_MEAS_REG_POWER_MODE__POS              (0)
#define BMP280_CTRL_MEAS_REG_POWER_MODE__MSK              (0x03)
#define BMP280_CTRL_MEAS_REG_POWER_MODE__LEN              (2)
/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR STANDBY DURATION */
/***********************************************/
/* Configuration Register */
#define BMP280_CONFIG_REG_STANDBY_DURN__POS                 (5)
#define BMP280_CONFIG_REG_STANDBY_DURN__MSK                 (0xE0)
#define BMP280_CONFIG_REG_STANDBY_DURN__LEN                 (3)
/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR IIR FILTER */
/***********************************************/
#define BMP280_CONFIG_REG_FILTER__POS              (2)
#define BMP280_CONFIG_REG_FILTER__MSK              (0x1C)
#define BMP280_CONFIG_REG_FILTER__LEN              (3)
/***************************************************************/
/**\name	GET AND SET BITSLICE FUNCTIONS       */
/***************************************************************/
#define BMP280_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMP280_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))
/****************************************************/
/**\name	DEFINITIONS FOR ARRAY SIZE OF DATA   */
/***************************************************/
#define	BMP280_TEMPERATURE_DATA_SIZE		(3)
#define	BMP280_PRESSURE_DATA_SIZE		(3)
#define	BMP280_DATA_FRAME_SIZE			(6)
#define	BMP280_CALIB_DATA_SIZE			(24)

/* numeric definitions */
#define	BMP280_INVALID_DATA					(0)

/*******************************************************/
/*             GET DRIVER DATA			       */
/*******************************************************/
#define BMP280_GET_DATA(_s) \
	((struct bmp280_drv_data_t *)(_s)->drv_data)
/**************************************************************/
/**\name	STRUCTURE and ENUM DEFINITIONS                */
/**************************************************************/

enum bmp280_state {
	BMP280_NOT_READY,
	BMP280_INIT,
};

/**
 * struct bmp280_calib_param_t - Holds all device specific
 *                                calibration parameters
 *
 * @dig_T1 to dig_T3:   calibration Temp data
 * @dig_P1 to dig_P9:   calibration Pressure data
 * @t_fine:   calibration t_fine data
 *
 */
struct bmp280_calib_param_t {
	uint16_t dig_T1;
	int16_t dig_T2;
	int16_t dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2;
	int16_t dig_P3;
	int16_t dig_P4;
	int16_t dig_P5;
	int16_t dig_P6;
	int16_t dig_P7;
	int16_t dig_P8;
	int16_t dig_P9;

	int32_t t_fine;
};

/**
 * struct bmp280_t - This structure holds BMP280 initialization parameters
 * @calib_param:          calibration data
 * @state:		  state of the chip NOT_READY/INIT/etc
 * @chip_id:              chip ID of the sensor
 * @port:                 port address
 * @addr:                 i2c address
 * @mode:		  current working mode
 * @oversamp_pres:    pressure over sampling
 * @oversamp_temp: temperature over sampling
 */
struct bmp280_drv_data_t {

	struct  bmp280_calib_param_t calib_param;
	uint8_t state;
	uint8_t chip_id;
	uint8_t port;
	uint8_t addr;
	int     mode;
	int	rate;
	uint8_t oversamp_pres;
	uint8_t oversamp_temp;
};

/**************************************************************/
/**\name	FUNCTION DECLARATIONS                         */
/**************************************************************/

/**************************************************************/
/**\name	FUNCTION FOR  INTIALIZATION                   */
/**************************************************************/
/**
 *	bmp280_init: This function is used for initialize
 *	the bus read and bus write functions
 *      and assign the chip id and I2C address of the BMP280 sensor
 *	chip id is read in the register 0xD0 bit from 0 to 7
 *
 *	@s: baro sensor structure pointer.
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
int bmp280_init(struct bmp280_drv_data_t *data);

/**************************************************************/
/**\name	FUNCTION FOR READ UNCOMPENSATED PRESSURE     */
/**************************************************************/
/**
 *	bmp280_read_uncomppressure - read uncompensated pressure.
 *	in the registers 0xF7, 0xF8 and 0xF9
 *	@note 0xF7 -> MSB -> bit from 0 to 7
 *	@note 0xF8 -> LSB -> bit from 0 to 7
 *	@note 0xF9 -> LSB -> bit from 4 to 7
 *
 *
 *
 *	@v_uncomp_pressure_s32 : The value of uncompensated pressure
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
int bmp280_read_uncomp_pressure(struct bmp280_drv_data_t *data,
					int *v_uncomp_pressure);

/**************************************************************/
/**\name	FUNCTION FOR READ TRUE PRESSURE S32 OUTPUT    */
/**************************************************************/
/**
 *	bmp280_compensate_pressure - Reads actual pressure from
 *				     uncompensated pressure
 *	and returns the value in Pascal(Pa)
 *	@note Output value of "96386" equals 96386 Pa =
 *	963.86 hPa = 963.86 millibar
 *
 *
 *  @v_uncomp_pressure: value of uncompensated pressure
 *
 *  @return Returns the Actual pressure out put as s32
 *
*/
int bmp280_compensate_pressure(struct bmp280_drv_data_t *data,
					int v_uncomp_pressure);

/**************************************************************/
/**\name	FUNCTION FOR WORK MODE   */
/**************************************************************/
/**
 *	bmp280_set_work_mode -  This API is used to write
 *	                        the working mode of the sensor
 *
 *
 *  @work_mode : The value of work mode
 *   value      |  mode
 * -------------|-------------
 *    0         | BMP280_ULTRA_LOW_POWER_MODE
 *    1         | BMP280_LOW_POWER_MODE
 *    2         | BMP280_STANDARD_RESOLUTION_MODE
 *    3         | BMP280_HIGH_RESOLUTION_MODE
 *    4         | BMP280_ULTRA_HIGH_RESOLUTION_MODE
 *
 *  @return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/

int bmp280_set_work_mode(struct bmp280_drv_data_t *data,
				uint8_t work_mode);
/**************************************************************/
/**\name	FUNCTION FOR READ UNCOMPENSATED TEMPERATURE     */
/**************************************************************/
/**
 *	This API is used to read uncompensated temperature
 *	in the registers 0xFA, 0xFB and 0xFC
 *	@note 0xFA -> MSB -> bit from 0 to 7
 *	@note 0xFB -> LSB -> bit from 0 to 7
 *	@note 0xFC -> LSB -> bit from 4 to 7
 *
 *	@param v_uncomp_temperature : The uncompensated temperature.
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/

int bmp280_read_uncomp_temperature(struct bmp280_drv_data_t *data,
					int *v_uncomp_temperature);
/**************************************************************/
/**\name	FUNCTION FOR READ TRUE TEMPERATURE S32 OUTPUT    */
/**************************************************************/
/*!
 *	Reads actual temperature
 *	from uncompensated temperature
 *	@note Returns the value in 0.01 degree Centigrade
 *	@note Output value of "5123" equals 51.23 DegC.
 *
 *  @param v_uncomp_temperature : value of uncompensated temperature
 *
 *  @return Actual temperature output as int
 *
*/
int bmp280_compensate_temperature_int32(struct bmp280_drv_data_t *data,
					int v_uncomp_temperature);

extern const struct accelgyro_drv bmp280_drv;
extern struct bmp280_drv_data_t bmp280_drv_data;
#endif
