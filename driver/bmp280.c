#include "i2c.h"
#include "console.h"
#include "barometer.h"
#include "driver/bmp280.h"
#include "common.h"
#include "stddef.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_BARO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_BARO, format, ## args)


/*
 * @brief This structure holds BMP280 initialization parameters
 */
struct bmp280_t bmp280_drv = {
	.chip_id = 0,
	.oversamp_pressure = 0,
	.oversamp_temperature = 0,
};

struct bmp280_t *p_bmp280_drv;

static int raw_read8(const int port, const int addr, const uint8_t reg,
					 int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;

	rv = i2c_read8(port, addr, reg, data_ptr);
	return rv;
}

/**
 * Read n bytes from barometer.
 */
static int raw_read_n(const int port, const int addr, const uint8_t reg,
		uint8_t *data_ptr, const int len)
{
	int rv = -EC_ERROR_PARAM1;

	i2c_lock(port, 1);
	rv = i2c_xfer(port, addr, &reg, 1,
			data_ptr, len, I2C_XFER_SINGLE);
	i2c_lock(port, 0);
	return rv;
}

/**
 *  * Write 8bit register from accelerometer.
 *   */
static int raw_write8(const int port, const int addr, const uint8_t reg, int data)
{
        int ret = -EC_ERROR_PARAM1;
	ret = i2c_write8(port, addr, reg, data);
	msleep(1);
	return ret;
}

/**
 *	@brief This API is used to
 *	calibration parameters used for calculation in the registers
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
 *	@retval -1 -> Error
 *
 *
*/

