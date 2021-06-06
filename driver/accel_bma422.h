/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMA422 gsensor module for Chrome EC */

#ifndef __CROS_EC_ACCEL_BMA422_H
#define __CROS_EC_ACCEL_BMA422_H

#include "accel_bma422.h"

/*
 * 7-bit address is 001111Xb. Where 'X' is determined
 * by the voltage on the ADDR pin.
 */
/* I2C addresses */
#define BMA422_ADDR0_FLAGS                        0x18

/* Chip-specific registers */
/**\name CHIP ID ADDRESS*/
#define BMA4_CHIP_ID_ADDR                         0x00
#define BMA422_CHIP_ID                            0x12

/**\name ERROR STATUS*/
#define BMA4_ERROR_ADDR                           0x02

/**\name STATUS REGISTER FOR SENSOR STATUS FLAG*/
#define BMA4_STATUS_ADDR                          0x03

/**\name AUX/ACCEL DATA BASE ADDRESS REGISTERS*/
#define BMA4_DATA_0_ADDR                          0x0A
#define BMA4_DATA_8_ADDR                          0x12

/**\name SENSOR TIME REGISTERS*/
#define BMA4_SENSORTIME_0_ADDR                    0x18

/**\name INTERRUPT/FEATURE STATUS REGISTERS*/
#define BMA4_INT_STAT_0_ADDR                      0x1C

/**\name INTERRUPT/FEATURE STATUS REGISTERS*/
#define BMA4_INT_STAT_1_ADDR                      0x1D

/**\name TEMPERATURE REGISTERS*/
#define BMA4_TEMPERATURE_ADDR                     0x22

/**\name FIFO REGISTERS*/
#define BMA4_FIFO_LENGTH_0_ADDR                   0x24
#define BMA4_FIFO_DATA_ADDR                       0x26

/**\name ACCEL CONFIG REGISTERS*/
#define BMA4_ACCEL_CONFIG_ADDR                    0x40

/**\name ACCEL RANGE ADDRESS*/
#define BMA4_ACCEL_RANGE_ADDR                     0x41

/**\name FEATURE CONFIG RELATED */
#define BMA4_RESERVED_REG_5B_ADDR                 0x5B
#define BMA4_RESERVED_REG_5C_ADDR                 0x5C
#define BMA4_FEATURE_CONFIG_ADDR                  0x5E
#define BMA4_INTERNAL_ERROR                       0x5F

/**\name SERIAL INTERFACE SETTINGS REGISTER*/
#define BMA4_IF_CONFIG_ADDR                       0x6B

/**\name Macro to define accelerometer configuration value for FOC */
#define BMA4_FOC_ACC_CONF_VAL                     0xB7

/**\name SPI,I2C SELECTION REGISTER*/
#define BMA4_NV_CONFIG_ADDR                       0x70

/**\name ACCEL OFFSET REGISTERS*/
#define BMA4_OFFSET_0_ADDR                        0x71
#define BMA4_OFFSET_1_ADDR                        0x72
#define BMA4_OFFSET_2_ADDR                        0x73

/**\name POWER_CTRL REGISTER*/
#define BMA4_POWER_CONF_ADDR                      0x7C
#define BMA4_POWER_CTRL_ADDR                      0x7D
#define BMA4_POWER_ACC_EC_MASK                    0x4


/**\name COMMAND REGISTER*/
#define BMA4_CMD_ADDR                             0x7E

/**\name GPIO REGISTERS*/
#define BMA4_STEP_CNT_OUT_0_ADDR                  0x1E
#define BMA4_HIGH_G_OUT_ADDR                      0x1F
#define BMA4_ACTIVITY_OUT_ADDR                    0x27
#define BMA4_ORIENTATION_OUT_ADDR                 0x28
#define BMA4_INTERNAL_STAT                        0x2A

/* Maximum length to read */
#define BMA4_MAX_LEN                              128

/*!
 * @brief Block size for config write
 */
#define BMA4_BLOCK_SIZE                           32

/**\name I2C slave address */
#define BMA4_I2C_ADDR_PRIMARY                     0x18
#define BMA4_I2C_ADDR_SECONDARY                   0x19
#define BMA4_I2C_BMM150_ADDR                      0x10

/**\name Interface selection macro */
#define BMA4_SPI_WR_MASK                          0x7F
#define BMA4_SPI_RD_MASK                          0x80

/**\name Chip ID macros */
#define BMA4_CHIP_ID_MIN                          0x10
#define BMA4_CHIP_ID_MAX                          0x15

/**\name Auxiliary sensor selection macro */
#define BMM150_SENSOR                             1
#define AKM9916_SENSOR                            2
#define BMA4_ASIC_INITIALIZED                     0x01

