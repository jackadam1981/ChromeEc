/**
 * Copyright (c) 2020 Bosch Sensortec GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file       bma4_defs.h
 * @date       2020-03-01
 * @version    V2.19.0
 *
 */

/*! \file bma4_defs.h
 * \brief Sensor Driver for BMA4 family of sensors
 */
#ifndef BMA4_DEFS_H__
#define BMA4_DEFS_H__

/*********************************************************************/
/**\ header files */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#endif

/*********************************************************************/
/* macro definitions */

#ifdef __KERNEL__

#if (!defined(UINT8_C) && !defined(INT8_C))
#define INT8_C(x)                                 S8_C(x)
#define UINT8_C(x)                                U8_C(x)
#endif

#if (!defined(UINT16_C) && !defined(INT16_C))
#define INT16_C(x)                                S16_C(x)
#define UINT16_C(x)                               U16_C(x)
#endif

#if (!defined(INT32_C) && !defined(UINT32_C))
#define INT32_C(x)                                S32_C(x)
#define UINT32_C(x)                               U32_C(x)
#endif

#if (!defined(INT64_C) && !defined(UINT64_C))
#define INT64_C(x)                                S64_C(x)
#define UINT64_C(x)                               U64_C(x)
#endif

#else /* __KERNEL__ */

#if (!defined(UINT8_C) && !defined(INT8_C))
#define INT8_C(x)                                 (x)
#define UINT8_C(x)                                (x##U)
#endif

#if (!defined(UINT16_C) && !defined(INT16_C))
#define INT16_C(x)                                (x)
#define UINT16_C(x)                               (x##U)
#endif

#if (!defined(INT32_C) && !defined(UINT32_C))
#define INT32_C(x)                                (x)
#define UINT32_C(x)                               (x##U)
#endif

#if (!defined(INT64_C) && !defined(UINT64_C))
#define INT64_C(x)                                (x##LL)
#define UINT64_C(x)                               (x##ULL)
#endif

#endif /* __KERNEL__ */

/**\name CHIP ID ADDRESS*/
#define BMA4_CHIP_ID_ADDR                         UINT8_C(0x00)

/**\name ERROR STATUS*/
#define BMA4_ERROR_ADDR                           UINT8_C(0x02)

/**\name STATUS REGISTER FOR SENSOR STATUS FLAG*/
#define BMA4_STATUS_ADDR                          UINT8_C(0x03)

/**\name AUX/ACCEL DATA BASE ADDRESS REGISTERS*/
#define BMA4_DATA_0_ADDR                          UINT8_C(0x0A)
#define BMA4_DATA_8_ADDR                          UINT8_C(0x12)

/**\name SENSOR TIME REGISTERS*/
#define BMA4_SENSORTIME_0_ADDR                    UINT8_C(0x18)

/**\name INTERRUPT/FEATURE STATUS REGISTERS*/
#define BMA4_INT_STAT_0_ADDR                      UINT8_C(0x1C)

/**\name INTERRUPT/FEATURE STATUS REGISTERS*/
#define BMA4_INT_STAT_1_ADDR                      UINT8_C(0x1D)

/**\name TEMPERATURE REGISTERS*/
#define BMA4_TEMPERATURE_ADDR                     UINT8_C(0x22)

/**\name FIFO REGISTERS*/
#define BMA4_FIFO_LENGTH_0_ADDR                   UINT8_C(0x24)
#define BMA4_FIFO_DATA_ADDR                       UINT8_C(0x26)

/**\name ACCEL CONFIG REGISTERS*/
#define BMA4_ACCEL_CONFIG_ADDR                    UINT8_C(0x40)

/**\name ACCEL RANGE ADDRESS*/
#define BMA4_ACCEL_RANGE_ADDR                     UINT8_C(0x41)

/**\name FEATURE CONFIG RELATED */
#define BMA4_RESERVED_REG_5B_ADDR                 UINT8_C(0x5B)
#define BMA4_RESERVED_REG_5C_ADDR                 UINT8_C(0x5C)
#define BMA4_FEATURE_CONFIG_ADDR                  UINT8_C(0x5E)
#define BMA4_INTERNAL_ERROR                       UINT8_C(0x5F)

/**\name SERIAL INTERFACE SETTINGS REGISTER*/
#define BMA4_IF_CONFIG_ADDR                       UINT8_C(0x6B)

