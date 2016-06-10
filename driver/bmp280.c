/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
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
#include "i2c.h"
#include "console.h"
#include "driver/bmp280.h"
#include "accelgyro.h"
#include "common.h"
#include "stddef.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_BARO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_BARO, format, ## args)

static int raw_read8(const int port, const int addr, const uint8_t reg,
					 int *data_ptr)
{
	return i2c_read8(port, addr, reg, data_ptr);
}

/**
 * Read n bytes from barometer.
 */
static int raw_read_n(const int port, const int addr, const uint8_t reg,
		uint8_t *data_ptr, const int len)
{
	int rv;

	i2c_lock(port, 1);
	rv = i2c_xfer(port, addr, &reg, 1,
			data_ptr, len, I2C_XFER_SINGLE);
	i2c_lock(port, 0);
	return rv;
}

/*
 * Write 8bit register from accelerometer.
 */
static int raw_write8(const int port, const int addr, const uint8_t reg,
		int data)
{
	return i2c_write8(port, addr, reg, data);
}


/*
 * This function is used to get calibration parameters used for
 * calculation in the registers
 *
 *  parameter | Register address |   bit
 *------------|------------------|----------------
 *	dig_T1    |  0x88 and 0x89   | from 0 : 7 to 8: 15
 *	dig_T2    |  0x8A and 0x8B   | from 0 : 7 to 8: 15
 *	dig_T3    |  0x8C and 0x8D   | from 0 : 7 to 8: 15
 *	dig_P1    |  0x8E and 0x8F   | from 0 : 7 to 8: 15
 *	dig_P2    |  0x90 and 0x91   | from 0 : 7 to 8: 15
 *	dig_P3    |  0x92 and 0x93   | from 0 : 7 to 8: 15
 *	dig_P4    |  0x94 and 0x95   | from 0 : 7 to 8: 15
 *	dig_P5    |  0x96 and 0x97   | from 0 : 7 to 8: 15
 *	dig_P6    |  0x98 and 0x99   | from 0 : 7 to 8: 15
 *	dig_P7    |  0x9A and 0x9B   | from 0 : 7 to 8: 15
 *	dig_P8    |  0x9C and 0x9D   | from 0 : 7 to 8: 15
 *	dig_P9    |  0x9E and 0x9F   | from 0 : 7 to 8: 15
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *
 */

static int bmp280_get_calib_param(struct bmp280_drv_data_t *data)
{
	int ret = 0;

	uint8_t a_data_u8[BMP280_CALIB_DATA_SIZE] = {0};

	ret = raw_read_n(data->port, data->addr,
			BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG,
			a_data_u8, BMP280_CALIB_DATA_SIZE);

	/* read calibration values*/
	data->calib_param.dig_T1 = (a_data_u8[1] << 8) | a_data_u8[0];
	data->calib_param.dig_T2 = (a_data_u8[3] << 8  | a_data_u8[2]);
	data->calib_param.dig_T3 = (a_data_u8[5] << 8) | a_data_u8[4];

	data->calib_param.dig_P1 = (a_data_u8[7] << 8) | a_data_u8[6];
	data->calib_param.dig_P2 = (a_data_u8[9] << 8) | a_data_u8[8];
	data->calib_param.dig_P3 = (a_data_u8[11] << 8) | a_data_u8[10];
	data->calib_param.dig_P4 = (a_data_u8[13] << 8) | a_data_u8[12];
	data->calib_param.dig_P5 = (a_data_u8[15] << 8) | a_data_u8[14];
	data->calib_param.dig_P6 = (a_data_u8[17] << 8) | a_data_u8[16];
	data->calib_param.dig_P7 = (a_data_u8[19] << 8) | a_data_u8[18];
	data->calib_param.dig_P8 = (a_data_u8[21] << 8) | a_data_u8[20];
	data->calib_param.dig_P9 = (a_data_u8[23] << 8) | a_data_u8[22];

	return ret;
}

/**
 * This API used to set the
 * Operational Mode from the sensor in the register 0xF4 bit 0 and 1
 *
 *  @return results of bus communication function
 *  @retval 0 -> Success
 *
 */