/**\name    Soft reset command */
#define BMA4_SOFT_RESET                           0xB6

/**\name    CONSTANTS */
#define BMA4_FIFO_CONFIG_LENGTH                   2
#define BMA4_ACCEL_CONFIG_LENGTH                  2
#define BMA4_FIFO_WM_LENGTH                       2
#define BMA4_NON_LATCH_MODE                       0
#define BMA4_LATCH_MODE                           1
#define BMA4_OPEN_DRAIN                           1
#define BMA4_PUSH_PULL                            0
#define BMA4_ACTIVE_HIGH                          1
#define BMA4_ACTIVE_LOW                           0
#define BMA4_EDGE_TRIGGER                         1
#define BMA4_LEVEL_TRIGGER                        0
#define BMA4_OUTPUT_ENABLE                        1
#define BMA4_OUTPUT_DISABLE                       0
#define BMA4_INPUT_ENABLE                         1
#define BMA4_INPUT_DISABLE                        0

/**\name ACCEL RANGE CHECK*/
#define BMA4_ACCEL_RANGE_2G                       0
#define BMA4_ACCEL_RANGE_4G                       1
#define BMA4_ACCEL_RANGE_8G                       2
#define BMA4_ACCEL_RANGE_16G                      3


/**\name BUS READ AND WRITE LENGTH FOR MAG & ACCEL*/
#define BMA4_ACCEL_DATA_LENGTH                    6
#define BMA4_FIFO_DATA_LENGTH                     2
#define BMA4_TEMP_DATA_SIZE                       1

/**\name TEMPERATURE CONSTANT */
#define BMA4_OFFSET_TEMP                          23
#define BMA4_DEG                                  1
#define BMA4_FAHREN                               2
#define BMA4_KELVIN                               3

/**\name DELAY DEFINITION IN MSEC*/
#define BMA4_AUX_IF_DELAY                         5
#define BMA4_BMM150_WAKEUP_DELAY1                 2
#define BMA4_BMM150_WAKEUP_DELAY2                 3
#define BMA4_BMM150_WAKEUP_DELAY3                 1
#define BMA4_GEN_READ_WRITE_DELAY                 1000
#define BMA4_AUX_COM_DELAY                        10000

/**\name    ARRAY PARAMETER DEFINITIONS*/
#define BMA4_SENSOR_TIME_MSB_BYTE                 2
#define BMA4_SENSOR_TIME_XLSB_BYTE                1
#define BMA4_SENSOR_TIME_LSB_BYTE                 0
#define BMA4_MAG_X_LSB_BYTE                       0
#define BMA4_MAG_X_MSB_BYTE                       1
#define BMA4_MAG_Y_LSB_BYTE                       2
#define BMA4_MAG_Y_MSB_BYTE                       3
#define BMA4_MAG_Z_LSB_BYTE                       4
#define BMA4_MAG_Z_MSB_BYTE                       5
#define BMA4_MAG_R_LSB_BYTE                       6
#define BMA4_MAG_R_MSB_BYTE                       7
#define BMA4_TEMP_BYTE                            0
#define BMA4_FIFO_LENGTH_MSB_BYTE                 1


/**\name    UTILITY MACROS  */
#define BMA4_SET_LOW_BYTE                         0x00FF
#define BMA4_SET_HIGH_BYTE                        0xFF00
#define BMA4_SET_LOW_NIBBLE                       0x0F

/* Macros used for Self test (BMA42X_VARIANT */
/* Self-test: Resulting minimum difference signal in mg for BMA42X */
#define BMA42X_ST_ACC_X_AXIS_SIGNAL_DIFF          400
#define BMA42X_ST_ACC_Y_AXIS_SIGNAL_DIFF          800
#define BMA42X_ST_ACC_Z_AXIS_SIGNAL_DIFF          400

/* Macros used for Self test (BMA42X_B_VARIANT */
/* Self-test: Resulting minimum difference signal in mg for BMA42X_B */
#define BMA42X_B_ST_ACC_X_AXIS_SIGNAL_DIFF        1800
#define BMA42X_B_ST_ACC_Y_AXIS_SIGNAL_DIFF        1800
#define BMA42X_B_ST_ACC_Z_AXIS_SIGNAL_DIFF        1800

/**\name    ERROR STATUS POSITION AND MASK*/
#define BMA4_FATAL_ERR_MSK                        0x01
#define BMA4_CMD_ERR_POS                          1
#define BMA4_CMD_ERR_MSK                          0x02
#define BMA4_ERR_CODE_POS                         2
#define BMA4_ERR_CODE_MSK                         0x1C
#define BMA4_FIFO_ERR_POS                         6
#define BMA4_FIFO_ERR_MSK                         0x40
#define BMA4_AUX_ERR_POS                          7
#define BMA4_AUX_ERR_MSK                          0x80