/**\name Macro to define accelerometer configuration value for FOC */
#define BMA4_FOC_ACC_CONF_VAL                     UINT8_C(0xB7)

/**\name SPI,I2C SELECTION REGISTER*/
#define BMA4_NV_CONFIG_ADDR                       UINT8_C(0x70)

/**\name ACCEL OFFSET REGISTERS*/
#define BMA4_OFFSET_0_ADDR                        UINT8_C(0x71)
#define BMA4_OFFSET_1_ADDR                        UINT8_C(0x72)
#define BMA4_OFFSET_2_ADDR                        UINT8_C(0x73)

/**\name POWER_CTRL REGISTER*/
#define BMA4_POWER_CONF_ADDR                      UINT8_C(0x7C)
#define BMA4_POWER_CTRL_ADDR                      UINT8_C(0x7D)

/**\name COMMAND REGISTER*/
#define BMA4_CMD_ADDR                             UINT8_C(0x7E)

/**\name GPIO REGISTERS*/
#define BMA4_STEP_CNT_OUT_0_ADDR                  UINT8_C(0x1E)
#define BMA4_HIGH_G_OUT_ADDR                      UINT8_C(0x1F)
#define BMA4_ACTIVITY_OUT_ADDR                    UINT8_C(0x27)
#define BMA4_ORIENTATION_OUT_ADDR                 UINT8_C(0x28)
#define BMA4_INTERNAL_STAT                        UINT8_C(0x2A)

/* Maximum length to read */
#define BMA4_MAX_LEN                              UINT8_C(128)

/*!
 * @brief Block size for config write
 */
#define BMA4_BLOCK_SIZE                           UINT8_C(32)

/**\name I2C slave address */
#define BMA4_I2C_ADDR_PRIMARY                     UINT8_C(0x18)
#define BMA4_I2C_ADDR_SECONDARY                   UINT8_C(0x19)
#define BMA4_I2C_BMM150_ADDR                      UINT8_C(0x10)

/**\name Interface selection macro */
#define BMA4_SPI_WR_MASK                          UINT8_C(0x7F)
#define BMA4_SPI_RD_MASK                          UINT8_C(0x80)

/**\name Chip ID macros */
#define BMA4_CHIP_ID_MIN                          UINT8_C(0x10)
#define BMA4_CHIP_ID_MAX                          UINT8_C(0x15)

/**\name Auxiliary sensor selection macro */
#define BMM150_SENSOR                             UINT8_C(1)
#define AKM9916_SENSOR                            UINT8_C(2)
#define BMA4_ASIC_INITIALIZED                     UINT8_C(0x01)

/**\name    Soft reset command */
#define BMA4_SOFT_RESET                           UINT8_C(0xB6)

/**\name    CONSTANTS */
#define BMA4_FIFO_CONFIG_LENGTH                   UINT8_C(2)
#define BMA4_ACCEL_CONFIG_LENGTH                  UINT8_C(2)
#define BMA4_FIFO_WM_LENGTH                       UINT8_C(2)
#define BMA4_NON_LATCH_MODE                       UINT8_C(0)
#define BMA4_LATCH_MODE                           UINT8_C(1)
#define BMA4_OPEN_DRAIN                           UINT8_C(1)
#define BMA4_PUSH_PULL                            UINT8_C(0)
#define BMA4_ACTIVE_HIGH                          UINT8_C(1)
#define BMA4_ACTIVE_LOW                           UINT8_C(0)
#define BMA4_EDGE_TRIGGER                         UINT8_C(1)
#define BMA4_LEVEL_TRIGGER                        UINT8_C(0)
#define BMA4_OUTPUT_ENABLE                        UINT8_C(1)
#define BMA4_OUTPUT_DISABLE                       UINT8_C(0)
#define BMA4_INPUT_ENABLE                         UINT8_C(1)
#define BMA4_INPUT_DISABLE                        UINT8_C(0)

/**\name ACCEL RANGE CHECK*/
#define BMA4_ACCEL_RANGE_2G                       UINT8_C(0)
#define BMA4_ACCEL_RANGE_4G                       UINT8_C(1)
#define BMA4_ACCEL_RANGE_8G                       UINT8_C(2)
#define BMA4_ACCEL_RANGE_16G                      UINT8_C(3)


