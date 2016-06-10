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
 *  * Write 8bit register from accelerometer.
 *   */
static int raw_write8(const int port, const int addr, const uint8_t reg, int data)
{
        int ret = -EC_ERROR_PARAM1;
	ret = i2c_write8(port, addr, reg, data);
	msleep(1);
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
		val =
		BMP280_SET_BITSLICE(val,
		BMP280_CONFIG_REG_STANDBY_DURN,
		standby_durn);

		ret =
		raw_write8(
				p_bmp280_drv->port,p_bmp280_drv->addr,
				BMP280_CONFIG_REG_STANDBY_DURN__REG,
				val);
	}
	return ret;
}

int bmp280_init(const struct baro_sensor_t *s)
{
	int val = 0, ret;

	p_bmp280_drv = &bmp280_drv;
	if(!s) {
		CPRINTS("Failed to init barometer sensor\n");
		return -1;
	}
	
	p_bmp280_drv->port = s->port;
	p_bmp280_drv->addr = s->addr;

	/* read chip id */			
        ret = raw_read8(p_bmp280_drv->port, p_bmp280_drv->addr, BMP280_CHIP_ID_REG, &val); 
	if(ret) {
		CPRINTS("Baro Init: I2C error %d\n",ret);
	}
	CPRINTS("Baro Init: Chip ID 0x%x\n",val);

	/* configure default settings */

	/* get device status */
        raw_read8(p_bmp280_drv->port, p_bmp280_drv->addr, BMP280_STAT_REG, &val); 
	CPRINTS("Baro Init: Device status %d\n",val);

	/* Set the power mode as NORMAL */
	ret = set_power_mode(BMP280_NORMAL_MODE);

	ret = get_power_mode(&val);
	CPRINTS("Baro power mode: %d\n",val);
	
	ret = bmp280_set_work_mode(BMP280_STANDARD_RESOLUTION_MODE);

	ret = bmp280_set_standby_durn(BMP280_STANDBY_TIME_125_MS);

	return 0;
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
 *	@param v_uncomp_pressure_s32 : The value of uncompensated pressure
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
	CPRINTF("Hello\n");
	*v_uncomp_pressure = 20;
	return 0;
}