static int set_power_mode(struct bmp280_drv_data_t *data, uint8_t power_mode)
{
	int val, ret;

	if (power_mode <= BMP280_NORMAL_MODE) {
		val = (data->oversamp_temp << 5) +
			(data->oversamp_pres << 2) + power_mode;

		ret = raw_write8(data->port, data->addr,
			BMP280_CTRL_MEAS_REG, val);
	}

	 return ret;
}
#if 0
/* Ununsed */
static int get_power_mode(struct bmp280_drv_data_t *data, int *power_mode)
{
	int val, ret;

	ret = raw_read8(data->port, data->addr,
		BMP280_CTRL_MEAS_REG, &val);

	CPRINTF("Power mode = %d", val);
	*power_mode =  BMP280_GET_BITSLICE(val,
			BMP280_CTRL_MEAS_REG_POWER_MODE);

	return ret;
}
#endif
int bmp280_set_work_mode(struct bmp280_drv_data_t *data, uint8_t work_mode)
{
	int val = 0, ret;

	if (work_mode > BMP280_ULTRA_HIGH_RESOLUTION_MODE)
		return ret;

	switch (work_mode) {
	case BMP280_ULTRA_LOW_POWER_MODE:
		data->oversamp_temp = BMP280_OVERSAMP_1X;
		data->oversamp_pres = BMP280_OVERSAMP_1X;
		break;
	case BMP280_LOW_POWER_MODE:
		data->oversamp_temp = BMP280_OVERSAMP_1X;
		data->oversamp_pres = BMP280_OVERSAMP_2X;
		break;
	case BMP280_STANDARD_RESOLUTION_MODE:
		data->oversamp_temp = BMP280_OVERSAMP_1X;
		data->oversamp_pres = BMP280_OVERSAMP_4X;
		break;
	case BMP280_HIGH_RESOLUTION_MODE:
		data->oversamp_temp = BMP280_OVERSAMP_1X;
		data->oversamp_pres = BMP280_OVERSAMP_8X;
		break;
	case BMP280_ULTRA_HIGH_RESOLUTION_MODE:
		data->oversamp_temp = BMP280_OVERSAMP_2X;
		data->oversamp_pres = BMP280_OVERSAMP_16X;
		break;
	}

	val = BMP280_SET_BITSLICE(val,
			BMP280_CTRL_MEAS_REG_OVERSAMP_TEMP,
			data->oversamp_temp);

        val = BMP280_SET_BITSLICE(val,
			BMP280_CTRL_MEAS_REG_OVERSAMP_PRES,
			data->oversamp_pres);

	ret = raw_write8(data->port,
		data->addr, BMP280_CTRL_MEAS_REG, val);

	return ret;
}

/**
 * bmp280_set_standby_durn:  This API used to Read the
 * standby duration time from the sensor in the register
 * 0xF5 bit 5 to 7
 *
 * @return results of bus communication function
 * @retval 0 -> Success
 */
int bmp280_set_standby_durn(struct bmp280_drv_data_t *data,
				uint8_t standby_durn)
{
	/* variable used to return communication result*/
	int ret, val;

	/* write the standby duration*/
	ret = raw_read8(data->port, data->addr,
			BMP280_CONFIG_REG,
			&val);

	if (ret == EC_SUCCESS) {
		val = BMP280_SET_BITSLICE(val,
			BMP280_CONFIG_REG_STANDBY_DURN,
			standby_durn);

		ret = raw_write8(data->port, data->addr,
			   BMP280_CONFIG_REG, val);
	}
	return ret;
}

/**
 * bmp280_read_uncomp_temperature: used to read uncompensated temp
 * in the registers 0xFA, 0xFB and 0xFC
 * @note 0xFA -> MSB -> bit from 0 to 7
 * @note 0xFB -> LSB -> bit from 0 to 7
 * @note 0xFC -> LSB -> bit from 4 to 7
 *
 * @uncomp_temperature : The uncompensated temperature.
 *
 * @return results of bus communication function
 * @retval 0 -> Success
 * @retval -1 -> Error
 *
 *
 */