/**\name BUS READ AND WRITE LENGTH FOR MAG & ACCEL*/
#define BMA4_ACCEL_DATA_LENGTH                    UINT8_C(6)
#define BMA4_FIFO_DATA_LENGTH                     UINT8_C(2)
#define BMA4_TEMP_DATA_SIZE                       UINT8_C(1)

/**\name TEMPERATURE CONSTANT */
#define BMA4_OFFSET_TEMP                          UINT8_C(23)
#define BMA4_DEG                                  UINT8_C(1)
#define BMA4_FAHREN                               UINT8_C(2)
#define BMA4_KELVIN                               UINT8_C(3)

/**\name DELAY DEFINITION IN MSEC*/
#define BMA4_AUX_IF_DELAY                         UINT8_C(5)
#define BMA4_BMM150_WAKEUP_DELAY1                 UINT8_C(2)
#define BMA4_BMM150_WAKEUP_DELAY2                 UINT8_C(3)
#define BMA4_BMM150_WAKEUP_DELAY3                 UINT8_C(1)
#define BMA4_GEN_READ_WRITE_DELAY                 UINT16_C(1000)
#define BMA4_AUX_COM_DELAY                        UINT16_C(10000)

/**\name    ARRAY PARAMETER DEFINITIONS*/
#define BMA4_SENSOR_TIME_MSB_BYTE                 UINT8_C(2)
#define BMA4_SENSOR_TIME_XLSB_BYTE                UINT8_C(1)
#define BMA4_SENSOR_TIME_LSB_BYTE                 UINT8_C(0)
#define BMA4_MAG_X_LSB_BYTE                       UINT8_C(0)
#define BMA4_MAG_X_MSB_BYTE                       UINT8_C(1)
#define BMA4_MAG_Y_LSB_BYTE                       UINT8_C(2)
#define BMA4_MAG_Y_MSB_BYTE                       UINT8_C(3)
#define BMA4_MAG_Z_LSB_BYTE                       UINT8_C(4)
#define BMA4_MAG_Z_MSB_BYTE                       UINT8_C(5)
#define BMA4_MAG_R_LSB_BYTE                       UINT8_C(6)
#define BMA4_MAG_R_MSB_BYTE                       UINT8_C(7)
#define BMA4_TEMP_BYTE                            UINT8_C(0)
#define BMA4_FIFO_LENGTH_MSB_BYTE                 UINT8_C(1)

/*! @name To define success code */
#define BMA4_OK                                   INT8_C(0)

/*! @name To define error codes */
#define BMA4_E_NULL_PTR                           INT8_C(-1)
#define BMA4_E_COM_FAIL                           INT8_C(-2)
#define BMA4_E_DEV_NOT_FOUND                      INT8_C(-3)
#define BMA4_E_INVALID_SENSOR                     INT8_C(-4)
#define BMA4_E_CONFIG_STREAM_ERROR                INT8_C(-5)
#define BMA4_E_SELF_TEST_FAIL                     INT8_C(-6)
#define BMA4_E_INVALID_STATUS                     INT8_C(-7)
#define BMA4_E_OUT_OF_RANGE                       INT8_C(-8)
#define BMA4_E_INT_LINE_INVALID                   INT8_C(-9)
#define BMA4_E_RD_WR_LENGTH_INVALID               INT8_C(-10)
#define BMA4_E_AUX_CONFIG_FAIL                    INT8_C(-11)
#define BMA4_E_SC_FIFO_HEADER_ERR                 INT8_C(-12)
#define BMA4_E_SC_FIFO_CONFIG_ERR                 INT8_C(-13)
#define BMA4_E_REMAP_ERROR                        INT8_C(-14)
#define BMA4_E_AVG_MODE_INVALID_CONF              INT8_C(-15)

/**\name    UTILITY MACROS  */
#define BMA4_SET_LOW_BYTE                         UINT16_C(0x00FF)
#define BMA4_SET_HIGH_BYTE                        UINT16_C(0xFF00)
#define BMA4_SET_LOW_NIBBLE                       UINT8_C(0x0F)

/* Macros used for Self test (BMA42X_VARIANT) */
/* Self-test: Resulting minimum difference signal in mg for BMA42X */
#define BMA42X_ST_ACC_X_AXIS_SIGNAL_DIFF          UINT16_C(400)
#define BMA42X_ST_ACC_Y_AXIS_SIGNAL_DIFF          UINT16_C(800)
#define BMA42X_ST_ACC_Z_AXIS_SIGNAL_DIFF          UINT16_C(400)

