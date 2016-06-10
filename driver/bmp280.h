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
/************************************************/
/**\name	REGISTER ADDRESS DEFINITION       */
/***********************************************/
#define BMP280_CHIP_ID_REG                   (0xD0)  /*Chip ID Register */
#define BMP280_RST_REG                       (0xE0) /*Softreset Register */
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
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__POS             (5)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__MSK             (0xE0)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__LEN             (3)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__REG             \
(BMP280_CTRL_MEAS_REG)
/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR PRESSURE OVERSAMPLING */
/***********************************************/
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRESSURE__POS             (2)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRESSURE__MSK             (0x1C)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRESSURE__LEN             (3)
#define BMP280_CTRL_MEAS_REG_OVERSAMP_PRESSURE__REG             \
(BMP280_CTRL_MEAS_REG)

/************************************************/
/**\name	POWER MODE DEFINITION       */
/***********************************************/
/* Sensor Specific constants */
#define BMP280_SLEEP_MODE                    (0x00)
#define BMP280_FORCED_MODE                   (0x01)
#define BMP280_NORMAL_MODE                   (0x03)
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

#define BMP280_ULTRALOWPOWER_OVERSAMP_PRESSURE          BMP280_OVERSAMP_1X
#define BMP280_ULTRALOWPOWER_OVERSAMP_TEMPERATURE       BMP280_OVERSAMP_1X

#define BMP280_LOWPOWER_OVERSAMP_PRESSURE	             BMP280_OVERSAMP_2X
#define BMP280_LOWPOWER_OVERSAMP_TEMPERATURE	         BMP280_OVERSAMP_1X

#define BMP280_STANDARDRESOLUTION_OVERSAMP_PRESSURE     BMP280_OVERSAMP_4X
#define BMP280_STANDARDRESOLUTION_OVERSAMP_TEMPERATURE  BMP280_OVERSAMP_1X

#define BMP280_HIGHRESOLUTION_OVERSAMP_PRESSURE         BMP280_OVERSAMP_8X
#define BMP280_HIGHRESOLUTION_OVERSAMP_TEMPERATURE      BMP280_OVERSAMP_1X

#define BMP280_ULTRAHIGHRESOLUTION_OVERSAMP_PRESSURE       BMP280_OVERSAMP_16X
#define BMP280_ULTRAHIGHRESOLUTION_OVERSAMP_TEMPERATURE    BMP280_OVERSAMP_2X

/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR POWER MODE */
/***********************************************/
#define BMP280_CTRL_MEAS_REG_POWER_MODE__POS              (0)
#define BMP280_CTRL_MEAS_REG_POWER_MODE__MSK              (0x03)
#define BMP280_CTRL_MEAS_REG_POWER_MODE__LEN              (2)
#define BMP280_CTRL_MEAS_REG_POWER_MODE__REG             (BMP280_CTRL_MEAS_REG)
/************************************************/
/**\name	BIT LENGTH,POSITION AND MASK DEFINITION
FOR STANDBY DURATION */
/***********************************************/
/* Configuration Register */
#define BMP280_CONFIG_REG_STANDBY_DURN__POS                 (5)
#define BMP280_CONFIG_REG_STANDBY_DURN__MSK                 (0xE0)
#define BMP280_CONFIG_REG_STANDBY_DURN__LEN                 (3)
#define BMP280_CONFIG_REG_STANDBY_DURN__REG                 (BMP280_CONFIG_REG)
/***************************************************************/
/**\name	GET AND SET BITSLICE FUNCTIONS       */
/***************************************************************/
/* never change this line */
#define BMP280_DELAY_FUNC(delay_in_msec)\
		delay_func(delay_in_msec)

#define BMP280_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMP280_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))



struct bmp280_t {
 uint8_t chip_id;
 uint8_t port;
 uint8_t addr;

 uint8_t oversamp_pressure;
 uint8_t oversamp_temperature;
};

/* Sensor precision */

/* TODO: Move to the right place*/

int bmp280_init(const struct baro_sensor_t *s);
int bmp280_read_uncomp_pressure(int *v_uncomp_pressure);

#endif
