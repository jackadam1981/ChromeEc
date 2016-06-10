#ifndef __CROS_EC_BARO_BMP280_H
#define __CROS_EC_BARO_BMP280_H

/* change to 0xec for amenia, should be in board file */
/* This can change depending upon the way SDO is connected */

/*
 * The addr field of barometer_sensor support both SPI and I2C:
 *
 * +-------------------------------+---+
 * |    7 bit i2c address          | 0 |
 * +-------------------------------+---+
 */

/*
 * Bit 0: 0 - If SDO is connected to GND
 * Bit 0: 1 - If SDO is connected to Vddio
 */
#define BMP280_I2C_ADDRESS1 		(0x76) << 1
#define BMP280_I2C_ADDRESS2 		(0x77) << 1

#define BMP280_REG_COMMAND_CHIP_ID      0xD0
#define BMP280_REG_COMMAND_DEV_STATUS   0xF3
/************************************************/
/**\name	POWER MODE DEFINITION       */
/***********************************************/
/* Sensor Specific constants */
#define BMP280_SLEEP_MODE                    (0x00)
#define BMP280_FORCED_MODE                   (0x01)
#define BMP280_NORMAL_MODE                   (0x03)


struct bmp280_t {
 uint8_t chip_id;
 uint8_t dev_addr;

 uint8_t oversamp_pressure;
 uint8_t oversamp_temperature;
}

/* Sensor precision */

/* TODO: Move to the right place*/

int bmp280_init(const struct baro_sensor_t *s);
int bmp280_read_uncomp_pressure(int *v_uncomp_pressure);

#endif