/* Macros used for Self test (BMA42X_B_VARIANT) */
/* Self-test: Resulting minimum difference signal in mg for BMA42X_B */
#define BMA42X_B_ST_ACC_X_AXIS_SIGNAL_DIFF        UINT16_C(1800)
#define BMA42X_B_ST_ACC_Y_AXIS_SIGNAL_DIFF        UINT16_C(1800)
#define BMA42X_B_ST_ACC_Z_AXIS_SIGNAL_DIFF        UINT16_C(1800)


/**\name BOOLEAN TYPES*/
#ifndef TRUE
#define TRUE                                      UINT8_C(0x01)
#endif

#ifndef FALSE
#define FALSE                                     UINT8_C(0x00)
#endif

#ifndef NULL
#define NULL                                      UINT8_C(0x00)
#endif

/**\name    ERROR STATUS POSITION AND MASK*/
#define BMA4_FATAL_ERR_MSK                        UINT8_C(0x01)
#define BMA4_CMD_ERR_POS                          UINT8_C(1)
#define BMA4_CMD_ERR_MSK                          UINT8_C(0x02)
#define BMA4_ERR_CODE_POS                         UINT8_C(2)
#define BMA4_ERR_CODE_MSK                         UINT8_C(0x1C)
#define BMA4_FIFO_ERR_POS                         UINT8_C(6)
#define BMA4_FIFO_ERR_MSK                         UINT8_C(0x40)
#define BMA4_AUX_ERR_POS                          UINT8_C(7)
#define BMA4_AUX_ERR_MSK                          UINT8_C(0x80)

/**\name    NV_CONFIG POSITION AND MASK*/
/* NV_CONF Description - Reg Addr --> (0x70), Bit --> 3 */
#define BMA4_NV_ACCEL_OFFSET_POS                  UINT8_C(3)
#define BMA4_NV_ACCEL_OFFSET_MSK                  UINT8_C(0x08)

/**\name ACCEL DATA READY POSITION AND MASK*/
#define BMA4_STAT_DATA_RDY_ACCEL_POS              UINT8_C(7)
#define BMA4_STAT_DATA_RDY_ACCEL_MSK              UINT8_C(0x80)

/**\name ADVANCE POWER SAVE POSITION AND MASK*/
#define BMA4_ADVANCE_POWER_SAVE_MSK               UINT8_C(0x01)

/**\name ACCELEROMETER ENABLE POSITION AND MASK*/
#define BMA4_ACCEL_ENABLE_POS                     UINT8_C(2)
#define BMA4_ACCEL_ENABLE_MSK                     UINT8_C(0x04)

/**\name    ACCEL CONFIGURATION POSITION AND MASK*/
#define BMA4_ACCEL_ODR_MSK                        UINT8_C(0x0F)
#define BMA4_ACCEL_BW_POS                         UINT8_C(4)
#define BMA4_ACCEL_BW_MSK                         UINT8_C(0x70)
#define BMA4_ACCEL_RANGE_MSK                      UINT8_C(0x03)
#define BMA4_ACCEL_PERFMODE_POS                   UINT8_C(7)
#define BMA4_ACCEL_PERFMODE_MSK                   UINT8_C(0x80)


/**\name    MAG I2C ADDRESS SELECTION POSITION AND MASK*/
#define BMA4_I2C_DEVICE_ADDR_POS                  UINT8_C(1)
#define BMA4_I2C_DEVICE_ADDR_MSK                  UINT8_C(0xFE)


/**\name    ACCEL ODR          */
#define BMA4_OUTPUT_DATA_RATE_0_78HZ              UINT8_C(0x01)
#define BMA4_OUTPUT_DATA_RATE_1_56HZ              UINT8_C(0x02)
#define BMA4_OUTPUT_DATA_RATE_3_12HZ              UINT8_C(0x03)
#define BMA4_OUTPUT_DATA_RATE_6_25HZ              UINT8_C(0x04)
#define BMA4_OUTPUT_DATA_RATE_12_5HZ              UINT8_C(0x05)
#define BMA4_OUTPUT_DATA_RATE_25HZ                UINT8_C(0x06)
#define BMA4_OUTPUT_DATA_RATE_50HZ                UINT8_C(0x07)
#define BMA4_OUTPUT_DATA_RATE_100HZ               UINT8_C(0x08)
#define BMA4_OUTPUT_DATA_RATE_200HZ               UINT8_C(0x09)
#define BMA4_OUTPUT_DATA_RATE_400HZ               UINT8_C(0x0A)
#define BMA4_OUTPUT_DATA_RATE_800HZ               UINT8_C(0x0B)
#define BMA4_OUTPUT_DATA_RATE_1600HZ              UINT8_C(0x0C)