int bmp280_get_calib_param(void)
{
	int ret = 0;
	uint8_t a_data_u8[BMP280_CALIB_DATA_SIZE] =
					{0, 0, 0,
					 0, 0, 0,
					 0, 0, 0,
					 0, 0, 0,
					 0, 0, 0,
					 0, 0, 0,
					 0, 0, 0,
					 0, 0, 0};

	ret = raw_read_n(p_bmp280_drv->port, p_bmp280_drv->addr,
			BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG,
			a_data_u8,
			BMP280_PRESSURE_TEMPERATURE_CALIB_DATA_LENGTH);

			/* read calibration values*/
	p_bmp280_drv->calib_param.dig_T1 = (uint16_t)(((
		(uint16_t)((uint8_t)a_data_u8[
		BMP280_TEMPERATURE_CALIB_DIG_T1_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[
		BMP280_TEMPERATURE_CALIB_DIG_T1_LSB]);
	p_bmp280_drv->calib_param.dig_T2 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[
		BMP280_TEMPERATURE_CALIB_DIG_T2_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_TEMPERATURE_CALIB_DIG_T2_LSB]);
	p_bmp280_drv->calib_param.dig_T3 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[
		BMP280_TEMPERATURE_CALIB_DIG_T3_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_TEMPERATURE_CALIB_DIG_T3_LSB]);
	p_bmp280_drv->calib_param.dig_P1 = (uint16_t)(((
		(uint16_t)((uint8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P1_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P1_LSB]);
	p_bmp280_drv->calib_param.dig_P2 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P2_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P2_LSB]);
	p_bmp280_drv->calib_param.dig_P3 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P3_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P3_LSB]);
	p_bmp280_drv->calib_param.dig_P4 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P4_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P4_LSB]);
	p_bmp280_drv->calib_param.dig_P5 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P5_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P5_LSB]);
	p_bmp280_drv->calib_param.dig_P6 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P6_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P6_LSB]);
	p_bmp280_drv->calib_param.dig_P7 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[BMP280_PRESSURE_CALIB_DIG_P7_MSB]))
		<< BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P7_LSB]);
	p_bmp280_drv->calib_param.dig_P8 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[
		BMP280_PRESSURE_CALIB_DIG_P8_MSB])) <<
		BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P8_LSB]);
	p_bmp280_drv->calib_param.dig_P9 = (int16_t)(((
		(int16_t)((int8_t)a_data_u8[
		BMP280_PRESSURE_CALIB_DIG_P9_MSB])) <<
		BMP280_SHIFT_BIT_POSITION_BY_08_BITS)
		| a_data_u8[BMP280_PRESSURE_CALIB_DIG_P9_LSB]);
	return ret;
}
/*
 *	@brief This API used to set the
 *	Operational Mode from the sensor in the register 0xF4 bit 0 and 1
 *
 *
 *
 *	@param v_power_mode_u8 : The value of power mode value
 *  value            |   Power mode
 * ------------------|------------------
 *	0x00             | BMP280_SLEEP_MODE
 *	0x01 and 0x02    | BMP280_FORCED_MODE
 *	0x03             | BMP280_NORMAL_MODE
 *
 *  @return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/

static int set_power_mode(uint8_t power_mode)
{
	uint8_t val;
	int ret;

	if (power_mode <= BMP280_NORMAL_MODE) {
		val = (p_bmp280_drv->oversamp_temperature <<
			BMP280_SHIFT_BIT_POSITION_BY_05_BITS) +
			(p_bmp280_drv->oversamp_pressure <<
			BMP280_SHIFT_BIT_POSITION_BY_02_BITS)
			+ power_mode;

		ret = raw_write8(p_bmp280_drv->port, p_bmp280_drv->addr,
	 	BMP280_CTRL_MEAS_REG_POWER_MODE__REG, val);
	}

	 return ret;
}

/*
 *	@brief This API used to get the
 *	Operational Mode from the sensor in the register 0xF4 bit 0 and 1
 *
 *
 *
 *	@param v_power_mode_u8 : The value of power mode value
 *  value            |   Power mode
 * ------------------|------------------
 *	0x00             | BMP280_SLEEP_MODE
 *	0x01 and 0x02    | BMP280_FORCED_MODE
 *	0x03             | BMP280_NORMAL_MODE
 *
 *  @return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/

static int get_power_mode(int *power_mode)
{
	 int val;
	 int ret;

         ret = raw_read8(p_bmp280_drv->port, p_bmp280_drv->addr,
	 	BMP280_CTRL_MEAS_REG_POWER_MODE__REG, &val);

	 CPRINTF("Power mode = %d", val);
	 *power_mode =  BMP280_GET_BITSLICE(val,
			BMP280_CTRL_MEAS_REG_POWER_MODE);

	 return ret;
}

int bmp280_set_work_mode(uint8_t work_mode)
{
	uint8_t val = 0;
	int ret = -EC_ERROR_PARAM1;

	if(work_mode > BMP280_ULTRA_HIGH_RESOLUTION_MODE)
		return ret;

	switch(work_mode) {
		case BMP280_ULTRA_LOW_POWER_MODE:
			p_bmp280_drv->oversamp_temperature =
			BMP280_ULTRALOWPOWER_OVERSAMP_TEMPERATURE;
			p_bmp280_drv->oversamp_pressure =
			BMP280_ULTRALOWPOWER_OVERSAMP_PRESSURE;
			break;
		case BMP280_LOW_POWER_MODE:
			p_bmp280_drv->oversamp_temperature =
			BMP280_LOWPOWER_OVERSAMP_TEMPERATURE;
			p_bmp280_drv->oversamp_pressure =
			BMP280_LOWPOWER_OVERSAMP_PRESSURE;
			break;
		case BMP280_STANDARD_RESOLUTION_MODE:
			p_bmp280_drv->oversamp_temperature =
			BMP280_STANDARDRESOLUTION_OVERSAMP_TEMPERATURE;
			p_bmp280_drv->oversamp_pressure =
			BMP280_STANDARDRESOLUTION_OVERSAMP_PRESSURE;
			break;
		case BMP280_HIGH_RESOLUTION_MODE:
			p_bmp280_drv->oversamp_temperature =
			BMP280_HIGHRESOLUTION_OVERSAMP_TEMPERATURE;
			p_bmp280_drv->oversamp_pressure =
			BMP280_HIGHRESOLUTION_OVERSAMP_PRESSURE;
			break;
		case BMP280_ULTRA_HIGH_RESOLUTION_MODE:
			p_bmp280_drv->oversamp_temperature =
			BMP280_ULTRAHIGHRESOLUTION_OVERSAMP_TEMPERATURE;
			p_bmp280_drv->oversamp_pressure =
			BMP280_ULTRAHIGHRESOLUTION_OVERSAMP_PRESSURE;
			break;
	}
	val = BMP280_SET_BITSLICE(val,
	BMP280_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE,
	p_bmp280_drv->oversamp_temperature);

	val = BMP280_SET_BITSLICE(val,
	 BMP280_CTRL_MEAS_REG_OVERSAMP_PRESSURE,
	p_bmp280_drv->oversamp_pressure);

	ret = raw_write8(p_bmp280_drv->port,
	p_bmp280_drv->addr, BMP280_CTRL_MEAS_REG, val);

	return ret;
}
/**************************************************************/
/**\name	FUNCTION FOR STANDBY DURATION   */
/**************************************************************/
/*!
 *	@brief This API used to Read the
 *	standby duration time from the sensor in the register 0xF5 bit 5 to 7
 *
 *	@param v_standby_durn_u8 : The standby duration time value.
 *  value     |  standby duration
 * -----------|--------------------
 *    0x00    | BMP280_STANDBYTIME_1_MS
 *    0x01    | BMP280_STANDBYTIME_63_MS
 *    0x02    | BMP280_STANDBYTIME_125_MS
 *    0x03    | BMP280_STANDBYTIME_250_MS
 *    0x04    | BMP280_STANDBYTIME_500_MS
 *    0x05    | BMP280_STANDBYTIME_1000_MS
 *    0x06    | BMP280_STANDBYTIME_2000_MS
 *    0x07    | BMP280_STANDBYTIME_4000_MS
 *
 *
 *
 *  @return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
int bmp280_set_standby_durn(uint8_t standby_durn)
{
	/* variable used to return communication result*/
	int ret = -EC_ERROR_PARAM1;
	int val = 0;
	/* check the p_bmp280 struct pointer as NULL*/
	if (p_bmp280_drv == NULL)
		return ret;

	/* write the standby duration*/
	ret = raw_read8(p_bmp280_drv->port,
			p_bmp280_drv->addr,
			BMP280_CONFIG_REG_STANDBY_DURN__REG,
			&val);

	if (ret == EC_SUCCESS) {
		val = BMP280_SET_BITSLICE(val,
			BMP280_CONFIG_REG_STANDBY_DURN,
			standby_durn);

		ret =
		raw_write8(p_bmp280_drv->port,p_bmp280_drv->addr,
			   BMP280_CONFIG_REG_STANDBY_DURN__REG,
			   val);
	}
	return ret;
}

