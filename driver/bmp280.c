#include "i2c.h"
#include "console.h"
#include "barometer.h"
#include "driver/bmp280.h"


#define CPRINTF(format, args...) cprintf(CC_BARO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_BARO, format, ## args)


/*
 * @brief This structure holds BMP280 initialization parameters
 */
struct bmp280_t bmp280_drv = {
	.chip_id = 0;
	.oversamp_pressure = 0;
	.oversamp_temperature = 0;
};

static int raw_read8(const int port, const int addr, const uint8_t reg,
					 int *data_ptr)
{
	int rv = -EC_ERROR_PARAM1;

	rv = i2c_read8(port, addr, reg, data_ptr);
	return rv;
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

int bmp280_set_power_mode(uint8_t power_mode)
{
  	
}

_/*!
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

int bmp280_get_power_mode(uint8_t power_mode)
{
  		 	
         raw_read8(s->port, s->addr, BMP280_REG_COMMAND_CHIP_ID, &val); 
}

int bmp280_init(const struct baro_sensor_t *s)
{
	int val = 0, ret;
	struct bmp280_t *p_bmp280_drv = &bmp280_drv;

	if(!s) {
		CPRINTS("Failed to init barometer sensor\n");i
		return -1;
	}
	
	/* read chip id */			
        ret = raw_read8(s->port, s->addr, BMP280_CHIP_ID_REG, &val); 
	
	if(ret) {
		CPRINTS("Baro Init: I2C error %d\n",ret);
	}
	CPRINTS("Baro Init: Chip ID 0x%x\n",val);

	/* get device status */
        raw_read8(s->port, s->addr, BMP280_STAT_REG, &val); 
	CPRINTS("Baro Init: Device status %d\n",val);

	/* configure default settings */
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