/**\name    ACCEL BANDWIDTH PARAMETER         */
#define BMA4_ACCEL_OSR4_AVG1                      UINT8_C(0)
#define BMA4_ACCEL_OSR2_AVG2                      UINT8_C(1)
#define BMA4_ACCEL_NORMAL_AVG4                    UINT8_C(2)
#define BMA4_ACCEL_CIC_AVG8                       UINT8_C(3)
#define BMA4_ACCEL_RES_AVG16                      UINT8_C(4)
#define BMA4_ACCEL_RES_AVG32                      UINT8_C(5)
#define BMA4_ACCEL_RES_AVG64                      UINT8_C(6)
#define BMA4_ACCEL_RES_AVG128                     UINT8_C(7)

/**\name    ACCEL PERFMODE PARAMETER         */
#define BMA4_CIC_AVG_MODE                         UINT8_C(0)
#define BMA4_CONTINUOUS_MODE                      UINT8_C(1)

/**\name    ENABLE/DISABLE SELECTIONS        */
#define BMA4_X_AXIS                               UINT8_C(0)
#define BMA4_Y_AXIS                               UINT8_C(1)
#define BMA4_Z_AXIS                               UINT8_C(2)


/**\name    ACCEL POWER MODE    */
#define BMA4_ACCEL_MODE_NORMAL                    UINT8_C(0x11)

/**\name    ENABLE/DISABLE BIT VALUES    */
#define BMA4_ENABLE                               UINT8_C(0x01)
#define BMA4_DISABLE                              UINT8_C(0x00)

/**\name    SENSOR RESOLUTION   */
#define BMA4_12_BIT_RESOLUTION                    UINT8_C(12)
#define BMA4_14_BIT_RESOLUTION                    UINT8_C(14)
#define BMA4_16_BIT_RESOLUTION                    UINT8_C(16)

/*************** FOC Macros ******************/

/* Reference value with positive and negative noise range in lsb */

/* Resolution : 16 bit */

/*
 * As per datasheet, Zero-g offset : +/- 20mg
 *
 * In range 2G,  1G is 16384. so, 16384 x 20 x (10 ^ -3) = 328
 * In range 4G,  1G is 8192.  so,  8192 x 20 x (10 ^ -3) = 164
 * In range 8G,  1G is 4096.  so,  4096 x 20 x (10 ^ -3) = 82
 * In range 16G, 1G is 2048.  so,  2048 x 20 x (10 ^ -3) = 41
 */
#define BMA4_16BIT_ACC_FOC_2G_REF               UINT16_C(16384)
#define BMA4_16BIT_ACC_FOC_4G_REF               UINT16_C(8192)
#define BMA4_16BIT_ACC_FOC_8G_REF               UINT16_C(4096)
#define BMA4_16BIT_ACC_FOC_16G_REF              UINT16_C(2048)

#define BMA4_16BIT_ACC_FOC_2G_OFFSET            UINT16_C(328)
#define BMA4_16BIT_ACC_FOC_4G_OFFSET            UINT16_C(164)
#define BMA4_16BIT_ACC_FOC_8G_OFFSET            UINT16_C(82)
#define BMA4_16BIT_ACC_FOC_16G_OFFSET           UINT16_C(41)