/**\name    NV_CONFIG POSITION AND MASK*/
/* NV_CONF Description - Reg Addr --> (0x70), Bit --> 3 */
#define BMA4_NV_ACCEL_OFFSET_POS                  3
#define BMA4_NV_ACCEL_OFFSET_MSK                  0x08

/**\name ACCEL DATA READY POSITION AND MASK*/
#define BMA4_STAT_DATA_RDY_ACCEL_POS              7
#define BMA4_STAT_DATA_RDY_ACCEL_MSK              0x80

/**\name ADVANCE POWER SAVE POSITION AND MASK*/
#define BMA4_ADVANCE_POWER_SAVE_MSK               0x01

/**\name ACCELEROMETER ENABLE POSITION AND MASK*/
#define BMA4_ACCEL_ENABLE_POS                     2
#define BMA4_ACCEL_ENABLE_MSK                     0x04

/**\name    ACCEL CONFIGURATION POSITION AND MASK*/
#define BMA4_ACCEL_ODR_MSK                        0x0F
#define BMA4_ACCEL_BW_POS                         4
#define BMA4_ACCEL_BW_MSK                         0x70
#define BMA4_ACCEL_RANGE_MSK                      0x03
#define BMA4_ACCEL_PERFMODE_POS                   7
#define BMA4_ACCEL_PERFMODE_MSK                   0x80


/**\name    MAG I2C ADDRESS SELECTION POSITION AND MASK*/
#define BMA4_I2C_DEVICE_ADDR_POS                  1
#define BMA4_I2C_DEVICE_ADDR_MSK                  0xFE


/**\name    ACCEL ODR          */
#define BMA4_OUTPUT_DATA_RATE_0_78HZ              0x01
#define BMA4_OUTPUT_DATA_RATE_1_56HZ              0x02
#define BMA4_OUTPUT_DATA_RATE_3_12HZ              0x03
#define BMA4_OUTPUT_DATA_RATE_6_25HZ              0x04
#define BMA4_OUTPUT_DATA_RATE_12_5HZ              0x05
#define BMA4_OUTPUT_DATA_RATE_25HZ                0x06
#define BMA4_OUTPUT_DATA_RATE_50HZ                0x07
#define BMA4_OUTPUT_DATA_RATE_100HZ               0x08
#define BMA4_OUTPUT_DATA_RATE_200HZ               0x09
#define BMA4_OUTPUT_DATA_RATE_400HZ               0x0A
#define BMA4_OUTPUT_DATA_RATE_800HZ               0x0B
#define BMA4_OUTPUT_DATA_RATE_1600HZ              0x0C

/**\name    ACCEL BANDWIDTH PARAMETER         */
#define BMA4_ACCEL_OSR4_AVG1                      0
#define BMA4_ACCEL_OSR2_AVG2                      1
#define BMA4_ACCEL_NORMAL_AVG4                    2
#define BMA4_ACCEL_CIC_AVG8                       3
#define BMA4_ACCEL_RES_AVG16                      4
#define BMA4_ACCEL_RES_AVG32                      5
#define BMA4_ACCEL_RES_AVG64                      6
#define BMA4_ACCEL_RES_AVG128                     7

/**\name    ACCEL PERFMODE PARAMETER         */
#define BMA4_CIC_AVG_MODE                         0
#define BMA4_CONTINUOUS_MODE                      1

/**\name    ENABLE/DISABLE SELECTIONS        */
#define BMA4_X_AXIS                               0
#define BMA4_Y_AXIS                               1
#define BMA4_Z_AXIS                               2


/**\name    ACCEL POWER MODE    */
#define BMA4_ACCEL_MODE_NORMAL                    0x11

/**\name    ENABLE/DISABLE BIT VALUES    */
#define BMA4_ENABLE                               0x01
#define BMA4_DISABLE                              0x00

/**\name    SENSOR RESOLUTION   */
#define BMA4_12_BIT_RESOLUTION                    12
#define BMA4_14_BIT_RESOLUTION                    14
#define BMA4_16_BIT_RESOLUTION                    16

/* Min and Max sampling frequency in mHz */
#define BMA422_ACCEL_MIN_FREQ	12500
#define BMA422_ACCEL_MAX_FREQ	MOTION_MAX_SENSOR_FREQUENCY(1600000, 6250)

extern const struct accelgyro_drv bma422_accel_drv;

struct bma422_accel_drv_data {
	struct accelgyro_saved_data_t saved_data;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

#endif /* __CROS_EC_ACCEL_BMA422_H */
