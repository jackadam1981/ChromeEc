/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMI3XX gsensor module for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_BMI3XX_H
#define __CROS_EC_ACCELGYRO_BMI3XX_H

/* General Macro Definitions */
/* LSB and MSB mask definitions */
#define BMI3_SET_LOW_BYTE 0x00FF
#define BMI3_SET_HIGH_BYTE 0xFF00

/* For enable and disable */
#define BMI3_ENABLE 0x1
#define BMI3_DISABLE 0x0

/* Utility macros */
#define BMI3_SET_BITS(reg_data, bitname, data)         \
		     ((reg_data & ~(bitname##_MASK)) | \
		     ((data << bitname##_POS) & bitname##_MASK))

#define BMI3_GET_BITS(reg_data, bitname)               \
		     ((reg_data & (bitname##_MASK)) >> \
		     (bitname##_POS))

#define BMI3_SET_BIT_POS0(reg_data, bitname, data)         \
			 ((reg_data & ~(bitname##_MASK)) | \
			 (data & bitname##_MASK))

#define BMI3_GET_BIT_POS0(reg_data, bitname) \
			 (reg_data & (bitname##_MASK))

/* Defines mode of operation for Accelerometer */
#define BMI3_POWER_MODE_MASK 			0x70
#define BMI3_POWER_MODE_POS 			4

#define BMI3_SENS_ODR_MASK 			0x0F

/* Full scale, Resolution */
#define BMI3_SENS_RANGE_MASK 			0x70
#define BMI3_SENS_RANGE_POS 			4

#define BMI3_CHIP_ID_MASK

/* Map FIFO water-mark interrupt to either INT1 or INT2 or IBI */
#define BMI3_FWM_INT_MASK 			0x30
#define BMI3_FWM_INT_POS 			4

/* Map FIFO full interrupt to either INT1 or INT2 or IBI */
#define BMI3_FFULL_INT_MASK 			0xC0
#define BMI3_FFULL_INT_POS 			6

/* Configure level of INT1 pin */
#define BMI3_INT1_LVL_MASK 			0x01

/* Configure behavior of INT1 pin */
#define BMI3_INT1_OD_MASK 			0x02
#define BMI3_INT1_OD_POS 			1

/* Output enable for INT1 pin */
#define BMI3_INT1_OUTPUT_EN_MASK 		0x04
#define BMI3_INT1_OUTPUT_EN_POS 		2

/* Input enable for INT1 pin */
#define BMI3_INT1_INPUT_EN_MASK 		0x08
#define BMI3_INT1_INPUT_EN_POS			3

/*  Mask definitions for interrupt pin configuration */
#define BMI3_INT_LATCH_MASK 			0x0001

/* Current fill level of FIFO buffer
 * An empty FIFO corresponds to 0x000. The word counter may be reset by reading
 * out all frames from the FIFO buffer or when the FIFO is reset through
 * fifo_flush. The word counter is updated each time a complete frame was read
 * or written. */
#define BMI3_FIFO_FILL_LVL_MASK 		0x07

/* Chip-specific registers */
#define BMI3_REG_CHIP_ID			0x00
#define BMI3_REG_STATUS				0x02
#define BMI3_REG_ACC_DATA_X			0x03
#define BMI3_REG_GYR_DATA_X			0x06
#define BMI3_REG_INT_STATUS_INT1		0x0D
#define BMI3_REG_FIFO_FILL_LVL			0x15
#define BMI3_REG_FIFO_DATA			0x16
#define BMI3_REG_ACC_CONF			0x20
#define BMI3_REG_GYR_CONF			0x21
#define BMI3_REG_INT_MAP1			0x3A
#define BMI3_REG_FIFO_WATERMARK			0x35
#define BMI3_REG_FIFO_CONF			0x36
#define BMI3_REG_FIFO_CTRL			0x37
#define BMI3_REG_IO_INT_CTRL			0x38
#define BMI3_REG_FEATURE_ENGINE_GLOB_CTRL	0x40
#define BMI3_REG_CMD				0x7E
/* Sensor Specific macros */
#define BMI3_ADDR_I2C_PRIM			0x68
#define BMI3_ADDR_I2C_SEC			0x69
#define BMI3_CHIP_ID_PRIM			0x40
#define BMI3_CHIP_ID_SEC			0x41
#define BMI3_16_BIT_RESOLUTION			16
#define BMI3_CMD_SOFT_RESET			0xDEAF
/*  Accelerometer G Range */
#define BMI3_ACC_RANGE_2G			0x00
#define BMI3_ACC_RANGE_4G			0x01
#define BMI3_ACC_RANGE_8G			0x02
#define BMI3_ACC_RANGE_16G			0x03
#define BMI3_ACC_RANGE_32G			0x04
/*  Accelerometer power modes */
#define BMI3_ACC_MODE_DISABLE			0x00
#define BMI3_ACC_MODE_ULTRA_LOW_PWR		0X02
#define BMI3_ACC_MODE_LOW_PWR			0x03
#define BMI3_ACC_MODE_NORMAL			0X04
#define BMI3_ACC_MODE_HIGH_PERF			0x07
/* Gyroscope DPS Range */
#define BMI3_GYR_RANGE_125DPS			0x00
#define BMI3_GYR_RANGE_250DPS			0x01
#define BMI3_GYR_RANGE_500DPS			0x02
#define BMI3_GYR_RANGE_1000DPS			0x03
#define BMI3_GYR_RANGE_2000DPS			0x04
#define BMI3_GYR_RANGE_4000DPS			0x05
#define BMI3_GYR_RANGE_8000DPS			0x06
#define BMI3_GYR_RANGE_16000DPS			0x07
/*  Gyroscope power modes */
#define BMI3_GYR_MODE_DISABLE			0x00
#define BMI3_GYR_MODE_SUSPEND			0X01
#define BMI3_GYR_MODE_ULTRA_LOW_PWR		0X02
#define BMI3_GYR_MODE_LOW_PWR			0x03
#define BMI3_GYR_MODE_NORMAL			0X04
#define BMI3_GYR_MODE_HIGH_PERF			0x07
/* BMI3 Interrupt Pin latch settings */
#define BMI3_INT_LATCH_EN			1
#define BMI3_INT_LATCH_DISABLE			0
/* BMI3 Interrupt Pin Behavior */
#define BMI3_INT_PUSH_PULL			0
#define BMI3_INT_OPEN_DRAIN			1
/* BMI3 Interrupt Pin Level */
#define BMI3_INT_ACTIVE_LOW			0
#define BMI3_INT_ACTIVE_HIGH			1
/* BMI3 Interrupt Output Enable */
#define BMI3_INT_OUTPUT_DISABLE			0
#define BMI3_INT_OUTPUT_ENABLE			1
/* Mask definitions for FIFO frame content configuration */
#define BMI3_FIFO_STOP_ON_FULL			0x01
#define BMI3_FIFO_TIME_EN			0x01
#define BMI3_FIFO_ACC_EN			0x02
#define BMI3_FIFO_GYR_EN			0x04
#define BMI3_FIFO_TEMP_EN			0x08
#define BMI3_FIFO_ALL_EN			0x0F
/* FIFO sensor data lengths */
#define BMI3_LENGTH_FIFO_ACC			0x6
#define BMI3_LENGTH_FIFO_GYR			0x6
/* Macro to define accelerometer configuration value for FOC */
#define BMI3_FOC_ACC_CONF_VAL_LSB		0xB7
#define BMI3_FOC_ACC_CONF_VAL_MSB		0x40
/* Macro to define the accel FOC range */
#define BMI3_ACC_FOC_2G_REF			16384
#define BMI3_ACC_FOC_4G_REF			8192
#define BMI3_ACC_FOC_8G_REF			4096
#define BMI3_ACC_FOC_16G_REF			2048
#define BMI3_FOC_SAMPLE_LIMIT			128
/* 20ms delay for 50Hz ODR */
#define FOC_TRY_COUNT				5
#define FOC_DELAY				20
#define BMI3_INT_STATUS_FWM			0x4000
#define BMI3_INT_STATUS_FFULL			0x8000
#define BMI3_FIFO_ACC_LENGTH			6
#define BMI3_FIFO_GYR_LENGTH			6
#define BMI3_SENSOR_TIME_LENGTH			2
/* Masks for FIFO dummy data frames */
#define BMI3_FIFO_GYRO_DUMMY_FRAME		0x7f02
#define BMI3_FIFO_ACCEL_DUMMY_FRAME		0x7f01

/* Other definitions */
#define BMI3_FIFO_BUFFER			64

/* Enum to define interrupt lines */
enum bmi3_hw_int_pin {
	BMI3_INT_NONE,
	BMI3_INT1,
	BMI3_INT2,
	BMI3_I3C_INT,
	BMI3_INT_PIN_MAX
};

/* Structure to define FIFO frame configuration */
struct bmi3_fifo_frame {
	/* Pointer to FIFO data */
	uint8_t *data;

	/* Number of user defined bytes of FIFO to be read */
	uint16_t length;

	/* Enables type of data to be streamed - accelerometer,
	 *  gyroscope
	 */
	uint8_t available_fifo_sens;

	/* Water-mark level for water-mark interrupt */
	uint16_t wm_lvl;

	/* Available fifo length */
	uint16_t available_fifo_len;
};

typedef enum sensor_index_t {
	FIRST_CONT_SENSOR = 0,
	SENSOR_ACCEL = FIRST_CONT_SENSOR,
	SENSOR_GYRO,
	NUM_OF_PRIMARY_SENSOR,
} sensor_index_en;

/* Structure to define FIFO accel, gyro x, y and z axes */
struct bmi3_fifo_data {
	/* Data in x-axis */
	int16_t x;

	/* Data in y-axis */
	int16_t y;

	/* Data in z-axis */
	int16_t z;
};

struct bmi3xx_drv_data {
	struct accelgyro_saved_data_t saved_data[3];
	uint8_t flags;
	uint8_t enabled_activities;
	uint8_t disabled_activities;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

#define BMI3_GET_DATA(_s) \
	((struct bmi3xx_drv_data *)(_s)->drv_data)

#define BMI3_GET_SAVED_DATA(_s) \
	(&BMI3_GET_DATA(_s)->saved_data)

#define BMI3_DRDY_OFF(_sensor)   (7 - (_sensor))
#define BMI3_DRDY_MASK(_sensor)  (1 << BMI3_DRDY_OFF(_sensor))

extern const struct accelgyro_drv bmi3xx_drv;

void bmi3xx_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_ACCELGYRO_BMI3XX_H */