#define BMA4_16BIT_ACC_2G_MAX_NOISE_LIMIT       (BMA4_16BIT_ACC_FOC_2G_REF + BMA4_16BIT_ACC_FOC_2G_OFFSET)
#define BMA4_16BIT_ACC_2G_MIN_NOISE_LIMIT       (BMA4_16BIT_ACC_FOC_2G_REF - BMA4_16BIT_ACC_FOC_2G_OFFSET)
#define BMA4_16BIT_ACC_4G_MAX_NOISE_LIMIT       (BMA4_16BIT_ACC_FOC_4G_REF + BMA4_16BIT_ACC_FOC_4G_OFFSET)
#define BMA4_16BIT_ACC_4G_MIN_NOISE_LIMIT       (BMA4_16BIT_ACC_FOC_4G_REF - BMA4_16BIT_ACC_FOC_4G_OFFSET)
#define BMA4_16BIT_ACC_8G_MAX_NOISE_LIMIT       (BMA4_16BIT_ACC_FOC_8G_REF + BMA4_16BIT_ACC_FOC_8G_OFFSET)
#define BMA4_16BIT_ACC_8G_MIN_NOISE_LIMIT       (BMA4_16BIT_ACC_FOC_8G_REF - BMA4_16BIT_ACC_FOC_8G_OFFSET)
#define BMA4_16BIT_ACC_16G_MAX_NOISE_LIMIT      (BMA4_16BIT_ACC_FOC_16G_REF + BMA4_16BIT_ACC_FOC_16G_OFFSET)
#define BMA4_16BIT_ACC_16G_MIN_NOISE_LIMIT      (BMA4_16BIT_ACC_FOC_16G_REF - BMA4_16BIT_ACC_FOC_16G_OFFSET)

/* Resolution : 12 bit */

/*
 * As per datasheet, Zero-g offset : +/- 80mg
 *
 * In range 2G,  1G is 1024. so, 1024 x 80 x (10 ^ -3) = 82
 * In range 4G,  1G is  512. so,  512 x 80 x (10 ^ -3) = 41
 * In range 8G,  1G is  256. so,  256 x 80 x (10 ^ -3) = 20
 * In range 16G, 1G is  128. so,  128 x 80 x (10 ^ -3) = 10
 */
#define BMA4_12BIT_ACC_FOC_2G_REF               UINT16_C(1024)
#define BMA4_12BIT_ACC_FOC_4G_REF               UINT16_C(512)
#define BMA4_12BIT_ACC_FOC_8G_REF               UINT16_C(256)
#define BMA4_12BIT_ACC_FOC_16G_REF              UINT16_C(128)

#define BMA4_12BIT_ACC_FOC_2G_OFFSET            UINT8_C(82)
#define BMA4_12BIT_ACC_FOC_4G_OFFSET            UINT8_C(41)
#define BMA4_12BIT_ACC_FOC_8G_OFFSET            UINT8_C(20)
#define BMA4_12BIT_ACC_FOC_16G_OFFSET           UINT8_C(10)

#define BMA4_12BIT_ACC_2G_MAX_NOISE_LIMIT       (BMA4_12BIT_ACC_FOC_2G_REF + BMA4_12BIT_ACC_FOC_2G_OFFSET)
#define BMA4_12BIT_ACC_2G_MIN_NOISE_LIMIT       (BMA4_12BIT_ACC_FOC_2G_REF - BMA4_12BIT_ACC_FOC_2G_OFFSET)
#define BMA4_12BIT_ACC_4G_MAX_NOISE_LIMIT       (BMA4_12BIT_ACC_FOC_4G_REF + BMA4_12BIT_ACC_FOC_4G_OFFSET)
#define BMA4_12BIT_ACC_4G_MIN_NOISE_LIMIT       (BMA4_12BIT_ACC_FOC_4G_REF - BMA4_12BIT_ACC_FOC_4G_OFFSET)
#define BMA4_12BIT_ACC_8G_MAX_NOISE_LIMIT       (BMA4_12BIT_ACC_FOC_8G_REF + BMA4_12BIT_ACC_FOC_8G_OFFSET)
#define BMA4_12BIT_ACC_8G_MIN_NOISE_LIMIT       (BMA4_12BIT_ACC_FOC_8G_REF - BMA4_12BIT_ACC_FOC_8G_OFFSET)
#define BMA4_12BIT_ACC_16G_MAX_NOISE_LIMIT      (BMA4_12BIT_ACC_FOC_16G_REF + BMA4_12BIT_ACC_FOC_16G_OFFSET)
#define BMA4_12BIT_ACC_16G_MIN_NOISE_LIMIT      (BMA4_12BIT_ACC_FOC_16G_REF - BMA4_12BIT_ACC_FOC_16G_OFFSET)

/*! for handling float temperature values */
#define BMA4_SCALE_TEMP                         INT32_C(1000)

/* BMA4_FAHREN_SCALED = 1.8 * 1000 */
#define BMA4_FAHREN_SCALED                      INT32_C(1800)