int bmp280_read_uncomp_temperature(struct bmp280_drv_data_t *data,
					int *uncomp_temperature)
{
	int ret;
	/* Array holding the MSB and LSb value
	a_data_u8[0] - Temperature MSB
	a_data_u8[1] - Temperature LSB
	a_data_u8[2] - Temperature LSB
	*/
	uint8_t a_data_u8[BMP280_TEMPERATURE_DATA_SIZE] = {0, 0, 0};

	/* read temperature data */
	ret = raw_read_n(data->port, data->addr,
			BMP280_TEMPERATURE_MSB_REG,
			a_data_u8, BMP280_TEMPERATURE_DATA_SIZE);

	*uncomp_temperature = (int32_t)((a_data_u8[0] << 12) |
					(a_data_u8[1] << 4) |
					(a_data_u8[2] >> 4));
	return ret;
}

/**
 * Reads actual temperature from uncompensated temperature
 * @note Returns the value in 0.01 degree Centigrade
 * @note Output value of "5123" equals 51.23 DegC.
 *
 * Algorithm from BMP280 Datasheet Rev 1.15 Section 8.2
 *
 */
int bmp280_compensate_temperature(struct bmp280_drv_data_t *data,
					int uncomp_temperature)
{
	int32_t var1 = 0;
	int32_t var2 = 0;
	int32_t temperature = 0;

	/* calculate true temperature*/
	/*calculate x1*/
	var1  = ((((uncomp_temperature >> 3) - ((int32_t)
		data->calib_param.dig_T1 << 1))) *
		((int32_t)data->calib_param.dig_T2)) >> 11;
	/*calculate x2*/
	var2  = (((((uncomp_temperature
		>> 4) - ((int32_t)data->calib_param.dig_T1)) *
		((uncomp_temperature >> 4) -
		((int32_t)data->calib_param.dig_T1))) >> 12) *
		((int32_t)data->calib_param.dig_T3)) >> 14;
	/*calculate t_fine*/
	data->calib_param.t_fine = var1 + var2;
	/*calculate temperature*/
	temperature  = (data->calib_param.t_fine * 5 + 128) >> 8;

	return temperature;
}

static void bmp280_compute_wait_time(struct bmp280_drv_data_t *data,
						uint8_t *waittime)
{
	*waittime = (T_INIT_MAX + T_MEASURE_PER_OSRS_MAX *
			(((1 << data->oversamp_temp) >> 1) +
			((1 << data->oversamp_pres) >> 1)) +
			(data->oversamp_pres ? T_SETUP_PRESSURE_MAX : 0)
			+ 15) / 16;

}

static int bmp280_read_uncomp_pressure_temperature(struct bmp280_drv_data_t *data,
						int *uncomp_pres, int *uncomp_temp)
{
	int ret;
	uint8_t a_data_u8[BMP280_DATA_FRAME_SIZE] = {0};

	ret = raw_read_n(data->port, data->addr,
			BMP280_PRESSURE_MSB_REG,
			a_data_u8, BMP280_DATA_FRAME_SIZE);

	*uncomp_pres = (int32_t)((a_data_u8[0] << 12) |
		     (a_data_u8[1] << 4) |
		     (a_data_u8[2] >> 4));

	*uncomp_temp = (int32_t)((a_data_u8[0] << 12) |
			(a_data_u8[1] << 4) |
			(a_data_u8[2] >> 4));

	return ret;
}

static int bmp280_get_forced_uncomp_pressure_temperature(struct bmp280_drv_data_t *data,
						int *uncomp_pres, int *uncomp_temp)
{
	int ret;
	uint8_t val = 0, waittime = 0;

	/* read pressure and temperature*/
	val = (data->oversamp_temp << 5) +
		(data->oversamp_pres << 2) + BMP280_FORCED_MODE;

	ret = raw_write8(data->port, data->addr,
			BMP280_CTRL_MEAS_REG, val);

	bmp280_compute_wait_time(data, &waittime);

	udelay(waittime * 1000);

	ret += bmp280_read_uncomp_pressure_temperature(data, uncomp_pres, uncomp_temp);
	return ret;
}
/*
 *	This API is used to read uncompensated pressure.
 *	in the registers 0xF7, 0xF8 and 0xF9
 *	@note 0xF7 -> MSB -> bit from 0 to 7
 *	@note 0xF8 -> LSB -> bit from 0 to 7
 *	@note 0xF9 -> LSB -> bit from 4 to 7
 *
 *	uncomp_pressure : The value of uncompensated pressure
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
int bmp280_read_uncomp_pressure(struct bmp280_drv_data_t *data,
				int *uncomp_pressure)
{

	int ret;
	/* Array holding the MSB and LSB value
	a_data_u8[0] - Pressure MSB
	a_data_u8[1] - Pressure LSB
	a_data_u8[2] - Pressure LSB
	*/

	uint8_t a_data_u8[BMP280_PRESSURE_DATA_SIZE] = {0, 0, 0};

	ret = raw_read_n(data->port, data->addr,
		BMP280_PRESSURE_MSB_REG,
		a_data_u8, BMP280_PRESSURE_DATA_SIZE);

	*uncomp_pressure = (int32_t)((a_data_u8[0] << 12) |
				     (a_data_u8[1] << 4) |
				     (a_data_u8[2] >> 4));

	return ret;
}