/*
 *	@brief This API is used to read uncompensated pressure.
 *	in the registers 0xF7, 0xF8 and 0xF9
 *	@note 0xF7 -> MSB -> bit from 0 to 7
 *	@note 0xF8 -> LSB -> bit from 0 to 7
 *	@note 0xF9 -> LSB -> bit from 4 to 7
 *
 *
 *
 *	@param v_uncomp_pressure_int32_t : The value of uncompensated pressure
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
int bmp280_read_uncomp_pressure(int *v_uncomp_pressure)
{

	int ret = 0;
	/* Array holding the MSB and LSb value
	a_data_u8[0] - Pressure MSB
	a_data_u8[1] - Pressure LSB
	a_data_u8[2] - Pressure LSB
	*/

	uint8_t a_data_u8[BMP280_PRESSURE_DATA_SIZE] = {0, 0, 0};

	ret = raw_read_n(p_bmp280_drv->port, p_bmp280_drv->addr,
		BMP280_PRESSURE_MSB_REG,
		a_data_u8, BMP280_PRESSURE_DATA_LENGTH);

	*v_uncomp_pressure = (int)(
		(((unsigned int)(a_data_u8[BMP280_PRESSURE_MSB_DATA]))
		<< BMP280_SHIFT_BIT_POSITION_BY_12_BITS) |
		(((unsigned int)(a_data_u8[BMP280_PRESSURE_LSB_DATA]))
		<< BMP280_SHIFT_BIT_POSITION_BY_04_BITS) |
		((unsigned int)a_data_u8[BMP280_PRESSURE_XLSB_DATA] >>
		BMP280_SHIFT_BIT_POSITION_BY_04_BITS));

	return ret;
}