/* BMA4_KELVIN_SCALED = 273.15 * 1000 */
#define BMA4_KELVIN_SCALED                      INT32_C(273150)


#define BMA4_MS_TO_US(X)                        (X * 1000)

#ifndef ABS
#define ABS(a)                                  ((a) > 0 ? (a) : -(a))   /*!< Absolute value */
#endif

/**\name    BIT SLICE GET AND SET FUNCTIONS */
#define BMA4_GET_BITSLICE(regvar, bitname) \
    ((regvar & bitname##_MSK) >> bitname##_POS)

#define BMA4_SET_BITSLICE(regvar, bitname, val) \
    ((regvar & ~bitname##_MSK) | \
     ((val << bitname##_POS) & bitname##_MSK))

#define BMA4_GET_DIFF(x, y)                     ((x) - (y))

#define BMA4_GET_LSB(var)                       (uint8_t)(var & BMA4_SET_LOW_BYTE)
#define BMA4_GET_MSB(var)                       (uint8_t)((var & BMA4_SET_HIGH_BYTE) >> 8)

#define BMA4_SET_BIT_VAL_0(reg_data, bitname)   (reg_data & ~(bitname##_MSK))

#define BMA4_SET_BITS_POS_0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     (data & bitname##_MSK))

#define BMA4_GET_BITS_POS_0(reg_data, bitname)  (reg_data & (bitname##_MSK))

/**
 * BMA4_INTF_RET_TYPE is the read/write interface return type which can be overwritten by the build system.
 * The default is set to int8_t.
 */
#ifndef BMA4_INTF_RET_TYPE
#define BMA4_INTF_RET_TYPE                      int8_t
#endif

/**
 * BST_INTF_RET_SUCCESS is the success return value read/write interface return type which can be
 * overwritten by the build system. The default is set to 0. It is used to check for a successful
 * execution of the read/write functions
 */
#ifndef BMA4_INTF_RET_SUCCESS
#define BMA4_INTF_RET_SUCCESS                   INT8_C(0)
#endif

/******************************************************************************/
/*!  @name         TYPEDEF DEFINITIONS                                        */
/******************************************************************************/

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific read functions of the user
 *
 * @param[in] reg_addr       : Register address from which data is read.
 * @param[out] read_data     : Pointer to data buffer where read data is stored.
 * @param[in] len            : Number of bytes of data to be read.
 * @param[in, out] intf_ptr  : Void pointer that can enable the linking of descriptors
 *                             for interface related call backs.
 *
 * @retval 0 for Success
 * @retval Non-zero for Failure
 */
typedef BMA4_INTF_RET_TYPE (*bma4_read_fptr_t)(uint8_t reg_addr, uint8_t *read_data, uint32_t len, void *intf_ptr);

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific write functions of the user
 *
 * @param[in] reg_addr      : Register address to which the data is written.
 * @param[in] read_data     : Pointer to data buffer in which data to be written
 *                            is stored.
 * @param[in] len           : Number of bytes of data to be written.
 * @param[in, out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                            for interface related call backs
 *
 * @retval 0 for Success
 * @retval Non-zero for Failure
 */
typedef BMA4_INTF_RET_TYPE (*bma4_write_fptr_t)(uint8_t reg_addr, const uint8_t *read_data, uint32_t len,
                                                void *intf_ptr);

/*!
 * @brief Delay function pointer which should be mapped to
 * delay function of the user
 *
 * @param[in] period              : Delay in microseconds.
 * @param[in, out] intf_ptr       : Void pointer that can enable the linking of descriptors
 *                                  for interface related call backs
 *
 */
typedef void (*bma4_delay_us_fptr_t)(uint32_t period, void *intf_ptr);

/******************************************************************************/
/*!  @name         Enum Declarations                                  */
/******************************************************************************/
/*!  @name Enum to define BMA4 variants */
enum  bma4_variant {
    BMA42X_VARIANT = 1,
    BMA42X_B_VARIANT,
    BMA45X_VARIANT
};

/* Enumerator describing interfaces */
enum bma4_intf {
    BMA4_SPI_INTF,
    BMA4_I2C_INTF
};

/**\name    STRUCTURE DEFINITIONS*/
/*!
 * @brief Axes re-mapping configuration
 */
struct bma4_axes_remap
{
    /*! Re-mapped x-axis */
    uint8_t x_axis;

    /*! Re-mapped y-axis */
    uint8_t y_axis;

    /*! Re-mapped z-axis */
    uint8_t z_axis;

    /*! Re-mapped x-axis sign */
    uint8_t x_axis_sign;

    /*! Re-mapped y-axis sign */
    uint8_t y_axis_sign;

    /*! Re-mapped z-axis sign */
    uint8_t z_axis_sign;
};

/*!
 *  @brief
 *  This structure holds all relevant information about BMA4
 */
struct bma4_dev
{
    /*! Chip id of BMA4 */
    uint8_t chip_id;

    /*! Chip id of auxiliary sensor */
    uint8_t aux_chip_id;

    /*! Interface pointer */
    void *intf_ptr;

    /*! Interface detail */
    enum bma4_intf intf;

    /*! Variable that holds error code */
    BMA4_INTF_RET_TYPE intf_rslt;

    /*! Auxiliary sensor information */
//    uint8_t aux_sensor;

    /*! Decide SPI or I2C read mechanism */
    uint8_t dummy_byte;

    /*! Resolution for FOC */
    uint8_t resolution;

    /*! Define the BMA4 variant */
    enum bma4_variant variant;

    /*! Used to check mag manual/auto mode status
     * int8_t mag_manual_enable;
     */

    /*! Config stream data buffer address will be assigned*/
//    const uint8_t *config_file_ptr;

    /*! Read/write length */
    uint16_t read_write_len;

    /*! Feature len */
//    uint8_t feature_len;

    /*! Contains asic information */
//    struct bma4_asic_data asic_data;

    /*! Contains aux configuration settings */
//    struct bma4_aux_config aux_config;

    /*! Structure to maintain a copy of the re-mapped axis */
//    struct bma4_axes_remap remap;

    /*! Bus read function pointer */
    bma4_read_fptr_t bus_read;

    /*! Bus write function pointer */
    bma4_write_fptr_t bus_write;

    /*! Delay(in microsecond) function pointer */
    bma4_delay_us_fptr_t delay_us;

    /*! Variable to store the size of config file */
//    uint16_t config_size;

    /*! Variable to store the status of performance mode */
    uint8_t perf_mode_status;
};

/*!
 *  @brief Error Status structure
 */
struct bma4_err_reg
{
    /*! Indicates fatal error */
    uint8_t fatal_err;

    /*! Indicates command error */
    uint8_t cmd_err;

    /*! Indicates error code */
    uint8_t err_code;

    /*! Indicates fifo error */
    uint8_t fifo_err;

    /*! Indicates mag error */
    uint8_t aux_err;
};

/*!
 * @brief Asic Status structure
 */
struct bma4_asic_status
{
    /*! Asic is in sleep/halt state */
    uint8_t sleep;

    /*! Dedicated interrupt is set again before previous interrupt
     * was acknowledged
     */
    uint8_t irq_ovrn;

    /*! Watchcell event detected (asic stopped) */
    uint8_t wc_event;

    /*! Stream transfer has started and transactions are ongoing */
    uint8_t stream_transfer_active;
};

/*!
 * @brief Accelerometer configuration structure
 */
struct bma4_accel_config
{
    /*! Output data rate in Hz */
    uint8_t odr;

    /*! Bandwidth parameter, determines filter configuration */
    uint8_t bandwidth;

    /*! Filter performance mode */
    uint8_t perf_mode;

    /*! G-range */
    uint8_t range;
};

/*!
 * @brief Accel xyz data structure
 */
struct bma4_accel
{
    /*! Accel X data */
    int16_t x;

    /*! Accel Y data */
    int16_t y;

    /*! Accel Z data */
    int16_t z;
};

/*!  @name Structure to enable an accel axis for FOC */
struct bma4_accel_foc_g_value
{
    /* '0' to disable x-axis and '1' to enable x-axis */
    uint8_t x;

    /* '0' to disable y-axis and '1' to enable y-axis */
    uint8_t y;

    /* '0' to disable z-axis and '1' to enable z-axis */
    uint8_t z;

    /* '0' for positive input and '1' for negative input */
    uint8_t sign;
};

/*! @name Structure to store temporary accelerometer values */
struct bma4_foc_temp_value
{
    /*! X data */
    int32_t x;

    /*! Y data */
    int32_t y;

    /*! Z data */
    int32_t z;
};

#endif /* End of BMA4_DEFS_H__ */