/*
 * Reads actual pressure from uncompensated pressure
 * and returns the value in Pascal(Pa)
 * @note Output value of "96386" equals 96386 Pa =
 * 963.86 hPa = 963.86 millibar
 *
 * Algorithm from BMP280 Datasheet Rev 1.15 Section 8.2
 *
*/
int bmp280_compensate_pressure(struct bmp280_drv_data_t *data,
					int uncomp_pressure)
{
	int var1, var2;
	uint32_t p = 0;

	/* calculate x1*/
	var1 = (((int32_t)data->calib_param.t_fine)
		>> 1) - 64000;
	/* calculate x2*/
	var2 = (((var1 >> 2) * (var1 >> 2)) >> 11)
		* ((int32_t)data->calib_param.dig_P6);
	var2 = var2 + ((var1 * ((int32_t)data->calib_param.dig_P5)) << 1);
	var2 = (var2 >> 2) + (((int32_t)data->calib_param.dig_P4) << 16);
	/* calculate x1*/
	var1 = (((data->calib_param.dig_P3 *
		(((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
		((((int32_t)data->calib_param.dig_P2) * var1) >> 1)) >> 18;
	var1 = ((((32768 + var1)) *
		((int32_t)data->calib_param.dig_P1)) >> 15);

	/* calculate pressure*/
	p = (((uint32_t)((1048576) - uncomp_pressure) -
		(var2 >> 12))) * 3125;

	/* check overflow*/
	if (p < 0x80000000) {
		/* Avoid exception caused by division by zero */
		if (var1 != 0)
			p = (p << 1) / ((uint32_t)var1);
		else
			return 0;
	} else {
		/* Avoid exception caused by division by zero */
		if (var1 != 0)
			p = (p / (uint32_t)var1) * 2;
		else
			return 0;
	}
	/* calculate x1*/
	var1 = (((int32_t)data->calib_param.dig_P9) *
		((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
	/* calculate x2*/
	var2 = (((int32_t)(p >> 2)) *
		((int32_t)data->calib_param.dig_P8)) >> 13;
	/* calculate true pressure*/
	p = (uint32_t)((int32_t)p + ((var1 + var2 +
		data->calib_param.dig_P7) >> 4));

	return p;
}

/* get device status */
int get_device_status(struct bmp280_drv_data_t *data)
{
	int val = 0;

	raw_read8(data->port, data->addr,
		BMP280_STAT_REG, &val);
	CPRINTS("Baro Init: Device status %d\n", val);
	return val;
}

#if 0
/* Unused */
/**
 * bmp280_set_soft_rst() - Used to reset the sensor
 * The value 0xB6 is written to the 0xE0 register
 * the device is reset using the
 * complete power-on-reset procedure.
 * Softreset can be easily set using bmp280_set_soft_rst().
 */

static int bmp280_set_soft_rst(struct bmp280_drv_data_t *data)
{
	/* variable used to return communication result*/
	int ret;
	int val = BMP280_SOFT_RESET_CODE;

	/* write soft reset */
	ret = raw_write8(data->port, data->addr,
			BMP280_RST_REG, val);
	return ret;
}

/**
 * bmp280_get_filter() - This API is used to reads filter setting
 * in the register 0xF5 bit 3 and 4
 *
 */
int bmp280_get_filter(struct bmp280_drv_data_t *data, uint8_t *v_value_u8)
{
	/* variable used to return communication result*/
	int ret, val;

	/* read filter*/
	ret = raw_read8(data->port, data->addr,
			BMP280_CONFIG_REG, &val);

	*v_value_u8 = BMP280_GET_BITSLICE(val, BMP280_CONFIG_REG_FILTER);
	return ret;
}

/**
 * @brief This API is used to write filter setting
 * in the register 0xF5 bit 3 and 4
 *
 */
int bmp280_set_filter(struct bmp280_drv_data_t *data, uint8_t v_value_u8)
{
	int ret, val = 0;

	/* write filter*/
	ret = raw_read8(data->port, data->addr,
			BMP280_CONFIG_REG, &val);
	if (ret == EC_SUCCESS) {
		val = BMP280_SET_BITSLICE(
			val, BMP280_CONFIG_REG_FILTER, v_value_u8);
		ret += raw_write8(data->port, data->addr,
			BMP280_CONFIG_REG, val);
	}
	return ret;
}
#endif

/**
 * bmp280_init() - Used to initialize barometer with default config
 *
 * @return results of bus communication function
 * @retval 0 -> Success
 */
int bmp280_init(struct bmp280_drv_data_t *data)
{
	int val = 0, ret = 0;

	if (!data) {
		CPRINTF("Baro init failed\n");
		return EC_ERROR_INVAL;
	}
	/* Read chip id */
	ret = raw_read8(data->port, data->addr,
					BMP280_CHIP_ID_REG, &val);
	if (ret) {
		CPRINTF("Baro Init: Unable to read chip ID err %d\n", ret);
		return ret;
	}
	CPRINTF("Baro Init: Chip ID 0x%x\n", val);

	/* Read bmp280 calibration parameter */
	ret = bmp280_get_calib_param(data);

	/* initialize settings if power mode is normal */
	if(data->mode == BMP280_NORMAL_MODE)
	{
		ret += set_power_mode(data, BMP280_NORMAL_MODE);
		ret += bmp280_set_work_mode(data,
					BMP280_STANDARD_RESOLUTION_MODE);
	} /* for forced mode this should be configured during every read */

	return ret;
}



/**
 *  These APIS are designed to use the existing motion_sensor
 *  task
 */
static int init(const struct motion_sensor_t *s)
{
	int ret;
	struct bmp280_drv_data_t *data;

	if (!s) {
		CPRINTF("Baro init: Invalid argument\n");
		return EC_ERROR_INVAL;
	}

	/* Initialize default barometer settings */
	data = BMP280_GET_DATA(s);
	data->port = s->port;
	data->addr = s->addr;
	data->oversamp_pres = BMP280_OVERSAMP_1X;
	data->oversamp_temp = BMP280_OVERSAMP_1X;

	ret = bmp280_init(data);

	if (ret)
		CPRINTF("[%T %s: set power mode Error %d]", s->name, ret);

	return ret;
}

static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	int ret;
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);
	int pres, temp;

	ret = bmp280_get_forced_uncomp_pressure_temperature(data, &pres, &temp);

	if (ret)
		return ret;

	v[0] = bmp280_compensate_pressure(data, pres);
	v[1] = v[2] = 0;

#ifdef BMP280_TEST_TEMP
	v[1] = bmp280_compensate_temperature(data, temp);
	CPRINTF("temperature value read is %d\n", v[1]);
#endif
	return ret;
}

static int set_data_rate(const struct motion_sensor_t *s, int rate,
							int rnd)
{
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);
	data->rate = rate;

	return EC_SUCCESS;
}

static int get_data_rate(const struct motion_sensor_t *s)
{
	/* Sensor in forced mode, rate is used by motion_sense */
	struct bmp280_drv_data_t *data = BMP280_GET_DATA(s);

	return data->rate;
}

const struct accelgyro_drv bmp280_drv = {
	.init = init,
	.read = read,
	.set_range = NULL,
	.get_range = NULL,
	.set_resolution = NULL,
	.get_resolution = NULL,
	.set_data_rate = set_data_rate,
	.get_data_rate = get_data_rate,
	.set_offset = NULL,
	.get_offset = NULL,
	.perform_calib = NULL,
};

struct bmp280_drv_data_t bmp280_drv_data = {
	.state = BMP280_NOT_READY,
	.chip_id = 0,
	.port = 0,
	.addr = 0,
	.mode = 0,
	.oversamp_pres = 0,
	.oversamp_temp = 0,
};