/*
 *	@brief Reads actual pressure from uncompensated pressure
 *	and returns the value in Pascal(Pa)
 *	@note Output value of "96386" equals 96386 Pa =
 *	963.86 hPa = 963.86 millibar
 *
 *
 *
 *
 *  @param  v_uncomp_pressure_int32_t: value of uncompensated pressure
 *
 *
 *
 *  @return Returns the Actual pressure out put as int32_t
 *
*/
int bmp280_compensate_pressure(int v_uncomp_pressure)
{
	int v_x1_u32r = 0;
	int v_x2_u32r = 0;
	uint32_t v_pressure_u32 = 0;

	/* calculate x1*/
	v_x1_u32r = (((int)p_bmp280_drv->calib_param.t_fine)
		>> BMP280_SHIFT_BIT_POSITION_BY_01_BIT) -
		(int)64000;

	/* calculate x2*/
	v_x2_u32r = (((v_x1_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS) *
		(v_x1_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS))
		>> BMP280_SHIFT_BIT_POSITION_BY_11_BITS) *
		((int)p_bmp280_drv->calib_param.dig_P6);

	v_x2_u32r = v_x2_u32r + ((v_x1_u32r *
		((int)p_bmp280_drv->calib_param.dig_P5))
		<< BMP280_SHIFT_BIT_POSITION_BY_01_BIT);

	v_x2_u32r = (v_x2_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS) +
		(((int)p_bmp280_drv->calib_param.dig_P4)
		<< BMP280_SHIFT_BIT_POSITION_BY_16_BITS);

	/* calculate x1*/
	v_x1_u32r = (((p_bmp280_drv->calib_param.dig_P3 *
		(((v_x1_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS) *
		(v_x1_u32r >> BMP280_SHIFT_BIT_POSITION_BY_02_BITS))
		>> BMP280_SHIFT_BIT_POSITION_BY_13_BITS))
		>> BMP280_SHIFT_BIT_POSITION_BY_03_BITS) +
		((((int)p_bmp280_drv->calib_param.dig_P2) *
		v_x1_u32r) >> BMP280_SHIFT_BIT_POSITION_BY_01_BIT))
		>> BMP280_SHIFT_BIT_POSITION_BY_18_BITS;

	v_x1_u32r = ((((32768 + v_x1_u32r)) *
		((int)p_bmp280_drv->calib_param.dig_P1))
		>> BMP280_SHIFT_BIT_POSITION_BY_15_BITS);

	/* calculate pressure*/
	v_pressure_u32 = (((uint32_t)(((int)1048576) - v_uncomp_pressure) -
		(v_x2_u32r >> BMP280_SHIFT_BIT_POSITION_BY_12_BITS))) * 3125;

	/* check overflow*/
	if (v_pressure_u32 < 0x80000000)
		/* Avoid exception caused by division by zero */
		if (v_x1_u32r != BMP280_INIT_VALUE)
			v_pressure_u32 =
			(v_pressure_u32 << BMP280_SHIFT_BIT_POSITION_BY_01_BIT)
			/ ((uint32_t)v_x1_u32r);
		else
			return BMP280_INVALID_DATA;
	else
		/* Avoid exception caused by division by zero */
		if (v_x1_u32r != BMP280_INIT_VALUE)
			v_pressure_u32 = (v_pressure_u32 /
			(uint32_t)v_x1_u32r) * 2;
		else
			return BMP280_INVALID_DATA;
		/* calculate x1*/
		v_x1_u32r = (((int32_t)
		p_bmp280_drv->calib_param.dig_P9) *
			((int32_t)(((v_pressure_u32
			>> BMP280_SHIFT_BIT_POSITION_BY_03_BITS)
			* (v_pressure_u32 >> BMP280_SHIFT_BIT_POSITION_BY_03_BITS))
			>> BMP280_SHIFT_BIT_POSITION_BY_13_BITS)))
			>> BMP280_SHIFT_BIT_POSITION_BY_12_BITS;
		/* calculate x2*/
		v_x2_u32r = (((int32_t)(v_pressure_u32
			>> BMP280_SHIFT_BIT_POSITION_BY_02_BITS)) *
			((int32_t)p_bmp280_drv->calib_param.dig_P8))
			>> BMP280_SHIFT_BIT_POSITION_BY_13_BITS;
		/* calculate true pressure*/
		v_pressure_u32 = (uint32_t)
			((int32_t)v_pressure_u32 +
			((v_x1_u32r + v_x2_u32r +
			p_bmp280_drv->calib_param.dig_P7)
			>> BMP280_SHIFT_BIT_POSITION_BY_04_BITS));

	return v_pressure_u32;
}


int bmp280_init(const struct baro_sensor_t *s)
{
	int val = 0, ret;

	p_bmp280_drv = &bmp280_drv;
	if(!s) {
		CPRINTS("Failed to init barometer sensor\n");
		return -1;
	}

	/* Todo: Move this init appropriately */
	p_bmp280_drv->port = s->port;
	p_bmp280_drv->addr = s->addr;
	/* Todo: Remove this hardcoding */
	p_bmp280_drv->oversamp_pressure = BMP280_STANDARDRESOLUTION_OVERSAMP_PRESSURE;

	/* read chip id */			
        ret = raw_read8(p_bmp280_drv->port, p_bmp280_drv->addr, BMP280_CHIP_ID_REG, &val);
	if(ret) {
		CPRINTS("Baro Init: I2C error %d\n",ret);
	}
	CPRINTS("Baro Init: Chip ID 0x%x\n",val);

	/* read bmp280 calibration parameter */

	ret = bmp280_get_calib_param();

	/* get device status */
        raw_read8(p_bmp280_drv->port, p_bmp280_drv->addr, BMP280_STAT_REG, &val);
	CPRINTS("Baro Init: Device status %d\n",val);

	/* configure default settings */

	/* Set the power mode as NORMAL */
	ret = set_power_mode(BMP280_NORMAL_MODE);

	ret = get_power_mode(&val);
	CPRINTS("Baro power mode: %d\n",val);

	ret = bmp280_set_work_mode(BMP280_STANDARD_RESOLUTION_MODE);

	ret = bmp280_set_standby_durn(BMP280_STANDBY_TIME_125_MS);

	/* Read uncompensated pressure */
	ret = bmp280_read_uncomp_pressure(&val);
	CPRINTS("Baro uncomp pressure: %d\n",val);

	/* Get compensated pressure */
	val = bmp280_compensate_pressure(val);
	CPRINTS("Baro comp pressure: %d Pa \n",val);
	return 0;
}


