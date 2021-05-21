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
 * @file       bmi3x0_defs.h
 * @date       2021-01-25
 * @version    v1.1.7
 *
 */
 #ifndef _BMI3X0_DEFS_H
 #define _BMI3X0_DEFS_H

/********************************************************* */
/*!             Header includes                           */
/********************************************************* */
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/kernel.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif

/********************************************************* */
/*!               Common Macros                           */
/********************************************************* */
#ifdef __KERNEL__
#if !defined(UINT8_C) && !defined(INT8_C)
#define INT8_C(x)    S8_C(x)
#define UINT8_C(x)   U8_C(x)
#endif

#if !defined(UINT16_C) && !defined(INT16_C)
#define INT16_C(x)   S16_C(x)
#define UINT16_C(x)  U16_C(x)
#endif

#if !defined(INT32_C) && !defined(UINT32_C)
#define INT32_C(x)   S32_C(x)
#define UINT32_C(x)  U32_C(x)
#endif

#if !defined(INT64_C) && !defined(UINT64_C)
#define INT64_C(x)   S64_C(x)
#define UINT64_C(x)  U64_C(x)
#endif
#endif

/*! C standard macros */
#ifndef NULL
#ifdef __cplusplus
#define NULL         0
#else
#define NULL         ((void *) 0)
#endif
#endif

/********************************************************* */
/*!             General Macro Definitions                 */
/********************************************************* */
/*! Utility macros */
#define BMI3X0_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MASK)) | \
     ((data << bitname##_POS) & bitname##_MASK))

#define BMI3X0_GET_BITS(reg_data, bitname) \
    ((reg_data & (bitname##_MASK)) >> \
     (bitname##_POS))

#define BMI3X0_SET_BIT_POS0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MASK)) | \
     (data & bitname##_MASK))

#define BMI3X0_GET_BIT_POS0(reg_data, bitname)         (reg_data & (bitname##_MASK))
#define BMI3X0_SET_BIT_VAL0(reg_data, bitname)         (reg_data & ~(bitname##_MASK))

/*! LSB and MSB mask definitions */
#define BMI3X0_SET_LOW_BYTE                            UINT16_C(0x00FF)
#define BMI3X0_SET_HIGH_BYTE                           UINT16_C(0xFF00)
#define BMI3X0_SET_LOW_NIBBLE                          UINT8_C(0x0F)

/*! For getting LSB and MSB */
#define BMI3X0_GET_LSB(var)                            (uint8_t)(var & BMI3X0_SET_LOW_BYTE)
#define BMI3X0_GET_MSB(var)                            (uint8_t)((var & BMI3X0_SET_HIGH_BYTE) >> 8)

/*! For enable and disable */
#define BMI3X0_ENABLE                                  UINT8_C(1)
#define BMI3X0_DISABLE                                 UINT8_C(0)

/*! To define TRUE or FALSE */
#define BMI3X0_TRUE                                    UINT8_C(1)
#define BMI3X0_FALSE                                   UINT8_C(0)

/*!
 * BMI3X0_INTF_RET_TYPE is the read/write interface return type which can be overwritten by the build system.
 * The default is set to int8_t.
 */
#ifndef BMI3X0_INTF_RET_TYPE
#define BMI3X0_INTF_RET_TYPE                           int8_t
#endif

/*!
 * BMI3X0_INTF_RET_SUCCESS is the success return value read/write interface return type which can be
 * overwritten by the build system. The default is set to 0.
 */
#ifndef BMI3X0_INTF_RET_SUCCESS
#define BMI3X0_INTF_RET_SUCCESS                        INT8_C(0)
#endif

/*! To define the chip id of bmi3x0 */
#define BMI3X0_CHIP_ID_PRIM                            UINT16_C(0x0040)
#define BMI3X0_CHIP_ID_SEC                             UINT16_C(0x0041)

/*! To define success code */
#define BMI3X0_OK                                      INT8_C(0)

/*! To define error codes */
#define BMI3X0_E_NULL_PTR                              INT8_C(-1)
#define BMI3X0_E_COM_FAIL                              INT8_C(-2)
#define BMI3X0_E_DEV_NOT_FOUND                         INT8_C(-3)
#define BMI3X0_E_ACC_INVALID_CFG                       INT8_C(-4)
#define BMI3X0_E_GYRO_INVALID_CFG                      INT8_C(-5)
#define BMI3X0_E_INVALID_SENSOR                        INT8_C(-6)
#define BMI3X0_E_INVALID_INT_PIN                       INT8_C(-7)
#define BMI3X0_E_INVALID_INPUT                         INT8_C(-8)
#define BMI3X0_E_INVALID_STATUS                        INT8_C(-9)
#define BMI3X0_E_DATA_RDY_INT_FAILED                   INT8_C(-10)
#define BMI3X0_E_INVALID_FOC_POSITION                  INT8_C(-11)
#define BMI3X0_E_INVALID_CONTEXT_SEL                   INT8_C(-12)
#define BMI3X0_INVALID_ST_SELECTION                    INT8_C(-13)

/*! BMI3X0 Commands */
#define BMI3X0_CMD_SELF_TEST_TRIGGER                   UINT16_C(0x0100)
#define BMI3X0_CMD_SELF_CALIB_TRIGGER                  UINT16_C(0x0101)
#define BMI3X0_CMD_SELF_CALIB_ABORT                    UINT16_C(0x0200)
#define BMI3X0_CMD_I3C_TCSYNC_UPDATE                   UINT16_C(0x0201)
#define BMI3X0_CMD_AXIS_MAP_UPDATE                     UINT16_C(0x300)
#define BMI3X0_CMD_USR_GAIN_OFFS_UPDATE                UINT16_C(0x301)
#define BMI3X0_CMD_SUPMODE_EN_FIRST                    UINT16_C(0x5352)
#define BMI3X0_CMD_EXTMODE_EN_LAST                     UINT16_C(0x64AD)
#define BMI3X0_CMD_EXTMODE_EN_FIRST                    UINT16_C(0xD3AC)
#define BMI3X0_CMD_SUPMODE_EN_LAST                     UINT16_C(0xD7AF)
#define BMI3X0_CMD_SOFT_RESET                          UINT16_C(0xDEAF)

/*! To define warnings for FIFO activity */
#define BMI3X0_W_FIFO_EMPTY                            UINT8_C(1)
#define BMI3X0_W_PARTIAL_READ                          UINT8_C(2)
#define BMI3X0_W_ST_PARTIAL_READ                       UINT8_C(3)
#define BMI3X0_W_FIFO_GYRO_DUMMY_FRAME                 UINT8_C(4)
#define BMI3X0_W_FIFO_ACCEL_DUMMY_FRAME                UINT8_C(5)
#define BMI3X0_W_FIFO_TEMP_DUMMY_FRAME                 UINT8_C(6)
#define BMI3X0_W_FIFO_INVALID_FRAME                    UINT8_C(7)

/*! Masks for FIFO dummy data frames */
#define BMI3X0_FIFO_GYRO_DUMMY_FRAME                   UINT16_C(0x7f02)
#define BMI3X0_FIFO_ACCEL_DUMMY_FRAME                  UINT16_C(0x7f01)
#define BMI3X0_FIFO_TEMP_DUMMY_FRAME                   UINT16_C(0x8000)

/*! Bit wise to define information */
#define BMI3X0_I_MIN_VALUE                             UINT8_C(1)
#define BMI3X0_I_MAX_VALUE                             UINT8_C(2)

/*! To define Self-test enable modes */
#define BMI3X0_ST_ACCEL_ONLY                           UINT8_C(1)
#define BMI3X0_ST_GYRO_ONLY                            UINT8_C(2)
#define BMI3X0_ST_BOTH_ACC_GYR                         UINT8_C(3)

/***************************************************************************** */
/*!         Sensor Macro Definitions                 */
/***************************************************************************** */
/*! Macros to define BMI3X0 sensor/feature types */
#define BMI3X0_ACCEL                                   UINT8_C(0)
#define BMI3X0_GYRO                                    UINT8_C(1)
#define BMI3X0_SIG_MOTION                              UINT8_C(2)
#define BMI3X0_ANY_MOTION                              UINT8_C(3)
#define BMI3X0_NO_MOTION                               UINT8_C(4)
#define BMI3X0_STEP_DETECTOR                           UINT8_C(5)
#define BMI3X0_STEP_COUNTER                            UINT8_C(6)
#define BMI3X0_TILT                                    UINT8_C(7)
#define BMI3X0_ORIENTATION                             UINT8_C(8)
#define BMI3X0_FLAT                                    UINT8_C(9)
#define BMI3X0_WAKEUP                                  UINT8_C(10)
#define BMI3X0_I3C_SYC                                 UINT8_C(11)
#define BMI3X0_GYRO_GAIN_UPDATE                        UINT8_C(12)
#define BMI3X0_ALT_ACCEL                               UINT8_C(13)
#define BMI3X0_ALT_GYRO                                UINT8_C(14)
#define BMI3X0_ALT_AUTO_CONFIG                         UINT8_C(15)

/*! Non virtual sensor features */
#define BMI3X0_TEMP                                    UINT8_C(16)
#define BMI3X0_ACCEL_SELF_TEST                         UINT8_C(17)
#define BMI3X0_GYRO_SELF_OFF                           UINT8_C(18)
#define BMI3X0_MAX_BURST_LEN                           UINT8_C(19)
#define BMI3X0_AXIS_MAP                                UINT8_C(20)
#define BMI3X0_GYRO_CROSS_SENSE                        UINT8_C(21)
#define BMI3X0_CRT_GYRO_SELF_TEST                      UINT8_C(22)
#define BMI3X0_ABORT_CRT_GYRO_SELF_TEST                UINT8_C(23)
#define BMI3X0_CONFIG_ID                               UINT8_C(24)
#define BMI3X0_I3C_SYNC_ACCEL                          UINT8_C(25)
#define BMI3X0_I3C_SYNC_GYRO                           UINT8_C(26)
#define BMI3X0_I3C_SYNC_TEMP                           UINT8_C(27)

/*! Maximum number of features in bmi3x0 */
#define BMI3X0_MAX_FEATURE                             UINT8_C(6)

/*! Maximum limit for context parameter set */
#define BMI3X0_PARAM_LIMIT_CONTEXT                     UINT8_C(3)

/*! Parameter set limit for bmi3x0 features */
#define BMI3X0_PARAM_LIMIT_TILT                        UINT8_C(3)
#define BMI3X0_PARAM_LIMIT_ANY_MOT                     UINT8_C(5)
#define BMI3X0_PARAM_LIMIT_NO_MOT                      UINT8_C(5)
#define BMI3X0_PARAM_LIMIT_SIG_MOT                     UINT8_C(5)
#define BMI3X0_PARAM_LIMIT_FLAT                        UINT8_C(5)
#define BMI3X0_PARAM_LIMIT_ORIENT                      UINT8_C(7)
#define BMI3X0_PARAM_LIMIT_WAKEUP                      UINT8_C(10)
#define BMI3X0_PARAM_LIMIT_STEP_COUNT                  UINT8_C(22)

#define BMI3X0_16_BIT_RESOLUTION                       UINT8_C(16)

/*! Masks for mapping of config major and minor mask instances in the system */
#define BMI3X0_CONFIG_1_MAJOR_MASK                     UINT16_C(0xFC00)
#define BMI3X0_CONFIG_1_MINOR_MASK                     UINT16_C(0x03FF)
#define BMI3X0_CONFIG_2_MAJOR_MASK                     UINT16_C(0xFC00)
#define BMI3X0_CONFIG_2_MINOR_MASK                     UINT16_C(0x03FF)

/*! Bit position for config version */
#define BMI3X0_CONFIG_POS                              UINT16_C(10)

/********************************************************* */
/*!                 Register Addresses                    */
/********************************************************* */

/*! To define the chip id address */
#define BMI3X0_REG_CHIP_ID                             UINT8_C(0x00)

/*! ASIC identification code */
#define BMI3X0_REG_ASIC_ID                             UINT8_C(0x00)

/*! Reports sensor error conditions */
#define BMI3X0_REG_ERR_REG                             UINT8_C(0x01)

/*! Sensor status flags */
#define BMI3X0_REG_STATUS                              UINT8_C(0x02)

/*! ACC Data X. */
#define BMI3X0_REG_ACC_DATA_X                          UINT8_C(0x03)

/*! ACC Data Y. */
#define BMI3X0_REG_ACC_DATA_Y                          UINT8_C(0x04)

/*! ACC Data Z. */
#define BMI3X0_REG_ACC_DATA_Z                          UINT8_C(0x05)

/*! GYR Data X. */
#define BMI3X0_REG_GYR_DATA_X                          UINT8_C(0x06)

/*! GYR Data Y. */
#define BMI3X0_REG_GYR_DATA_Y                          UINT8_C(0x07)

/*! GYR Data Z. */
#define BMI3X0_REG_GYR_DATA_Z                          UINT8_C(0x08)

/*! Temperature Data.
 *  The resolution is 512 LSB/K.
 *  0x0000 -> 23 degree Celsius
 *  0x8000 -> invalid
 */
#define BMI3X0_REG_TEMP_DATA                           UINT8_C(0x09)

/*! Sensor time LSW (15:0). */
#define BMI3X0_REG_SENSOR_TIME_0                       UINT8_C(0x0A)

/*! Sensor time MSW (31:16). */
#define BMI3X0_REG_SENSOR_TIME_1                       UINT8_C(0x0B)

/*! Saturation flags for each sensor and axis. */
#define BMI3X0_REG_SAT_FLAGS                           UINT8_C(0x0C)

/*! INT1 Status Register.
 *  This register is clear-on-read.
 */
#define BMI3X0_REG_INT_STATUS_INT1                     UINT8_C(0x0D)

/*! INT2 Status Register.
 *  This register is clear-on-read.
 */
#define BMI3X0_REG_INT_STATUS_INT2                     UINT8_C(0x0E)

/*! I3C IBI Status Register.
 *  This register is clear-on-read.
 */
#define BMI3X0_REG_INT_STATUS_IBI                      UINT8_C(0x0F)

/*! Feature engine General purpose register 0. */
#define BMI3X0_REG_FEATURE_ENGINE_GP_0                 UINT8_C(0x10)

/*! Feature engine General purpose register 1. */
#define BMI3X0_REG_FEATURE_ENGINE_GP_1                 UINT8_C(0x11)

/*! Feature engine General purpose register 2. */
#define BMI3X0_REG_FEATURE_ENGINE_GP_2                 UINT8_C(0x12)

/*! Feature engine General purpose register 3. */
#define BMI3X0_REG_FEATURE_ENGINE_GP_3                 UINT8_C(0x13)

/*! Feature engine GP valid data status/sync and direction bits for each register. Direction bits are read-only for HIF and
 * read-write for FEATURE_ENGINE. */
#define BMI3X0_REG_FEATURE_ENGINE_GP_STATUS            UINT8_C(0x14)

/*! FIFO fill state in words */
#define BMI3X0_REG_FIFO_FILL_LVL                       UINT8_C(0x15)

/*! FIFO data output register */
#define BMI3X0_REG_FIFO_DATA                           UINT8_C(0x16)

/*! Sets the output data rate, bandwidth, range and the mode of the Accelerometer */
#define BMI3X0_REG_ACC_CONF                            UINT8_C(0x20)

/*! Sets the output data rate, bandwidth, range and the mode of the Gyroscope in the sensor */
#define BMI3X0_REG_GYR_CONF                            UINT8_C(0x21)

/*! Alternate configuration for accel */
#define BMI3X0_REG_ALT_ACC_CONF                        UINT8_C(0x28)

/*! Alternate gyro configuration */
#define BMI3X0_REG_ALT_GYR_CONF                        UINT8_C(0x29)

/*! Alternate configuration control */
#define BMI3X0_REG_ALT_CONF                            UINT8_C(0x2A)

/*! Reports configuration active for gyro and accel */
#define BMI3X0_REG_ALT_STATUS                          UINT8_C(0x2B)

/*! FIFO watermark level */
#define BMI3X0_REG_FIFO_WATERMARK                      UINT8_C(0x35)

/*! FIFO configuration */
#define BMI3X0_REG_FIFO_CONF                           UINT8_C(0x36)

/*! FIFO Control */
#define BMI3X0_REG_FIFO_CTRL                           UINT8_C(0x37)

/*! Configures the electrical behavior of the interrupt pins */
#define BMI3X0_REG_IO_INT_CTRL                         UINT8_C(0x38)

/*! Interrupt Configuration Register. */
#define BMI3X0_REG_INT_CONF                            UINT8_C(0x39)

/*! Interrupt Mapping Register for feature engine interrupts A-H. */
#define BMI3X0_REG_INT_MAP1                            UINT8_C(0x3A)

/*! Interrupt Mapping Register for feature engine interrupts I-K, DRDY and FIFO. */
#define BMI3X0_REG_INT_MAP2                            UINT8_C(0x3B)

/*! Global feature engine control register */
#define BMI3X0_REG_FEATURE_ENGINE_GLOB_CTRL            UINT8_C(0x40)

/*! Start address of the DMA transaction. Has to be written to initate a transaction. */
#define BMI3X0_REG_FEATURE_ENGINE_DMA_TX               UINT8_C(0x41)

/*! DMA read/write data. On read transaction expect first word to be zero. */
#define BMI3X0_REG_FEATURE_ENGINE_DMA_TX_DATA          UINT8_C(0x42)

/*! DMA Status Register. */
#define BMI3X0_REG_FEATURE_ENGINE_DMA_STATUS           UINT8_C(0x43)

/*! RESERVED IN DATASHEET. */
#define BMI3X0_REG_FEATURE_ENGINE_DMA_RESERVED         UINT8_C(0x44)

/*! Feature engine Status Register. */
#define BMI3X0_REG_FEATURE_ENGINE_STATUS               UINT8_C(0x45)

/*! Feature engine Algo event status Register. */
#define BMI3X0_REG_FEATURE_ENGINE_EVENT_STATUS         UINT8_C(0x47)

/*! Feature engine Program Counter Register. */
#define BMI3X0_REG_FEATURE_ENGINE_PC                   UINT8_C(0x48)

/*! Feature engine Stack Pointer Register. */
#define BMI3X0_REG_FEATURE_ENGINE_SP                   UINT8_C(0x49)

/*! Feature engine Link Register. */
#define BMI3X0_REG_FEATURE_ENGINE_LR                   UINT8_C(0x4A)

/*! BMI3 I2C address */
#define BMI3X0_ADDR_I2C_PRIM                           UINT8_C(0x68)
#define BMI3X0_ADDR_I2C_SEC                            UINT8_C(0x69)

/*! Interface SPI register */
#define BMI3X0_REG_IO_SPI_IF                           UINT8_C(0x50)

/*! Pads Strength Trim Register (OTP backed) - User mirror register */
#define BMI3X0_REG_IO_PAD_STRENGTH                     UINT8_C(0x51)

/*! HIF configuration bits (OTP backed) - User mirror register. */
#define BMI3X0_REG_HIF_CONF                            UINT8_C(0x52)

/*! ODR Deviation Trim Register (OTP backed) - User mirror register */
#define BMI3X0_REG_IO_ODR_DEVIATION                    UINT8_C(0x53)

/*! Temperature accel user OFF register - Axis X */
#define BMI3X0_REG_TEMP_ACC_USR_OFF_X                  UINT8_C(0x60)

/*! Temperature accel user DGain register - Axis X */
#define BMI3X0_REG_TEMP_ACC_USR_DGAIN_X                UINT8_C(0x61)

/*! Temperature accel user OFF register - Axis Y */
#define BMI3X0_REG_TEMP_ACC_USR_OFF_Y                  UINT8_C(0x62)

/*! Temperature accel user DGain register - Axis Y */
#define BMI3X0_REG_TEMP_ACC_USR_DGAIN_Y                UINT8_C(0x63)

/*! Temperature accel user OFF register - Axis Z */
#define BMI3X0_REG_TEMP_ACC_USR_OFF_Z                  UINT8_C(0x64)

/*! Temperature accel user DGain register - Axis Z */
#define BMI3X0_REG_TEMP_ACC_USR_DGAIN_Z                UINT8_C(0x65)

/*! Temperature gyro user OFF register - Axis X */
#define BMI3X0_REG_TEMP_GYR_USR_OFF_X                  UINT8_C(0x66)

/*! Temperature gyro user DGain register - Axis X */
#define BMI3X0_REG_TEMP_GYR_USR_DGAIN_X                UINT8_C(0x67)

/*! Temperature gyro user OFF register - Axis Y */
#define BMI3X0_REG_TEMP_GYR_USR_OFF_Y                  UINT8_C(0x68)

/*! Temperature gyro user DGain register - Axis Y */
#define BMI3X0_REG_TEMP_GYR_USR_DGAIN_Y                UINT8_C(0x69)

/*! Temperature gyro user OFF register - Axis Z */
#define BMI3X0_REG_TEMP_GYR_USR_OFF_Z                  UINT8_C(0x6A)

/*! Temperature gyro user DGain register - Axis Z */
#define BMI3X0_REG_TEMP_GYR_USR_DGAIN_Z                UINT8_C(0x6B)

/*! Diverse control bits to influence datapath. */
#define BMI3X0_REG_GYR_DP_CTRL                         UINT8_C(0x6D)

/*! Diverse control bits to influence datapath. */
#define BMI3X0_REG_ACC_DP_CTRL                         UINT8_C(0x6E)

/*! I3C Timing Control Sync TPH Register */
#define BMI3X0_REG_I3C_TC_SYNC_TPH                     UINT8_C(0x70)

/*! I3C Timing Control Sync TU Register */
#define BMI3X0_REG_I3C_TC_SYNC_TU                      UINT8_C(0x71)

/*! I3C Timing Control Sync ODR Register */
#define BMI3X0_REG_I3C_TC_SYNC_ODR                     UINT8_C(0x72)

/*! Command Register */
#define BMI3X0_REG_CMD                                 UINT8_C(0x7E)

/*! Register Map Control Register */
#define BMI3X0_REG_EXT_MODE                            UINT8_C(0x7F)

/*! Macro to define start address of data in RAM patch */
#define BMI3X0_CONFIG_ARRAY_DATA_START_ADDR            UINT8_C(4)

/********************************************************* */
/*!               Macros for bit masking                  */
/********************************************************* */

/*! Programmable (OTP) part of chip id. */
#define BMI3X0_CHIP_ID_OTP_MASK                        UINT16_C(0x000F)

/*! Fixed part of chip id. */
#define BMI3X0_CHIP_ID_FIXED_MASK                      UINT16_C(0x00F0)
#define BMI3X0_CHIP_ID_FIXED_POS                       UINT8_C(4)

/*! Minor revision code 00 = A, 01 = B, ... */
#define BMI3X0_REV_ID_MINOR_MASK                       UINT16_C(0x0F00)
#define BMI3X0_REV_ID_MINOR_POS                        UINT8_C(8)

/*! Major revision code 00 = A, 01 = B, ... */
#define BMI3X0_REV_ID_MAJOR_MASK                       UINT16_C(0xF000)
#define BMI3X0_REV_ID_MAJOR_POS                        UINT8_C(12)

/*! Fatal Error, chip is not in operational state (Boot-, power-system). This flag will be reset only by power-on-reset
 * or soft-reset. */
#define BMI3X0_FATAL_ERR_MASK                          UINT16_C(0x0001)

/*! uC interrupt request overrun. This flag is clear-on-read. */
#define BMI3X0_UC_IRQ_OVRN_MASK                        UINT16_C(0x0004)
#define BMI3X0_UC_IRQ_OVRN_POS                         UINT8_C(2)

/*! uC watch cell indication. This flag is clear-on-read. */
#define BMI3X0_UC_WC_MASK                              UINT16_C(0x0008)
#define BMI3X0_UC_WC_POS                               UINT8_C(3)

/*! uC watchdog timer trigerred. This flag is clear-on-read. */
#define BMI3X0_UC_WD_MASK                              UINT16_C(0x0010)
#define BMI3X0_UC_WD_POS                               UINT8_C(4)

/*! Unsupported accelerometer configuration set by user.
 * This flag will be reset when configuration has been corrected.
 */
#define BMI3X0_ACC_CONF_ERR_MASK                       UINT16_C(0x0020)
#define BMI3X0_ACC_CONF_ERR_POS                        UINT8_C(5)

/*! Unsupported gyroscope configuration set by user.
 * This flag will be reset when configuration has been corrected.
 */
#define BMI3X0_GYR_CONF_ERR_MASK                       UINT16_C(0x0040)
#define BMI3X0_GYR_CONF_ERR_POS                        UINT8_C(6)

/*! Unsupported temperature sensor configuration set by user.
 * This flag will be reset when configuration has been corrected.
 */
#define BMI3X0_TEMP_CONF_ERR_MASK                      UINT16_C(0x0080)
#define BMI3X0_TEMP_CONF_ERR_POS                       UINT8_C(7)

/*! SDR parity error or read abort condition (maximum clock stall time for I3C Read Trasfer) occurred.
 *  This flag is a clear-on-read type. It is cleared automatically once read.
 *  Refer to the MIPI I3C specification chapter 'Master Clock Stalling' for detail info regarding the read abort
 * condition.
 */
#define BMI3X0_I3C_ERROR0_MASK                         UINT16_C(0x0100)
#define BMI3X0_I3C_ERROR0_POS                          UINT8_C(8)

/*! S0/S1 error occurred. When S0/S1 error occurs, the slave will recover automatically after 60 us as if we see a
 * HDR-exit pattern on the bus while the flag will persist for notification purpose.
 * This flag is clear-on-read type. It is cleared automatically once read.
 */
#define BMI3X0_I3C_ERROR3_MASK                         UINT16_C(0x0800)
#define BMI3X0_I3C_ERROR3_POS                          UINT8_C(11)

/*! '1' after device power up or soft-reset. This flag is clear-on-read. */
#define BMI3X0_POR_DETECTED_MASK                       UINT8_C(0x01)

#define BMI3X0_FEAT_ENG_INACT_MASK                     UINT8_C(0x0)

#define BMI3X0_FEAT_ENG_ACT_MASK                       UINT8_C(0x1)

#define BMI3X0_INIT_CRC_ERR_MASK                       UINT8_C(0x3)

#define BMI3X0_UGAIN_OFFS_UPD_ERR_MASK                 UINT8_C(0x4)

#define BMI3X0_NO_ERROR_MASK                           UINT8_C(0x5)

#define BMI3X0_AXIS_MAP_ERR_MASK                       UINT8_C(0x6)

#define BMI3X0_TCSYNC_CONF_ERR_MASK                    UINT8_C(0x8)

#define BMI3X0_SC_ST_ABORTED_MASK                      UINT8_C(0x9)

#define BMI3X0_SC_IGNORED_MASK                         UINT8_C(0xA)

#define BMI3X0_ST_IGNORED_MASK                         UINT8_C(0xB)

#define BMI3X0_SC_ST_PRECON_ERR_MASK                   UINT8_C(0xC)

#define BMI3X0_MODE_CHANGE_WHILE_SC_ST_MASK            UINT8_C(0xD)

#define BMI3X0_POSTPONE_I3C_SYNC_MASK                  UINT8_C(0xE)

#define BMI3X0_MODE_CHANGE_WHILE_I3C_SYNC_MASK         UINT8_C(0xF)

#define BMI3X0_ST_ACC_EN_MASK                          UINT8_C(0x1)

#define BMI3X0_ST_GYRO_EN_MASK                         UINT8_C(0x2)
#define BMI3X0_ST_GYRO_EN_POS                          UINT8_C(1)

#define BMI3X0_ST_ACC_GYR_EN_MASK                      UINT8_C(0x3)

#define BMI3X0_SC_ST_STATUS_MASK                       UINT8_C(0x10)
#define BMI3X0_SC_ST_RESULT_MASK                       UINT8_C(0x40)

#define BMI3X0_ST_ACC_X_OK_MASK                        UINT8_C(0x01)
#define BMI3X0_ST_ACC_Y_OK_MASK                        UINT8_C(0x02)
#define BMI3X0_ST_ACC_Z_OK_MASK                        UINT8_C(0x04)
#define BMI3X0_ST_GYR_X_OK_MASK                        UINT8_C(0x08)
#define BMI3X0_ST_GYR_Y_OK_MASK                        UINT8_C(0x10)
#define BMI3X0_ST_GYR_Z_OK_MASK                        UINT8_C(0x20)
#define BMI3X0_ST_GYR_DRIVE_OK_MASK                    UINT8_C(0x40)

#define BMI3X0_ST_ACC_X_OK_POS                         UINT8_C(0)
#define BMI3X0_ST_ACC_Y_OK_POS                         UINT8_C(1)
#define BMI3X0_ST_ACC_Z_OK_POS                         UINT8_C(2)
#define BMI3X0_ST_GYR_X_OK_POS                         UINT8_C(3)
#define BMI3X0_ST_GYR_Y_OK_POS                         UINT8_C(4)
#define BMI3X0_ST_GYR_Z_OK_POS                         UINT8_C(5)
#define BMI3X0_ST_GYR_DRIVE_OK_POS                     UINT8_C(6)

/*! Macros to define values of BMI3 axis and its sign for re-map settings */
#define BMI3X0_MAP_XYZ_AXIS                            UINT8_C(0x00)
#define BMI3X0_MAP_YXZ_AXIS                            UINT8_C(0x01)
#define BMI3X0_MAP_XZY_AXIS                            UINT8_C(0x02)
#define BMI3X0_MAP_ZXY_AXIS                            UINT8_C(0x03)
#define BMI3X0_MAP_YZX_AXIS                            UINT8_C(0x04)
#define BMI3X0_MAP_ZYX_AXIS                            UINT8_C(0x05)
#define BMI3X0_MAP_POSITIVE                            UINT8_C(0x00)
#define BMI3X0_MAP_NEGATIVE                            UINT8_C(0x01)

/*! Macros to define polarity */
#define BMI3X0_NEG_SIGN                                UINT8_C(1)
#define BMI3X0_POS_SIGN                                UINT8_C(0)

/*!     Remap Axes */
/**************************************************************/
#define BMI3X0_XYZ_AXIS_MASK                           UINT8_C(0x07)
#define BMI3X0_X_AXIS_SIGN_MASK                        UINT8_C(0x08)
#define BMI3X0_Y_AXIS_SIGN_MASK                        UINT8_C(0x10)
#define BMI3X0_Z_AXIS_SIGN_MASK                        UINT8_C(0x20)

/*! Bit position definitions of BMI3 axis re-mapping */
#define BMI3X0_X_AXIS_SIGN_POS                         UINT8_C(3)
#define BMI3X0_Y_AXIS_SIGN_POS                         UINT8_C(4)
#define BMI3X0_Z_AXIS_SIGN_POS                         UINT8_C(5)

/******************************************************************************/
/*!        Macro Definitions for Axes re-mapping             */
/******************************************************************************/
/*!  Macros for the user-defined values of axes and their polarities */
#define BMI3X0_XYZ                                     UINT8_C(0x00)
#define BMI3X0_YXZ                                     UINT8_C(0x01)
#define BMI3X0_XZY                                     UINT8_C(0x02)
#define BMI3X0_ZXY                                     UINT8_C(0x03)
#define BMI3X0_YZX                                     UINT8_C(0x04)
#define BMI3X0_ZYX                                     UINT8_C(0x05)

#define BMI3X0_INVERT                                  UINT8_C(0x01)
#define BMI3X0_NOT_INVERT                              UINT8_C(0x00)

/*! Data ready for Temperature. This flag is clear-on-read. */
#define BMI3X0_DRDY_TEMP_MASK                          UINT16_C(0x0800)
#define BMI3X0_DRDY_TEMP_POS                           UINT8_C(11)

/*! Data ready for Gyroscope. This flag is clear-on-read. */
#define BMI3X0_DRDY_GYR_MASK                           UINT8_C(0x1000)
#define BMI3X0_DRDY_GYR_POS                            UINT8_C(12)

/*! Data ready for Accelerometer. This flag is clear-on-read. */
#define BMI3X0_DRDY_ACC_MASK                           UINT8_C(0x2000)
#define BMI3X0_DRDY_ACC_POS                            UINT8_C(13)

/*! Accel x-axis mask and bit position */
#define BMI3X0_ACC_X_MASK                              UINT16_C(0xFFFF)

/*! Accel y-axis mask and bit position */
#define BMI3X0_ACC_Y_MASK                              UINT16_C(0xFFFF)

/*! Accel z-axis mask and bit position */
#define BMI3X0_ACC_Z_MASK                              UINT16_C(0xFFFF)

/*! Gyro x-axis mask and bit position */
#define BMI3X0_GYR_X_MASK                              UINT16_C(0xFFFF)

/*! Gyro y-axis mask and bit position */
#define BMI3X0_GYR_Y_MASK                              UINT16_C(0xFFFF)

/*! Gyro z-axis mask and bit position */
#define BMI3X0_GYR_Z_MASK                              UINT16_C(0xFFFF)

/*! Temperature value.
 *  T (deg C) := temp_data/512 + 23
 */
#define BMI3X0_TEMP_DATA_MASK                          UINT16_C(0xFFFF)

/*! Sensor time LSW (15:0) */
#define BMI3X0_SENSOR_TIME_15_0_MASK                   UINT16_C(0xFFFF)

/*! Sensor time MSW (31:16) */
#define BMI3X0_SENSOR_TIME_31_16_MASK                  UINT16_C(0xFFFF)

/*! Saturation flag for accel X axis */
#define BMI3X0_SATF_ACC_X_MASK                         UINT16_C(0x0001)

/*! Saturation flag for accel Y axis */
#define BMI3X0_SATF_ACC_Y_MASK                         UINT16_C(0x0002)
#define BMI3X0_SATF_ACC_Y_POS                          UINT8_C(1)

/*! Saturation flag for accel Z axis */
#define BMI3X0_SATF_ACC_Z_MASK                         UINT16_C(0x0004)
#define BMI3X0_SATF_ACC_Z_POS                          UINT8_C(2)

/*! Saturation flag for gyro X axis */
#define BMI3X0_SATF_GYR_X_MASK                         UINT16_C(0x0008)
#define BMI3X0_SATF_GYR_X_POS                          UINT8_C(3)

/*! Saturation flag for gyro Y axis */
#define BMI3X0_SATF_GYR_Y_MASK                         UINT16_C(0x0010)
#define BMI3X0_SATF_GYR_Y_POS                          UINT8_C(4)

/*! Saturation flag for gyro Z axis */
#define BMI3X0_SATF_GYR_Z_MASK                         UINT16_C(0x0020)
#define BMI3X0_SATF_GYR_Z_POS                          UINT8_C(5)

/*! Feature interrupt status bit values */
#define BMI3X0_INT_STATUS_NO_MOTION                    UINT16_C(0x0001)
#define BMI3X0_INT_STATUS_ANY_MOTION                   UINT16_C(0x0002)
#define BMI3X0_INT_STATUS_FLAT                         UINT16_C(0x0004)
#define BMI3X0_INT_STATUS_ORIENTATION                  UINT16_C(0x0008)
#define BMI3X0_INT_STATUS_STEP_DETECTOR                UINT16_C(0x0010)
#define BMI3X0_INT_STATUS_STEP_COUNTER                 UINT16_C(0x0020)
#define BMI3X0_INT_STATUS_SIG_MOTION                   UINT16_C(0x0040)
#define BMI3X0_INT_STATUS_TILT                         UINT16_C(0x0080)
#define BMI3X0_INT_STATUS_TAP                          UINT16_C(0x0100)
#define BMI3X0_INT_STATUS_I3C                          UINT16_C(0x0200)
#define BMI3X0_INT_STATUS_ERR                          UINT16_C(0x0400)
#define BMI3X0_INT_STATUS_TEMP_DRDY                    UINT16_C(0x0800)
#define BMI3X0_INT_STATUS_GYR_DRDY                     UINT16_C(0x1000)
#define BMI3X0_INT_STATUS_ACC_DRDY                     UINT16_C(0x2000)
#define BMI3X0_INT_STATUS_FWM                          UINT16_C(0x4000)
#define BMI3X0_INT_STATUS_FFULL                        UINT16_C(0x8000)

/*! No-motion output */
#define BMI3X0_IBI_STATUS_NO_MOTION                    UINT16_C(0x0001)
#define BMI3X0_IBI_STATUS_ANY_MOTION                   UINT16_C(0x0002)
#define BMI3X0_IBI_STATUS_FLAT                         UINT16_C(0x0004)
#define BMI3X0_IBI_STATUS_ORIENTATION                  UINT16_C(0x0008)
#define BMI3X0_IBI_STATUS_STEP_DETECTOR                UINT16_C(0x0010)
#define BMI3X0_IBI_STATUS_STEP_COUNTER                 UINT16_C(0x0020)
#define BMI3X0_IBI_STATUS_SIG_MOTION                   UINT16_C(0x0040)
#define BMI3X0_IBI_STATUS_TILT                         UINT16_C(0x0080)
#define BMI3X0_IBI_STATUS_TAP                          UINT16_C(0x0100)
#define BMI3X0_IBI_STATUS_I3C                          UINT16_C(0x0200)
#define BMI3X0_IBI_STATUS_ERR_STATUS                   UINT16_C(0x0400)
#define BMI3X0_IBI_STATUS_TEMP_DRDY                    UINT16_C(0x0800)
#define BMI3X0_IBI_STATUS_GYR_DRDY                     UINT16_C(0x1000)
#define BMI3X0_IBI_STATUS_ACC_DRDY                     UINT16_C(0x2000)
#define BMI3X0_IBI_STATUS_FWM                          UINT16_C(0x4000)
#define BMI3X0_IBI_STATUS_FFULL                        UINT16_C(0x8000)
#define BMI3X0_IBI_STATUS_I3C_SYNC_EN                  UINT16_C(0x8000)
#define BMI3X0_IBI_STATUS_ERROR                        UINT16_C(0x000F)

/*! Self-calibration (gyroscope only) or self-test (accelerometer and/or gyroscope) execution status. 0 indicates that
 * the procedure is ongoing. 1 indicates that the procedure is completed. */
#define BMI3X0_SC_ST_COMPLETE_MASK                     UINT16_C(0x0010)
#define BMI3X0_SC_ST_COMPLETE_POS                      UINT8_C(4)

/*! Gyroscope self-calibration result (1=OK, 0=Not OK). Bit sc_st_complete should be 1 prior to reading this bit. */
#define BMI3X0_GYRO_SC_RESULT_MASK                     UINT16_C(0x0020)
#define BMI3X0_GYRO_SC_RESULT_POS                      UINT8_C(5)

/*! Accelerometer and/or gyroscope self-test result (1=OK, 0=Not OK). Bit sc_st_complete should be 1 prior to reading
 * this bit. */
#define BMI3X0_ST_RESULT_MASK                          UINT16_C(0x0040)
#define BMI3X0_ST_RESULT_POS                           UINT8_C(6)

/*! Insufficient sample rate for either 50Hz or 200Hz or I3C TC-sync feature */
#define BMI3X0_SAMPLE_RATE_ERR_MASK                    UINT16_C(0x0080)
#define BMI3X0_SAMPLE_RATE_ERR_POS                     UINT8_C(7)

/*! User gain/offset update execution status. 0 indicates that the procedure is ongoing. 1 indicates that the procedure
 * is completed */
#define BMI3X0_UGAIN_OFFS_UPD_COMPLETE_MASK            UINT16_C(0x0100)
#define BMI3X0_UGAIN_OFFS_UPD_COMPLETE_POS             UINT8_C(8)

/*! Memory test result (1=OK, 0=Not OK) */
#define BMI3X0_MBIST_RESULT_MASK                       UINT16_C(0x0200)
#define BMI3X0_MBIST_RESULT_POS                        UINT8_C(9)

/*! Axis mapping completed */
#define BMI3X0_AXIS_MAP_COMPLETE_MASK                  UINT16_C(0x0400)
#define BMI3X0_AXIS_MAP_COMPLETE_POS                   UINT8_C(10)

/*! Current state of the system */
#define BMI3X0_STATE_MASK                              UINT16_C(0x1800)
#define BMI3X0_STATE_POS                               UINT8_C(11)

/*! Step counter value word-0 (low word) */
#define BMI3X0_STEP_COUNTER_OUT_0_MASK                 UINT16_C(0x0FFFF)

/*! Step counter value word-1 (high word) */
#define BMI3X0_STEP_COUNTER_OUT_1_MASK                 UINT16_C(0x0FFFF)

/*! A data has been written to any of the FEATURE_ENGINE_GP_x registers. Write 1 to clear and sync. */
#define BMI3X0_FEATURE_ENGINE_GP_STS_MASK              UINT16_C(0x0001)

/*! Direction (for HIF) of the FEATURE_ENGINE_GP0 register. 0-read-only, 1-write-only */
#define BMI3X0_FEATURE_ENGINE_GP0_HIF_DIR_MASK         UINT16_C(0x0002)
#define BMI3X0_FEATURE_ENGINE_GP0_HIF_DIR_POS          UINT8_C(1)

/*! Direction (for HIF) of the FEATURE_ENGINE_GP1 register. 0-read-only, 1-write-only */
#define BMI3X0_FEATURE_ENGINE_GP1_HIF_DIR_MASK         UINT16_C(0x0004)
#define BMI3X0_FEATURE_ENGINE_GP1_HIF_DIR_POS          UINT8_C(2)

/*! Direction (for HIF) of the FEATURE_ENGINE_GP2 register. 0-read-only, 1-write-only */
#define BMI3X0_FEATURE_ENGINE_GP2_HIF_DIR_MASK         UINT16_C(0x0008)
#define BMI3X0_FEATURE_ENGINE_GP2_HIF_DIR_POS          UINT8_C(3)

/*! Direction (for HIF) of the FEATURE_ENGINE_GP3 register. 0-read-only, 1-write-only */
#define BMI3X0_FEATURE_ENGINE_GP3_HIF_DIR_MASK         UINT16_C(0x0010)
#define BMI3X0_FEATURE_ENGINE_GP3_HIF_DIR_POS          UINT8_C(4)

/*! Current fill level of FIFO buffer
 * An empty FIFO corresponds to 0x000. The word counter may be reset by reading out all frames from the FIFO buffer or
 * when the FIFO is reset through fifo_flush. The word counter is updated each time a complete frame was read or
 * written. */
#define BMI3X0_FIFO_FILL_LVL_MASK                      UINT16_C(0x07FF)

/*! FIFO read data (16 bits)
 * Data format depends on the setting of register FIFO_CONF. The FIFO data are organized in frames. The new data flag is
 * preserved. Read burst access must be used, the address will not increment when the read burst reads at the address of
 * FIFO_DATA. When a frame is only partially read out it is retransmitted including the header at the next readout. */
#define BMI3X0_FIFO_DATA_MASK                          UINT16_C(0x0FFFF)

/*! ODR in Hz */
#define BMI3X0_ACC_ODR_MASK                            UINT16_C(0x000F)

/*! Full scale, Resolution */
#define BMI3X0_ACC_RANGE_MASK                          UINT16_C(0x0070)
#define BMI3X0_ACC_RANGE_POS                           UINT8_C(4)

/*! The Accel bandwidth coefficient defines the 3 dB cutoff frequency in relation to the ODR */
#define BMI3X0_ACC_BW_MASK                             UINT16_C(0x0080)
#define BMI3X0_ACC_BW_POS                              UINT8_C(7)

/*! Number of samples to be averaged */
#define BMI3X0_ACC_AVG_NUM_MASK                        UINT16_C(0x0700)
#define BMI3X0_ACC_AVG_NUM_POS                         UINT8_C(8)

/*! Defines mode of operation for Accelerometer. DO NOT COPY OPERATION DESCRIPTION TO CUSTOMER SPEC! */
#define BMI3X0_ACC_MODE_MASK                           UINT16_C(0x7000)
#define BMI3X0_ACC_MODE_POS                            UINT8_C(12)

/*! ODR in Hz */
#define BMI3X0_GYR_ODR_MASK                            UINT16_C(0x000F)

/*! Full scale, Resolution */
#define BMI3X0_GYR_RANGE_MASK                          UINT16_C(0x0070)
#define BMI3X0_GYR_RANGE_POS                           UINT8_C(4)

/*! The Gyroscope bandwidth coefficient defines the 3 dB cutoff frequency in relation to the ODR */
#define BMI3X0_GYR_BW_MASK                             UINT16_C(0x0080)
#define BMI3X0_GYR_BW_POS                              UINT8_C(7)

/*! Number of samples to be averaged */
#define BMI3X0_GYR_AVG_NUM_MASK                        UINT16_C(0x0700)
#define BMI3X0_GYR_AVG_NUM_POS                         UINT8_C(8)

/*! Defines mode of operation for Gyroscope.DO NOT COPY OPERATION DESCRIPTION TO CUSTOMER SPEC! */
#define BMI3X0_GYR_MODE_MASK                           UINT16_C(0x7000)
#define BMI3X0_GYR_MODE_POS                            UINT8_C(12)

/*! ODR in Hz */
#define BMI3X0_ALT_ACC_ODR_MASK                        UINT16_C(0x000F)

/*! Number of samples to be averaged */
#define BMI3X0_ALT_ACC_AVG_NUM_MASK                    UINT16_C(0x0700)
#define BMI3X0_ALT_ACC_AVG_NUM_POS                     UINT8_C(8)

/*! Defines mode of operation for Accelerometer. DO NOT COPY OPERATION DESCRIPTION TO CUSTOMER SPEC! */
#define BMI3X0_ALT_ACC_MODE_MASK                       UINT16_C(0x7000)
#define BMI3X0_ALT_ACC_MODE_POS                        UINT8_C(12)

/*! ODR in Hz */
#define BMI3X0_ALT_GYR_ODR_MASK                        UINT16_C(0x000F)

/*! Number of samples to be averaged */
#define BMI3X0_ALT_GYR_AVG_NUM_MASK                    UINT16_C(0x0700)
#define BMI3X0_ALT_GYR_AVG_NUM_POS                     UINT8_C(8)

/*! Defines mode of operation for Gyroscope.DO NOT COPY OPERATION DESCRIPTION TO CUSTOMER SPEC! */
#define BMI3X0_ALT_GYR_MODE_MASK                       UINT16_C(0x7000)
#define BMI3X0_ALT_GYR_MODE_POS                        UINT8_C(12)

/*! Enables switching possibility to alternate configuration for accel */
#define BMI3X0_ALT_ACC_EN_MASK                         UINT16_C(0x0001)

/*! Enables switching possibility to alternate configuration for gyro */
#define BMI3X0_ALT_GYR_EN_MASK                         UINT16_C(0x0010)
#define BMI3X0_ALT_GYR_EN_POS                          UINT8_C(4)

/*! Accel is using ALT_ACC_CONF if set; ACC_CONF otherwise */
#define BMI3X0_ALT_ACC_ACTIVE_MASK                     UINT16_C(0x0001)

/*! Gyro is using ALT_GYR_CONF if set; GYR_CONF otherwise */
#define BMI3X0_ALT_GYR_ACTIVE_MASK                     UINT16_C(0x0010)
#define BMI3X0_ALT_GYR_ACTIVE_POS                      UINT8_C(4)

/*! Trigger an interrupt when FIFO contains fifo_watermark words */
#define BMI3X0_FIFO_WATERMARK_MASK                     UINT16_C(0x03FF)

/*! Stop writing samples into fifo when fifo is full */
#define BMI3X0_FIFO_STOP_ON_FULL_MASK                  UINT16_C(0x0001)

/*! Store Sensortime data in FIFO */
#define BMI3X0_FIFO_TIME_EN_MASK                       UINT16_C(0x0100)
#define BMI3X0_FIFO_TIME_EN_POS                        UINT8_C(8)

/*! Store Accelerometer data in FIFO (all 3 axes) */
#define BMI3X0_FIFO_ACC_EN_MASK                        UINT16_C(0x0200)
#define BMI3X0_FIFO_ACC_EN_POS                         UINT8_C(9)

/*! Store Gyroscope data in FIFO (all 3 axes) */
#define BMI3X0_FIFO_GYR_EN_MASK                        UINT16_C(0x0400)
#define BMI3X0_FIFO_GYR_EN_POS                         UINT8_C(10)

/*! Store temperature data in FIFO */
#define BMI3X0_FIFO_TEMP_EN_MASK                       UINT16_C(0x0800)
#define BMI3X0_FIFO_TEMP_EN_POS                        UINT8_C(11)

/*! Clear all fifo data; do not touch configuration. */
#define BMI3X0_FIFO_FLUSH_MASK                         UINT16_C(0x0001)

/*! Maximum number of interrupt pins */
#define BMI3X0_INT_PIN_MAX_NUM                         UINT8_C(2)

/*! Configure level of INT1 pin */
#define BMI3X0_INT1_LVL_MASK                           UINT16_C(0x0001)

/*! Configure behavior of INT1 pin */
#define BMI3X0_INT1_OD_MASK                            UINT16_C(0x0002)
#define BMI3X0_INT1_OD_POS                             UINT8_C(1)

/*! Output enable for INT1 pin */
#define BMI3X0_INT1_OUTPUT_EN_MASK                     UINT16_C(0x0004)
#define BMI3X0_INT1_OUTPUT_EN_POS                      UINT8_C(2)

/*! Input enable for INT1 pin */
#define BMI3X0_INT1_INPUT_EN_MASK                      UINT16_C(0x0008)
#define BMI3X0_INT1_INPUT_EN_POS                       UINT8_C(3)

/*! Configure level of INT2 pin */
#define BMI3X0_INT2_LVL_MASK                           UINT16_C(0x0100)
#define BMI3X0_INT2_LVL_POS                            UINT8_C(8)

/*! Configure behavior of INT2 pin */
#define BMI3X0_INT2_OD_MASK                            UINT16_C(0x0200)
#define BMI3X0_INT2_OD_POS                             UINT8_C(9)

/*! Output enable for INT2 pin */
#define BMI3X0_INT2_OUTPUT_EN_MASK                     UINT16_C(0x0400)
#define BMI3X0_INT2_OUTPUT_EN_POS                      UINT8_C(10)

/*! Input enable for INT2 pin */
#define BMI3X0_INT2_INPUT_EN_MASK                      UINT16_C(0x0800)
#define BMI3X0_INT2_INPUT_EN_POS                       UINT8_C(11)

/*! Map no-motion output to either INT1 or INT2 or IBI */
#define BMI3X0_NO_MOTION_OUT_MASK                      UINT16_C(0x0003)

/*! Map any-motion output to either INT1 or INT2 or IBI */
#define BMI3X0_ANY_MOTION_OUT_MASK                     UINT16_C(0x000C)
#define BMI3X0_ANY_MOTION_OUT_POS                      UINT8_C(2)

/*! Map flat output to either INT1 or INT2 or IBI */
#define BMI3X0_FLAT_OUT_MASK                           UINT16_C(0x0030)
#define BMI3X0_FLAT_OUT_POS                            UINT8_C(4)

/*! Map orientation output to either INT1 or INT2 or IBI */
#define BMI3X0_ORIENTATION_OUT_MASK                    UINT8_C(0x00C0)
#define BMI3X0_ORIENTATION_OUT_POS                     UINT8_C(6)

/*! Map step_detector output to either INT1 or INT2 or IBI */
#define BMI3X0_STEP_DETECTOR_OUT_MASK                  UINT8_C(0x0300)
#define BMI3X0_STEP_DETECTOR_OUT_POS                   UINT8_C(8)

/*! Map step_counter watermark output to either INT1 or INT2 or IBI */
#define BMI3X0_STEP_COUNTER_OUT_MASK                   UINT8_C(0x0C00)
#define BMI3X0_STEP_COUNTER_OUT_POS                    UINT8_C(10)

/*! Map sigmotion output to either INT1 or INT2 or IBI */
#define BMI3X0_SIG_MOTION_OUT_MASK                     UINT16_C(0x3000)
#define BMI3X0_SIG_MOTION_OUT_POS                      UINT8_C(12)

/*! Map tilt output to either INT1 or INT2 or IBI */
#define BMI3X0_TILT_OUT_MASK                           UINT16_C(0xC000)
#define BMI3X0_TILT_OUT_POS                            UINT8_C(14)

/*! Map tap output to either INT1 or INT2 or IBI */
#define BMI3X0_TAP_OUT_MASK                            UINT16_C(0x0003)
#define BMI3X0_TAP_OUT_POS                             UINT8_C(0)

/*! Map i3c output to either INT1 or INT2 or IBI */
#define BMI3X0_I3C_OUT_MASK                            UINT16_C(0x000C)
#define BMI3X0_I3C_OUT_POS                             UINT8_C(2)

/*! Map feature engine's error or status change to either INT1 or INT2 or IBI */
#define BMI3X0_ERR_STATUS_MASK                         UINT16_C(0x0030)
#define BMI3X0_ERR_STATUS_POS                          UINT8_C(4)

/*! Map temperature data ready interrupt to either INT1 or INT2 or IBI */
#define BMI3X0_TEMP_DRDY_INT_MASK                      UINT16_C(0x00C0)
#define BMI3X0_TEMP_DRDY_INT_POS                       UINT8_C(6)

/*! Map gyro data ready interrupt to either INT1 or INT2 or IBI */
#define BMI3X0_GYR_DRDY_INT_MASK                       UINT16_C(0x0300)
#define BMI3X0_GYR_DRDY_INT_POS                        UINT8_C(8)

/*! Map accel data ready interrupt to either INT1 or INT2 or IBI */
#define BMI3X0_ACC_DRDY_INT_MASK                       UINT16_C(0x0C00)
#define BMI3X0_ACC_DRDY_INT_POS                        UINT8_C(10)

/*! Map FIFO watermark interrupt to either INT1 or INT2 or IBI */
#define BMI3X0_FWM_INT_MASK                            UINT16_C(0x3000)
#define BMI3X0_FWM_INT_POS                             UINT8_C(12)

/*! Map FIFO full interrupt to either INT1 or INT2 or IBI */
#define BMI3X0_FFULL_INT_MASK                          UINT16_C(0xC000)
#define BMI3X0_FFULL_INT_POS                           UINT8_C(14)

/*! Enable FEATURE_ENGINE. When enabled, FCU wakeup triggers for Feature engine are functional. */
#define BMI3X0_FEATURE_ENGINE_EN_MASK                  UINT16_C(0x0001)

/*! Mapping of RAM instances in the system. RESERVED for user in Datasheet! */
#define BMI3X0_MEM_CONF_RAM_MASK                       UINT16_C(0xC000)
#define BMI3X0_MEM_CONF_RAM_POS                        UINT8_C(14)

/*! Start address of DMA transaction */
#define BMI3X0_DMA_TX_ADDR_MASK                        UINT16_C(0x07FF)

/*! Data port for DMA transfers. */
#define BMI3X0_DMA_TX_DATA_MASK                        UINT16_C(0xFFFF)

/*! Out of bounds error. Cleared on next TX. */
#define BMI3X0_DMA_OOB_ERR_MASK                        UINT16_C(0x0001)

/*! DMA is ready to use. If this bit is 0, DMA access will not work. */
#define BMI3X0_DMA_READY_MASK                          UINT16_C(0x0002)
#define BMI3X0_DMA_READY_POS                           UINT8_C(1)

/*! DMA IS UNLOCKED. This field is RESEVED in Datasheet. */
#define BMI3X0_DMA_UNLOCKED_MASK                       UINT16_C(0x8000)
#define BMI3X0_DMA_UNLOCKED_POS                        UINT8_C(15)

/*! Feature engine is in sleep/halt state (will be reset after read) */
#define BMI3X0_SLEEP_MASK                              UINT16_C(0x0001)

/*! Dedicated interupt is set again before previous interrupt was acknowledged.
 * This bit is cleared when all corresponding interrupt overrun flags are cleared.
 */
#define BMI3X0_IRQ_OVRN_MASK                           UINT16_C(0x0002)
#define BMI3X0_IRQ_OVRN_POS                            UINT8_C(1)

/*! Watchcell event detected (FEATURE_ENGINE stopped) */
#define BMI3X0_WC_EVENT_MASK                           UINT16_C(0x0004)
#define BMI3X0_WC_EVENT_POS                            UINT8_C(2)

/*! DMA controller has started DMA and DMA transactions are ongoing */
#define BMI3X0_DMA_ACTIVE_MASK                         UINT16_C(0x0008)
#define BMI3X0_DMA_ACTIVE_POS                          UINT8_C(3)

/*! Feature engine has been disabled by host. */
#define BMI3X0_FEATURE_ENGINE_DISABLED_LOG_MASK        UINT16_C(0x0010)
#define BMI3X0_FEATURE_ENGINE_DISABLED_LOG_POS         UINT8_C(4)

/*! Watchdog event detected */
#define BMI3X0_WD_EVENT_MASK                           UINT16_C(0x0020)
#define BMI3X0_WD_EVENT_POS                            UINT8_C(5)

/*! Output value of the orientation detection feature. Value after device initialization is 0b00 i.e. Portrait upright
 * */
#define BMI3X0_ORIENTATION_PORTRAIT_LANDSCAPE_MASK     UINT16_C(0x0003)

/*! Output value of face down face up orientation (only if ud_en is enabled). Value after device initialization is 0b0
 * i.e. Face up */
#define BMI3X0_ORIENTATION_FACEUP_DOWN_MASK            UINT16_C(0x0004)
#define BMI3X0_ORIENTATION_FACEUP_DOWN_POS             UINT8_C(2)

/*! Single tap detected */
#define BMI3X0_S_TAP_MASK                              UINT16_C(0x0008)
#define BMI3X0_S_TAP_POS                               UINT8_C(3)

/*! Double tap detected */
#define BMI3X0_D_TAP_MASK                              UINT16_C(0x0010)
#define BMI3X0_D_TAP_POS                               UINT8_C(4)

/*! Triple tap detected */
#define BMI3X0_T_TAP_MASK                              UINT16_C(0x0020)
#define BMI3X0_T_TAP_POS                               UINT8_C(5)

/*! Feature Engine Program Counter Register. */
#define BMI3X0_PC_MASK                                 UINT16_C(0xFFFF)

/*! Feature Engine Stack Pointer Register. */
#define BMI3X0_SP_MASK                                 UINT16_C(0xFFFF)

/*! Feature Engine Link Register. */
#define BMI3X0_LR_MASK                                 UINT16_C(0xFFFF)

/*! Mask definitions for SPI read/write address */
#define BMI3X0_SPI_RD_MASK                             UINT16_C(0x80)
#define BMI3X0_SPI_WR_MASK                             UINT16_C(0x7F)

/*! Enable SPI PIN3 mode. Default is PIN4 mode. */
#define BMI3X0_SPI3_EN_MASK                            UINT16_C(0x0001)

/*! Mask definitions for power configuration register */
#define BMI3X0_ADV_POW_EN_MASK                         UINT16_C(0x01)

/*! Generic output pad drive strength primary interface, 0: weakest; 7: strongest */
#define BMI3X0_IF_DRV_MASK                             UINT16_C(0x0007)

/*! SDX I2C control bit - increase drive strength */
#define BMI3X0_IF_I2C_BOOST_MASK                       UINT16_C(0x0008)
#define BMI3X0_IF_I2C_BOOST_POS                        UINT8_C(3)

/*! Select timer period for I2C Watchdog */
#define BMI3X0_I2C_WDT_SEL_MASK                        UINT16_C(0x0001)

/*! I2C Watchdog at the SDI pin in I2C interface mode */
#define BMI3X0_I2C_WDT_EN_MASK                         UINT16_C(0x0002)
#define BMI3X0_I2C_WDT_EN_POS                          UINT8_C(1)

/*! ODR clock deviation */
#define BMI3X0_ODR_DEVIATION_MASK                      UINT16_C(0x001F)

/*! User temperature and independent term for offset compensation in accelerometer - Axis X:
 *          1 LSB = 30.52ug; 0x1000 -> invalid */
#define BMI3X0_ACC_USR_OFF_B0_X_MASK                   UINT16_C(0x1FFF)

/*! User gain correction of accelerometer - Axis X
 *          covers ï¿½3.125% of sensitivity */
#define BMI3X0_ACC_USR_DGAIN_S0_X_MASK                 UINT16_C(0x00FF)

/*! User temperature and independent term for offset compensation in accelerometer - Axis Y:
 *          1 LSB = 30.52ug; 0x1000 -> invalid */
#define BMI3X0_ACC_USR_OFF_B0_Y_MASK                   UINT16_C(0x1FFF)

/*! User gain correction of accelerometer - Axis Y
 *          covers ï¿½3.125% of sensitivity */
#define BMI3X0_ACC_USR_DGAIN_S0_Y_MASK                 UINT16_C(0x00FF)

/*! User temperature and independent term for offset compensation in accelerometer - Axis Z:
 *          1 LSB = 30.52ug; 0x1000 -> invalid */
#define BMI3X0_ACC_USR_OFF_B0_Z_MASK                   UINT16_C(0x1FFF)

/*! User gain correction of accelerometer - Axis Z
 *          covers ï¿½3.125% of sensitivity */
#define BMI3X0_ACC_USR_DGAIN_S0_Z_MASK                 UINT16_C(0x00FF)

/*! User temperature and quadrature independent term for offset compensation in rate - Axis X:
 *          1 LSB = 0.061ï¿½/s; 0x200 -> invalid */
#define BMI3X0_GYR_USR_OFF_B0_X_MASK                   UINT16_C(0x03FF)

/*! User gain correction in rate - Axis X
 *          covers ï¿½12.5% of sensitivity */
#define BMI3X0_GYR_USR_DGAIN_S0_X_MASK                 UINT16_C(0x007F)

/*! User temperature and quadrature independent term for offset compensation in rate - Axis Y:
 *          1 LSB = 0.061ï¿½/s; 0x200 -> invalid */
#define BMI3X0_GYR_USR_OFF_B0_Y_MASK                   UINT16_C(0x03FF)

/*! User gain correction in rate - Axis Y
 *          covers ï¿½12.5% of sensitivity */
#define BMI3X0_GYR_USR_DGAIN_S0_Y_MASK                 UINT16_C(0x007F)

/*! User temperature and quadrature independent term for offset compensation in rate - Axis Z:
 *          1 LSB = 0.061ï¿½/s; 0x200 -> invalid */
#define BMI3X0_GYR_USR_OFF_B0_Z_MASK                   UINT16_C(0x03FF)

/*! User gain correction in rate - Axis Z
 *          covers ï¿½12.5% of sensitivity */
#define BMI3X0_GYR_USR_DGAIN_S0_Z_MASK                 UINT16_C(0x007F)

/*! Disable cross-axis compensation */
#define BMI3X0_GYR_CA_DIS_MASK                         UINT16_C(0x0004)
#define BMI3X0_GYR_CA_DIS_POS                          UINT8_C(2)

/*! Disable cross-axis compensation */
#define BMI3X0_ACC_CA_DIS_MASK                         UINT16_C(0x0004)
#define BMI3X0_ACC_CA_DIS_POS                          UINT8_C(2)

/*! I3C Timing Control Sync TPH Register */
#define BMI3X0_I3C_TC_SYNC_TPH_MASK                    UINT16_C(0xFFFF)

/*! I3C Timing Control Sync TU Register */
#define BMI3X0_I3C_TC_SYNC_TU_MASK                     UINT16_C(0x00FF)

/*! I3C Timing Control Sync ODR Register */
#define BMI3X0_I3C_TC_SYNC_ODR_MASK                    UINT16_C(0x00FF)

/*! Available commands (Note: Register will always return 0x00 as read result) */
#define BMI3X0_CMD_MASK                                UINT16_C(0xFFFF)

/*! Target page address mask and bit position */
#define BMI3X0_TARGET_PAGE_MASK                        UINT16_C(0x001F)

/*! Enable Extended mode Page select. Entering extended mode only via CMD register.
 * Reset to user by writing "00" */
#define BMI3X0_PRIVILEGE_MASK                          UINT16_C(0xC000)
#define BMI3X0_PRIVILEGE_POS                           UINT8_C(14)

/******************************************************************************/
/*!        Macro Definitions for feature enable                 */
/******************************************************************************/
/*! No-motion detection output */
#define BMI3X0_NO_MOTION_EN_X_MASK                     UINT16_C(0x0001)

/*! No-motion detection output */
#define BMI3X0_NO_MOTION_EN_Y_MASK                     UINT16_C(0x0002)
#define BMI3X0_NO_MOTION_EN_Y_POS                      UINT8_C(1)

/*! No-motion detection output */
#define BMI3X0_NO_MOTION_EN_Z_MASK                     UINT16_C(0x0004)
#define BMI3X0_NO_MOTION_EN_Z_POS                      UINT8_C(2)

/*! Any-motion detection output */
#define BMI3X0_ANY_MOTION_EN_X_MASK                    UINT16_C(0x0008)
#define BMI3X0_ANY_MOTION_EN_X_POS                     UINT8_C(3)

/*! Any-motion detection output */
#define BMI3X0_ANY_MOTION_EN_Y_MASK                    UINT16_C(0x0010)
#define BMI3X0_ANY_MOTION_EN_Y_POS                     UINT8_C(4)

/*! Any-motion detection output */
#define BMI3X0_ANY_MOTION_EN_Z_MASK                    UINT16_C(0x0020)
#define BMI3X0_ANY_MOTION_EN_Z_POS                     UINT8_C(5)

/*! Flat detection output */
#define BMI3X0_FLAT_EN_MASK                            UINT16_C(0x0040)
#define BMI3X0_FLAT_EN_POS                             UINT8_C(6)

/*! Orientation detection output */
#define BMI3X0_ORIENTATION_EN_MASK                     UINT16_C(0x0080)
#define BMI3X0_ORIENTATION_EN_POS                      UINT8_C(7)

/*! Step detector output */
#define BMI3X0_STEP_DETECTOR_EN_MASK                   UINT16_C(0x0100)
#define BMI3X0_STEP_DETECTOR_EN_POS                    UINT8_C(8)

/*! Step counter watermark output */
#define BMI3X0_STEP_COUNTER_EN_MASK                    UINT16_C(0x0200)
#define BMI3X0_STEP_COUNTER_EN_POS                     UINT8_C(9)

/*! Sigmotion detection output */
#define BMI3X0_SIG_MOTION_EN_MASK                      UINT16_C(0x0400)
#define BMI3X0_SIG_MOTION_EN_POS                       UINT8_C(10)

/*! Tilt detection output */
#define BMI3X0_TILT_EN_MASK                            UINT16_C(0x0800)
#define BMI3X0_TILT_EN_POS                             UINT8_C(11)

/*! Tap detection output */
#define BMI3X0_S_TAP_EN_MASK                           UINT16_C(0x1000)
#define BMI3X0_S_TAP_EN_POS                            UINT8_C(12)

#define BMI3X0_D_TAP_EN_MASK                           UINT16_C(0x2000)
#define BMI3X0_D_TAP_EN_POS                            UINT8_C(13)

#define BMI3X0_T_TAP_EN_MASK                           UINT16_C(0x4000)
#define BMI3X0_T_TAP_EN_POS                            UINT8_C(14)

#define BMI3X0_I3C_SYNC_FILTER_EN_MASK                 UINT16_C(0x0001)

#define BMI3X0_I3C_SYNC_EN_MASK                        UINT16_C(0x8000)
#define BMI3X0_I3C_SYNC_EN_POS                         UINT8_C(15)

/******************************************************************************/
/*!  Mask definitions for feature interrupts configuration */
/******************************************************************************/
#define BMI3X0_ANY_NO_SLOPE_THRESHOLD_MASK             UINT16_C(0x0FFF)

#define BMI3X0_ANY_NO_ACC_REF_UP_MASK                  UINT16_C(0x1000)
#define BMI3X0_ANY_NO_ACC_REF_UP_POS                   UINT8_C(12)

#define BMI3X0_ANY_NO_HYSTERESIS_MASK                  UINT16_C(0x03FF)

#define BMI3X0_ANY_NO_DURATION_MASK                    UINT16_C(0x1FFF)

#define BMI3X0_ANY_NO_WAIT_TIME_MASK                   UINT16_C(0xE000)
#define BMI3X0_ANY_NO_WAIT_TIME_POS                    UINT8_C(13)

/*!  Bit mask definitions for BMI3X0 flat feature configuration */
#define BMI3X0_FLAT_THETA_MASK                         UINT16_C(0x003F)
#define BMI3X0_FLAT_BLOCKING_MASK                      UINT16_C(0x00C0)
#define BMI3X0_FLAT_HOLD_TIME_MASK                     UINT16_C(0xFF00)
#define BMI3X0_FLAT_SLOPE_THRES_MASK                   UINT16_C(0x00FF)
#define BMI3X0_FLAT_HYST_MASK                          UINT16_C(0XFF00)

/*! Bit position definitions for BMI3X0 flat feature configuration */
#define BMI3X0_FLAT_BLOCKING_POS                       UINT8_C(0x06)
#define BMI3X0_FLAT_HOLD_TIME_POS                      UINT8_C(0x08)
#define BMI3X0_FLAT_HYST_POS                           UINT8_C(0x08)

/*!  Bit mask definitions for BMI3X0 significant motion feature configuration */
#define BMI3X0_SIG_BLOCK_SIZE_MASK                     UINT16_C(0xFFFF)

#define BMI3X0_SIG_P2P_MIN_MASK                        UINT16_C(0x03FF)

#define BMI3X0_SIG_MCR_MIN_MASK                        UINT16_C(0xFC00)
#define BMI3X0_SIG_MCR_MIN_POS                         UINT8_C(10)

#define BMI3X0_SIG_P2P_MAX_MASK                        UINT16_C(0x03FF)

#define BMI3X0_MCR_MAX_MASK                            UINT16_C(0xFC00)
#define BMI3X0_MCR_MAX_POS                             UINT8_C(10)

/*!  Bit mask definitions for BMI3X0 tilt feature configuration */
#define BMI3X0_TILT_SEGMENT_SIZE_MASK                  UINT16_C(0x00FF)

#define BMI3X0_TILT_MIN_TILT_ANGLE_MASK                UINT16_C(0xFF00)
#define BMI3X0_TILT_MIN_TILT_ANGLE_POS                 UINT8_C(8)

#define BMI3X0_TILT_BETA_ACC_MEAN_MASK                 UINT16_C(0xFFFF)

/*! @name Bit mask definitions for BMI3 orientation feature configuration */
#define BMI3X0_ORIENT_UD_EN_MASK                       UINT16_C(0x0001)
#define BMI3X0_ORIENT_MODE_MASK                        UINT16_C(0x0006)
#define BMI3X0_ORIENT_BLOCKING_MASK                    UINT16_C(0x0018)
#define BMI3X0_ORIENT_THETA_MASK                       UINT16_C(0x07E0)
#define BMI3X0_ORIENT_HOLD_TIME_MASK                   UINT16_C(0XF800)
#define BMI3X0_ORIENT_SLOPE_THRES_MASK                 UINT16_C(0X00FF)
#define BMI3X0_ORIENT_HYST_MASK                        UINT16_C(0XFF00)

/*! @name Bit position definitions for BMI3 orientation feature configuration */
#define BMI3X0_ORIENT_MODE_POS                         UINT8_C(1)
#define BMI3X0_ORIENT_BLOCKING_POS                     UINT8_C(3)
#define BMI3X0_ORIENT_THETA_POS                        UINT8_C(5)
#define BMI3X0_ORIENT_HOLD_TIME_POS                    UINT8_C(11)
#define BMI3X0_ORIENT_HYST_POS                         UINT8_C(8)

/*! @name Bit position definitions for BMI3 step counter feature configuration */
#define BMI3X0_STEP_WATERMARK_MASK                     UINT16_C(0x3FF)

#define BMI3X0_STEP_RESET_COUNTER_MASK                 UINT16_C(0x0400)
#define BMI3X0_STEP_RESET_COUNTER_POS                  UINT8_C(10)

#define BMI3X0_STEP_ENV_MIN_DIST_UP_MASK               UINT16_C(0xFFFF)

#define BMI3X0_STEP_ENV_COEF_UP_MASK                   UINT16_C(0xFFFF)

#define BMI3X0_STEP_ENV_MIN_DIST_DOWN_MASK             UINT16_C(0xFFFF)

#define BMI3X0_STEP_ENV_COEF_DOWN_MASK                 UINT16_C(0xFFFF)

#define BMI3X0_STEP_MEAN_VAL_DECAY_MASK                UINT16_C(0xFFFF)

#define BMI3X0_STEP_MEAN_STEP_DUR_MASK                 UINT16_C(0xFFFF)

#define BMI3X0_STEP_BUFFER_SIZE_MASK                   UINT16_C(0x000F)

#define BMI3X0_STEP_FILTER_CASCADE_ENABLED_MASK        UINT16_C(0x0010)
#define BMI3X0_STEP_FILTER_CASCADE_ENABLED_POS         UINT8_C(4)

#define BMI3X0_STEP_COUNTER_INCREMENT_MASK             UINT16_C(0xFFE0)
#define BMI3X0_STEP_COUNTER_INCREMENT_POS              UINT8_C(5)

#define BMI3X0_STEP_PEAK_DURATION_MIN_WALKING_MASK     UINT16_C(0x00FF)

#define BMI3X0_STEP_PEAK_DURATION_MIN_RUNNING_MASK     UINT16_C(0xFF00)
#define BMI3X0_STEP_PEAK_DURATION_MIN_RUNNING_POS      UINT8_C(8)

#define BMI3X0_STEP_ACTIVITY_DETECTION_FACTOR_MASK     UINT16_C(0x000F)

#define BMI3X0_STEP_ACTIVITY_DETECTION_THRESHOLD_MASK  UINT16_C(0xFFF0)
#define BMI3X0_STEP_ACTIVITY_DETECTION_THRESHOLD_POS   UINT8_C(4)

#define BMI3X0_STEP_DURATION_MAX_MASK                  UINT16_C(0x00FF)

#define BMI3X0_STEP_DURATION_WINDOW_MASK               UINT16_C(0xFF00)
#define BMI3X0_STEP_DURATION_WINDOW_POS                UINT8_C(8)

#define BMI3X0_STEP_DURATION_PP_ENABLED_MASK           UINT16_C(0x0001)

#define BMI3X0_STEP_DURATION_THRESHOLD_MASK            UINT16_C(0x000E)
#define BMI3X0_STEP_DURATION_THRESHOLD_POS             UINT8_C(1)

#define BMI3X0_STEP_MEAN_CROSSING_PP_ENABLED_MASK      UINT16_C(0x0010)
#define BMI3X0_STEP_MEAN_CROSSING_PP_ENABLED_POS       UINT8_C(4)

#define BMI3X0_STEP_MCR_THRESHOLD_MASK                 UINT16_C(0x03E0)
#define BMI3X0_STEP_MCR_THRESHOLD_POS                  UINT8_C(5)

#define BMI3X0_STEP_DEVICE_CONTEXT_MASK                UINT16_C(0x0C00)
#define BMI3X0_STEP_DEVICE_CONTEXT_POS                 UINT8_C(10)

/*! @name Bit mask definitions for BMI3 tap feature configuration */
#define BMI3X0_TAP_AXIS_SEL_MASK                       UINT16_C(0x0003)
#define BMI3X0_TAP_WAIT_FR_TIME_OUT_MASK               UINT16_C(0x0004)
#define BMI3X0_TAP_MAX_PEAKS_MASK                      UINT16_C(0x0038)
#define BMI3X0_TAP_MODE_MASK                           UINT16_C(0x00C0)
#define BMI3X0_TAP_PEAK_THRES_MASK                     UINT16_C(0X03FF)
#define BMI3X0_TAP_MAX_GEST_DUR_MASK                   UINT16_C(0XFC00)
#define BMI3X0_TAP_MAX_DUR_BW_PEAKS_MASK               UINT16_C(0X000F)
#define BMI3X0_TAP_SHOCK_SETT_DUR_MASK                 UINT16_C(0X00F0)
#define BMI3X0_TAP_MIN_QUITE_DUR_BW_TAPS_MASK          UINT16_C(0X0F00)
#define BMI3X0_TAP_QUITE_TIME_AFTR_GEST_MASK           UINT16_C(0XF000)

/*! @name Bit position definitions for BMI3 tap feature configuration */
#define BMI3X0_TAP_WAIT_FR_TIME_OUT_POS                UINT8_C(0x02)
#define BMI3X0_TAP_MAX_PEAKS_POS                       UINT8_C(0x03)
#define BMI3X0_TAP_MODE_POS                            UINT8_C(0x06)
#define BMI3X0_TAP_MAX_GEST_DUR_POS                    UINT8_C(0x0A)
#define BMI3X0_TAP_SHOCK_SETT_DUR_POS                  UINT8_C(0x04)
#define BMI3X0_TAP_MIN_QUITE_DUR_BW_TAPS_POS           UINT8_C(0x08)
#define BMI3X0_TAP_QUITE_TIME_AFTR_GEST_POS            UINT8_C(0x0C)

#define BMI3X0_TAP_DET_STATUS_SINGLE                   UINT16_C(0X0008)
#define BMI3X0_TAP_DET_STATUS_DOUBLE                   UINT16_C(0X0010)
#define BMI3X0_TAP_DET_STATUS_TRIPLE                   UINT16_C(0X0020)

/******************************************************************************/
/*! BMI3 sensor data bytes */
/******************************************************************************/
#define BMI3X0_ACC_NUM_BYTES                           UINT8_C(20)
#define BMI3X0_GYR_NUM_BYTES                           UINT8_C(14)
#define BMI3X0_CRT_CONFIG_FILE_SIZE                    UINT16_C(2048)
#define BMI3X0_FEAT_SIZE_IN_BYTES                      UINT8_C(16)
#define BMI3X0_ACC_CONFIG_LENGTH                       UINT8_C(2)
#define BMI3X0_NUM_BYTES_I3C_SYNC_ACC                  UINT8_C(16)
#define BMI3X0_NUM_BYTES_I3C_SYNC_GYR                  UINT8_C(10)
#define BMI3X0_NUM_BYTES_I3C_SYNC_TEMP                 UINT8_C(4)

/******************************************************************************/
/*!        Accelerometer Macro Definitions               */
/******************************************************************************/
/*!  Accelerometer Bandwidth parameters */
#define BMI3X0_ACC_AVG1                                UINT8_C(0x00)
#define BMI3X0_ACC_AVG2                                UINT8_C(0x01)
#define BMI3X0_ACC_AVG4                                UINT8_C(0x02)
#define BMI3X0_ACC_AVG8                                UINT8_C(0x03)
#define BMI3X0_ACC_AVG16                               UINT8_C(0x04)
#define BMI3X0_ACC_AVG32                               UINT8_C(0x05)
#define BMI3X0_ACC_AVG64                               UINT8_C(0x06)
#define BMI3X0_ACC_AVG128                              UINT8_C(0x07)

/*!  Accelerometer Output Data Rate */
#define BMI3X0_ACC_ODR_0_39HZ                          UINT8_C(0x00)
#define BMI3X0_ACC_ODR_0_78HZ                          UINT8_C(0x01)
#define BMI3X0_ACC_ODR_1_56HZ                          UINT8_C(0x02)
#define BMI3X0_ACC_ODR_3_125HZ                         UINT8_C(0x03)
#define BMI3X0_ACC_ODR_6_25HZ                          UINT8_C(0x04)
#define BMI3X0_ACC_ODR_12_5HZ                          UINT8_C(0x05)
#define BMI3X0_ACC_ODR_25HZ                            UINT8_C(0x06)
#define BMI3X0_ACC_ODR_50HZ                            UINT8_C(0x07)
#define BMI3X0_ACC_ODR_100HZ                           UINT8_C(0x08)
#define BMI3X0_ACC_ODR_200HZ                           UINT8_C(0x09)
#define BMI3X0_ACC_ODR_400HZ                           UINT8_C(0x0A)
#define BMI3X0_ACC_ODR_800HZ                           UINT8_C(0x0B)
#define BMI3X0_ACC_ODR_1600HZ                          UINT8_C(0x0C)
#define BMI3X0_ACC_ODR_3200HZ                          UINT8_C(0x0D)
#define BMI3X0_ACC_ODR_6400HZ                          UINT8_C(0x0E)
#define BMI3X0_ACC_ODR_12800HZ                         UINT8_C(0x0F)

/*!  Accelerometer G Range */
#define BMI3X0_ACC_RANGE_2G                            UINT8_C(0x00)
#define BMI3X0_ACC_RANGE_4G                            UINT8_C(0x01)
#define BMI3X0_ACC_RANGE_8G                            UINT8_C(0x02)
#define BMI3X0_ACC_RANGE_16G                           UINT8_C(0x03)
#define BMI3X0_ACC_RANGE_32G                           UINT8_C(0x04)

#define BMI3X0_ACC_MODE_DISABLE                        UINT8_C(0x00)
#define BMI3X0_ACC_MODE_ULTRA_LOW_PWR                  UINT8_C(0X02)
#define BMI3X0_ACC_MODE_LOW_PWR                        UINT8_C(0x03)
#define BMI3X0_ACC_MODE_NORMAL                         UINT8_C(0X04)
#define BMI3X0_ACC_MODE_HIGH_PERF                      UINT8_C(0x07)

#define BMI3X0_ACC_BW_ODR_HALF                         UINT8_C(0)
#define BMI3X0_ACC_BW_ODR_QUARTER                      UINT8_C(1)

/******************************************************************************/
/*!       Gyroscope Macro Definitions               */
/******************************************************************************/
/*!  Gyroscope Bandwidth parameters */
#define BMI3X0_GYR_AVG1                                UINT8_C(0x00)
#define BMI3X0_GYR_AVG2                                UINT8_C(0x01)
#define BMI3X0_GYR_AVG4                                UINT8_C(0x02)
#define BMI3X0_GYR_AVG8                                UINT8_C(0x03)
#define BMI3X0_GYR_AVG16                               UINT8_C(0x04)
#define BMI3X0_GYR_AVG32                               UINT8_C(0x05)
#define BMI3X0_GYR_AVG64                               UINT8_C(0x06)
#define BMI3X0_GYR_AVG128                              UINT8_C(0x07)

/*! Gyroscope Output Data Rate */
#define BMI3X0_GYR_ODR_0_39HZ                          UINT8_C(0x00)
#define BMI3X0_GYR_ODR_0_78HZ                          UINT8_C(0x01)
#define BMI3X0_GYR_ODR_1_56HZ                          UINT8_C(0x02)
#define BMI3X0_GYR_ODR_3_125HZ                         UINT8_C(0x03)
#define BMI3X0_GYR_ODR_6_25HZ                          UINT8_C(0x04)
#define BMI3X0_GYR_ODR_12_5HZ                          UINT8_C(0x05)
#define BMI3X0_GYR_ODR_25HZ                            UINT8_C(0x06)
#define BMI3X0_GYR_ODR_50HZ                            UINT8_C(0x07)
#define BMI3X0_GYR_ODR_100HZ                           UINT8_C(0x08)
#define BMI3X0_GYR_ODR_200HZ                           UINT8_C(0x09)
#define BMI3X0_GYR_ODR_400HZ                           UINT8_C(0x0A)
#define BMI3X0_GYR_ODR_800HZ                           UINT8_C(0x0B)
#define BMI3X0_GYR_ODR_1600HZ                          UINT8_C(0x0C)
#define BMI3X0_GYR_ODR_3200HZ                          UINT8_C(0x0D)
#define BMI3X0_GYR_ODR_6400HZ                          UINT8_C(0x0E)
#define BMI3X0_GYR_ODR_12K8HZ                          UINT8_C(0x0F)

/*! Gyroscope DPS Range */
#define BMI3X0_GYR_RANGE_125DPS                        UINT8_C(0x00)
#define BMI3X0_GYR_RANGE_250DPS                        UINT8_C(0x01)
#define BMI3X0_GYR_RANGE_500DPS                        UINT8_C(0x02)
#define BMI3X0_GYR_RANGE_1000DPS                       UINT8_C(0x03)
#define BMI3X0_GYR_RANGE_2000DPS                       UINT8_C(0x04)
#define BMI3X0_GYR_RANGE_4000DPS                       UINT8_C(0x05)
#define BMI3X0_GYR_RANGE_8000DPS                       UINT8_C(0x06)
#define BMI3X0_GYR_RANGE_16000DPS                      UINT8_C(0x07)

#define BMI3X0_GYR_MODE_DISABLE                        UINT8_C(0x00)
#define BMI3X0_GYR_MODE_SUSPEND                        UINT8_C(0X01)
#define BMI3X0_GYR_MODE_ULTRA_LOW_PWR                  UINT8_C(0X02)
#define BMI3X0_GYR_MODE_LOW_PWR                        UINT8_C(0x03)
#define BMI3X0_GYR_MODE_NORMAL                         UINT8_C(0X04)
#define BMI3X0_GYR_MODE_HIGH_PERF                      UINT8_C(0x07)

/*! Gyroscope bandwidth */
#define BMI3X0_GYR_BW_ODR_HALF                         UINT8_C(0)
#define BMI3X0_GYR_BW_ODR_QUARTER                      UINT8_C(1)

#define BMI3X0_SC_SENSITIVITY_EN                       UINT8_C(1)
#define BMI3X0_SC_OFFSET_EN                            UINT8_C(2)

/*! Self-calibration enable disable macros */
#define BMI3X0_SC_APPLY_CORR_DIS                       UINT8_C(0)
#define BMI3X0_SC_APPLY_CORR_EN                        UINT8_C(4)

/******************************************************************************/
/*!       I3C Macro Definitions               */
/******************************************************************************/

/*! I3C sync ODR */
#define BMI3XO_I3C_SYNC_ODR_6_25HZ                     UINT8_C(0x04)
#define BMI3XO_I3C_SYNC_ODR_12_5HZ                     UINT8_C(0x05)
#define BMI3XO_I3C_SYNC_ODR_25HZ                       UINT8_C(0x06)
#define BMI3XO_I3C_SYNC_ODR_50HZ                       UINT8_C(0x07)
#define BMI3XO_I3C_SYNC_ODR_100HZ                      UINT8_C(0x08)
#define BMI3XO_I3C_SYNC_ODR_200HZ                      UINT8_C(0x09)
#define BMI3XO_I3C_SYNC_ODR_400HZ                      UINT8_C(0x0A)
#define BMI3XO_I3C_SYNC_ODR_800HZ                      UINT8_C(0x0B)

/*! I3C sync division factor */
#define BMI3XO_I3C_SYNC_DIVISION_FACTOR_11             UINT8_C(0)
#define BMI3XO_I3C_SYNC_DIVISION_FACTOR_12             UINT8_C(1)
#define BMI3XO_I3C_SYNC_DIVISION_FACTOR_13             UINT8_C(2)
#define BMI3XO_I3C_SYNC_DIVISION_FACTOR_14             UINT8_C(3)

/******************************************************************************/
/*!       Feature interrupts base address definitions               */
/******************************************************************************/

#define BMI3X0_BASE_ADDR_CONFIG_VERSION                UINT8_C(0x00)
#define BMI3X0_BASE_ADDR_AXIS_REMAP                    UINT8_C(0x03)
#define BMI3X0_BASE_ADDR_ANY_MOTION                    UINT8_C(0x05)
#define BMI3X0_BASE_ADDR_NO_MOTION                     UINT8_C(0x08)
#define BMI3X0_BASE_ADDR_FLAT                          UINT8_C(0x0B)
#define BMI3X0_BASE_ADDR_SIG_MOTION                    UINT8_C(0x0D)
#define BMI3X0_BASE_ADDR_STEP_CNT                      UINT8_C(0x10)
#define BMI3X0_BASE_ADDR_ORIENT                        UINT8_C(0x1C)
#define BMI3X0_BASE_ADDR_TAP                           UINT8_C(0x1E)
#define BMI3X0_BASE_ADDR_TILT                          UINT8_C(0x21)
#define BMI3X0_BASE_ADDR_ALT_AUTO_CONFIG               UINT8_C(0X23)
#define BMI3X0_BASE_ADDR_SC_ST                         UINT8_C(0x24)
#define BMI3X0_BASE_ADDR_ST_SELECT                     UINT8_C(0x25)
#define BMI3X0_BASE_ADDR_SC                            UINT8_C(0x26)
#define BMI3X0_BASE_ADDR_GYRO_SC_ST_COEFFICIENTS       UINT8_C(0x28)
#define BMI3X0_BASE_ADDR_I3C_SYNC                      UINT8_C(0x36)
#define BMI3X0_BASE_ADDR_I3C_SYNC_ACC                  UINT8_C(0x37)
#define BMI3X0_BASE_ADDR_I3C_SYNC_GYR                  UINT8_C(0x3A)
#define BMI3X0_BASE_ADDR_I3C_SYNC_TEMP                 UINT8_C(0x3D)
#define BMI3X0_BASE_ADDR_TEMP_ACC_OFFSET_GAIN          UINT8_C(0x40)
#define BMI3X0_BASE_ADDR_TEMP_GYRO_OFFSET_GAIN         UINT8_C(0x46)

/******************************************************************************/
/*! @name BMI3X0 Interrupt Modes */
/******************************************************************************/
/* Non latched */
#define BMI3X0_INT_NON_LATCH                           UINT8_C(0)

/*!  Mask definitions for interrupt pin configuration */
#define BMI3X0_INT_LATCH_MASK                          UINT16_C(0x0001)

#define BMI3X0_INT_LATCH_EN                            UINT8_C(1)
#define BMI3X0_INT_LATCH_DISABLE                       UINT8_C(0)

/*! @name BMI3X0 Interrupt Pin Behavior */
#define BMI3X0_INT_PUSH_PULL                           UINT8_C(0)
#define BMI3X0_INT_OPEN_DRAIN                          UINT8_C(1)

/*! @name BMI3X0 Interrupt Pin Level */
#define BMI3X0_INT_ACTIVE_LOW                          UINT8_C(0)
#define BMI3X0_INT_ACTIVE_HIGH                         UINT8_C(1)

/*! @name BMI3X0 Interrupt Output Enable */
#define BMI3X0_INT_OUTPUT_DISABLE                      UINT8_C(0)
#define BMI3X0_INT_OUTPUT_ENABLE                       UINT8_C(1)

/*! @name BMI3X0 Interrupt Input Enable */
#define BMI3X0_INT_INPUT_DISABLE                       UINT8_C(0)
#define BMI3X0_INT_INPUT_ENABLE                        UINT8_C(1)

/**\name Orientation output macros */
#define BMI3X0_FACE_UP                                 UINT8_C(0x00)
#define BMI3X0_FACE_DOWN                               UINT8_C(0x01)

#define BMI3X0_PORTRAIT_UP_RIGHT                       UINT8_C(0x00)
#define BMI3X0_LANDSCAPE_LEFT                          UINT8_C(0x01)
#define BMI3X0_PORTRAIT_UP_DOWN                        UINT8_C(0x02)
#define BMI3X0_LANDSCAPE_RIGHT                         UINT8_C(0x03)

/******************************************************************************/
/*! @name       FIFO Macro Definitions                                        */
/******************************************************************************/

/*! Mask definitions for data interrupt mapping */
#define BMI3X0_INT_FFULL                               UINT16_C(0xC000)
#define BMI3X0_INT_FWM                                 UINT16_C(0x3000)
#define BMI3X0_INT_ACC_DRDY                            UINT16_C(0x0C00)
#define BMI3X0_INT_GYR_DRDY                            UINT16_C(0x0300)
#define BMI3X0_INT_TEMP_DRDY                           UINT16_C(0x00C0)
#define BMI3X0_INT_ERR_STATUS                          UINT16_C(0x0030)
#define BMI3X0_INT_I3C                                 UINT16_C(0x000C)

/*! Mask definitions for FIFO frame content configuration */
#define BMI3X0_FIFO_STOP_ON_FULL                       UINT16_C(0x0001)
#define BMI3X0_FIFO_TIME_EN                            UINT16_C(0x0100)
#define BMI3X0_FIFO_ACC_EN                             UINT16_C(0x0200)
#define BMI3X0_FIFO_GYR_EN                             UINT16_C(0x0400)
#define BMI3X0_FIFO_TEMP_EN                            UINT16_C(0x0800)
#define BMI3X0_FIFO_ALL_EN                             UINT16_C(0x0F00)

/*! FIFO sensor data lengths */
#define BMI3X0_LENGTH_FIFO_ACC                         UINT8_C(6)
#define BMI3X0_LENGTH_FIFO_GYR                         UINT8_C(6)
#define BMI3X0_LENGTH_TEMPERATURE                      UINT8_C(2)
#define BMI3X0_LENGTH_SENSOR_TIME                      UINT8_C(2)
#define BMI3X0_LENGTH_FIFO_CONFIG                      UINT8_C(2)
#define BMI3X0_LENGTH_FIFO_WM                          UINT8_C(2)
#define BMI3X0_LENGTH_MAX_FIFO_FILTER                  UINT8_C(1)
#define BMI3X0_LENGTH_FIFO_DATA                        UINT8_C(2)
#define BMI3X0_LENGTH_FIFO_MSB_BYTE                    UINT8_C(1)

/*! FIFO frame masks */
#define BMI3X0_FIFO_LSB_CONFIG_CHECK                   UINT16_C(0x0000)
#define BMI3X0_FIFO_MSB_CONFIG_CHECK                   UINT16_C(0x8000)

/*! BMI3X0 Mask definitions of FIFO configuration registers */
#define BMI3X0_FIFO_CONFIG_MASK                        UINT16_C(0x0F01)

/*! BMI3X0 sensor selection for header-less frames  */
#define BMI3X0_FIFO_HEAD_LESS_ACC_FRM                  UINT16_C(0x0200)
#define BMI3X0_FIFO_HEAD_LESS_GYR_FRM                  UINT16_C(0x0400)
#define BMI3X0_FIFO_HEAD_LESS_SENS_TIME_FRM            UINT16_C(0x0100)
#define BMI3X0_FIFO_HEAD_LESS_TEMP_FRM                 UINT16_C(0x0800)
#define BMI3X0_FIFO_HEAD_LESS_ALL_FRM                  UINT16_C(0x0F00)

/******************************************************************************/
/*! @name       Extended mode Macro Definitions                                        */
/******************************************************************************/

/*! Macros to select target page */
#define BMI3X0_TARGET_USER_PAGE                        UINT8_C(0x00)
#define BMI3X0_TARGET_HIF_PAGE                         UINT8_C(0x01)
#define BMI3X0_TARGET_SHDW_PAGE                        UINT8_C(0x02)
#define BMI3X0_TARGET_HWINT_PAGE                       UINT8_C(0x03)
#define BMI3X0_TARGET_FIFO_PAGE                        UINT8_C(0x04)
#define BMI3X0_TARGET_FCU_PAGE                         UINT8_C(0x05)
#define BMI3X0_TARGET_PMU_PAGE                         UINT8_C(0x06)
#define BMI3X0_TARGET_FEATURE_ENGINE_PAGE              UINT8_C(0x07)
#define BMI3X0_TARGET_ANA_PAGE                         UINT8_C(0x08)
#define BMI3X0_TARGET_TEMP_PAGE                        UINT8_C(0x09)
#define BMI3X0_TARGET_ACC_AFE_PAGE                     UINT8_C(0x0A)
#define BMI3X0_TARGET_ACC_DP_PAGE                      UINT8_C(0x0B)
#define BMI3X0_TARGET_GYR_AFE_PAGE                     UINT8_C(0x0C)
#define BMI3X0_TARGET_GYR_DP_PAGE                      UINT8_C(0x0D)
#define BMI3X0_TARGET_GYR_DRV_PAGE                     UINT8_C(0x0E)
#define BMI3X0_TARGET_CDB_PAGE                         UINT8_C(0x0F)
#define BMI3X0_TARGET_IO_PAGE                          UINT8_C(0x10)
#define BMI3X0_TARGET_OTPCTRL_PAGE                     UINT8_C(0x11)
#define BMI3X0_TARGET_BOOT_PAGE                        UINT8_C(0x12)
#define BMI3X0_TARGET_SUP_PAGE                         UINT8_C(0x13)
#define BMI3X0_TARGET_TST_PAGE                         UINT8_C(0x14)

/*! Macros to select privilege */
#define BMI3X0_USER_MODE_MASK                          UINT8_C(0x00)
#define BMI3X0_EXTENDED_MODE_MASK                      UINT8_C(0x80)
#define BMI3X0_SUPER_MODE_MASK                         UINT8_C(0xC0)

/******************************************************************************/
/*! @name       Alternate configuration macros                                */
/******************************************************************************/

/*! Enables switching possibility to alternate configuration for accel */
#define BMI3X0_ALT_ACC_ENABLE                          UINT8_C(0x01)

/*! Enables switching possibility to alternate configuration for gyro */
#define BMI3X0_ALT_GYR_ENABLE                          UINT8_C(0x10)

#define BMI3X0_ALT_CONF_ALT_SWITCH_MASK                UINT8_C(0x0F)

#define BMI3X0_ALT_CONF_USER_SWITCH_MASK               UINT8_C(0xF0)
#define BMI3X0_ALT_CONF_USER_SWITCH_POS                UINT8_C(4)

#define BMI3X0_ALT_CONF_RESET_ON                       UINT8_C(1)
#define BMI3X0_ALT_CONF_RESET_OFF                      UINT8_C(0)

#define BMI3X0_ALT_NO_MOTION                           UINT8_C(1)
#define BMI3X0_ALT_ANY_MOTION                          UINT8_C(2)
#define BMI3X0_ALT_FLAT                                UINT8_C(3)
#define BMI3X0_ALT_ORIENT                              UINT8_C(4)
#define BMI3X0_ALT_STEP_DETECTOR                       UINT8_C(5)
#define BMI3X0_ALT_STEP_COUNTER                        UINT8_C(6)
#define BMI3X0_ALT_SIG_MOTION                          UINT8_C(7)
#define BMI3X0_ALT_TILT                                UINT8_C(8)
#define BMI3X0_ALT_TAP                                 UINT8_C(9)

#define BMI3X0_ALT_ACCEL_STATUS_MASK                   UINT8_C(0x01)

#define BMI3X0_ALT_GYRO_STATUS_MASK                    UINT8_C(0x10)
#define BMI3X0_ALT_GYRO_STATUS_POS                     UINT8_C(4)

/******************************************************************************/
/*! @name       Status macros                                                 */
/******************************************************************************/
#define BMI3X0_STATUS_POR                              UINT8_C(0x01)
#define BMI3X0_STATUS_DRDY_TEMP                        UINT8_C(0x20)
#define BMI3X0_STATUS_DRDY_GYR                         UINT8_C(0x40)
#define BMI3X0_STATUS_DRDY_ACC                         UINT8_C(0x80)

/******************************************************************************/
/*! @name       FOC macros                                                    */
/******************************************************************************/

/*! Macro to define the accel FOC range */
#define BMI3X0_ACC_FOC_2G_REF                          UINT16_C(16384)
#define BMI3X0_ACC_FOC_4G_REF                          UINT16_C(8192)
#define BMI3X0_ACC_FOC_8G_REF                          UINT16_C(4096)
#define BMI3X0_ACC_FOC_16G_REF                         UINT16_C(2048)

#define BMI3X0_FOC_SAMPLE_LIMIT                        UINT8_C(128)

#define BMI3X0_MAX_NOISE_LIMIT(RANGE_VALUE)            (RANGE_VALUE + UINT16_C(2050))
#define BMI3X0_MIN_NOISE_LIMIT(RANGE_VALUE)            (RANGE_VALUE - UINT16_C(2050))

/*! Macro to define accelerometer configuration value for FOC */
#define BMI3X0_FOC_ACC_CONF_VAL_LSB                    UINT8_C(0xB7)
#define BMI3X0_FOC_ACC_CONF_VAL_MSB                    UINT8_C(0x40)

/*! Macro to define X Y and Z axis for an array */
#define BMI3X0_X_AXIS                                  UINT8_C(0)
#define BMI3X0_Y_AXIS                                  UINT8_C(1)
#define BMI3X0_Z_AXIS                                  UINT8_C(2)

#define BMI3X0_FOC_INVERT_VALUE                        INT8_C(-1)

/*! For defining absolute values */
#define BMI3X0_ABS(a)                                  ((a) > 0 ? (a) : -(a))

/******************************************************************************/
/*! @name       Gyro self-calibration/self-test coefficient macros  */
/******************************************************************************/
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_0                UINT16_C(0x5A2E)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_1                UINT16_C(0x9219)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_2                UINT16_C(0x5637)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_3                UINT16_C(0xFFE8)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_4                UINT16_C(0xFFEF)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_5                UINT16_C(0x000D)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_6                UINT16_C(0x07CA)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_7                UINT16_C(0xFFCD)
#define BMI3X0_GYRO_SC_ST_COEFFICIENT_8                UINT16_C(0xEF6C)

/********************************************************* */
/*!               Function Pointers                       */
/********************************************************* */

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific read functions of the user
 *
 * @param[in]     reg_addr : 8bit register address of the sensor
 * @param[out]    reg_data : Data from the specified address
 * @param[in]     length   : Length of the reg_data array
 * @param[in,out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                           for interface related callbacks
 * @retval 0 for Success
 * @retval Non-zero for Failure
 */
typedef BMI3X0_INTF_RET_TYPE (*bmi3x0_read_fptr_t)(uint8_t reg_addr, uint8_t *reg_data, uint32_t length,
                                                   void *intf_ptr);

/*!
 * @brief Bus communication function pointer which should be mapped to
 * the platform specific write functions of the user
 *
 * @param[in]     reg_addr : 8bit register address of the sensor
 * @param[out]    reg_data : Data to the specified address
 * @param[in]     length   : Length of the reg_data array
 * @param[in,out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                           for interface related callbacks
 * @retval 0 for Success
 * @retval Non-zero for Failure
 *
 */
typedef BMI3X0_INTF_RET_TYPE (*bmi3x0_write_fptr_t)(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
                                                    void *intf_ptr);

/*!
 * @brief Delay function pointer which should be mapped to
 * delay function of the user
 *
 * @param period - The time period in microseconds
 * @param[in,out] intf_ptr : Void pointer that can enable the linking of descriptors
 *                           for interface related callbacks
 */
typedef void (*bmi3x0_delay_us_fptr_t)(uint32_t period, void *intf_ptr);

/********************************************************* */
/*!                  Enumerators                          */
/********************************************************* */

/*!
 * Enumerator describing interfaces
 */
enum bmi3x0_intf {
    /*! SPI interface */
    BMI3X0_SPI_INTF = 0,
    /*! I2C interface */
    BMI3X0_I2C_INTF,
    /*! I3C interface */
    BMI3X0_I3C_INTF
};

/*!  @name Enum to define BMI3X0 sensor configuration errors for accelerometer
 *   and gyroscope
 */
enum bmi3x0_sensor_config_error {
    BMI3X0_NO_ERROR,
    BMI3X0_ACC_ERROR,
    BMI3X0_GYR_ERROR,
    BMI3X0_ACC_GYR_ERROR
};

/*!  @name Enum to define interrupt lines */
enum bmi3x0_hw_int_pin {
    BMI3X0_INT_NONE,
    BMI3X0_INT1,
    BMI3X0_INT2,
    BMI3X0_I3C_INT,
    BMI3X0_INT_PIN_MAX
};

/*!  @name Enum to define context switch selection values */
enum bmi3x0_context_sel {
    BMI3X0_SMART_PHONE_SEL,
    BMI3X0_WEARABLE_SEL,
    BMI3X0_HEARABLE_SEL,
    BMI3X0_SEL_MAX
};

/********************************************************* */
/*!                      Structures                       */
/********************************************************* */

/*! @name Structure to store the local copy of the re-mapped axis and
 * the value of its sign for register settings
 */
struct bmi3x0_axes_remap
{
    /*! Re-map x, y and z axis */
    uint8_t axis_map;

    /*! Re-mapped x-axis sign */
    uint8_t invert_x;

    /*! Re-mapped y-axis sign */
    uint8_t invert_y;

    /*! Re-mapped z-axis sign */
    uint8_t invert_z;
};

/*! @name Structure to define the type of sensor and its interrupt pin */
struct bmi3x0_sens_int_config
{
    /*! Defines the type of sensor */
    uint8_t type;

    /*! Type of interrupt pin */
    enum bmi3x0_hw_int_pin hw_int_pin;
};

/*! Structure to define the output configuration value of features */
struct bmi3x0_int_map
{
    /*! Output configuration value of sig-motion */
    uint8_t sig_mot_out_conf;

    /*! Output configuration value of any-motion */
    uint8_t any_mot_out_conf;

    /*! Output configuration value of no-motion */
    uint8_t no_mot_out_conf;

    /*! Output configuration value of step-detector */
    uint8_t step_det_out_conf;

    /*! Output configuration value of step-counter */
    uint8_t step_counter_out_conf;

    /*! Output configuration value of tilt */
    uint8_t tilt_out_conf;

    /*! Output configuration value of orientation */
    uint8_t orient_out_conf;

    /*! Output configuration value of flat */
    uint8_t flat_out_conf;

    /*! Output configuration value of wake-up */
    uint8_t wake_up_out_conf;
};

/*! Structure to define FIFO frame configuration */
struct bmi3x0_fifo_frame
{
    /*! Pointer to FIFO data */
    uint8_t *data;

    /*! Number of user defined bytes of FIFO to be read */
    uint16_t length;

    /*! Enables type of data to be streamed - accelerometer,
     *  gyroscope
     */
    uint16_t available_fifo_sens;

    /*! Water-mark level for water-mark interrupt */
    uint16_t wm_lvl;

    /*! Available fifo length */
    uint16_t available_fifo_len;

    /*! To store available fifo accel frames */
    uint16_t avail_fifo_accel_frames;

    /*! Available fifo sensor time frames */
    uint8_t avail_fifo_sens_time_frames;

    /*! To store available fifo gyro frames */
    uint16_t avail_fifo_gyro_frames;

    /*! To store available fifo temperature frames */
    uint16_t avail_fifo_temp_frames;
};

/*!
 * Primary device structure
 */
struct bmi3x0_dev
{
    /*! Chip id of BMI3X0 */
    uint16_t chip_id;

    /*!
     * The interface pointer is used to enable the user
     * to link their interface descriptors for reference during the
     * implementation of the read and write interfaces to the
     * hardware.
     */
    void *intf_ptr;

    /*!
     * To store information during boundary check conditions
     * If minimum value is stored, update info with BMI3X0_I_MIN_VALUE
     * If maximum value is stored, update info with BMI3X0_I_MAX_VALUE
     */
    uint8_t info;

    /*! Type of Interface  */
    enum bmi3x0_intf intf;

    /*! To store interface pointer error */
    BMI3X0_INTF_RET_TYPE intf_rslt;

    /*! For switching from I2C to SPI */
    uint8_t dummy_byte;

    /*! Resolution for FOC */
    uint8_t resolution;

    /*! User set read/write length */
    uint16_t read_write_len;

    /*! Read function pointer */
    bmi3x0_read_fptr_t read;

    /*! Write function pointer */
    bmi3x0_write_fptr_t write;

    /*!  Delay function pointer */
    bmi3x0_delay_us_fptr_t delay_us;

    /*! Array of feature input configuration structure */
    const struct bmi3x0_feature_config *feat_in_config;

    /*! Array of feature output configuration structure */
    const struct bmi3x0_feature_config *feat_out_config;
};

/*! @name Structure to define Interrupt pin configuration */
struct bmi3x0_int_pin_cfg
{
    /*! Configure level of interrupt pin */
    uint8_t lvl;

    /*! Configure behavior of interrupt pin */
    uint8_t od;

    /*! Output enable for interrupt pin */
    uint8_t output_en;

    /*! Input enable for interrupt pin */
    uint8_t input_en;
};

/*! @name Structure to define interrupt pin type, mode and configurations */
struct bmi3x0_int_pin_config
{
    /*! Interrupt pin type: INT1 or INT2 or BOTH */
    uint8_t pin_type;

    /*! Latched or non-latched mode */
    uint8_t int_latch;

    /*! Structure to define Interrupt pin configuration */
    struct bmi3x0_int_pin_cfg pin_cfg[BMI3X0_INT_PIN_MAX_NUM];
};

/*! @name Structure to define accelerometer and gyroscope sensor axes and
 * sensor time for virtual frames
 */
struct bmi3x0_sens_axes_data
{
    /*! Data in x-axis */
    int16_t x;

    /*! Data in y-axis */
    int16_t y;

    /*! Data in z-axis */
    int16_t z;

    /*! Sensor time for frames */
    uint32_t sens_time;

    /*! Saturation flag status for X axis */
    uint8_t sat_x : 1;

    /*! Saturation flag status for Y axis */
    uint8_t sat_y : 1;

    /*! Saturation flag status for Z axis */
    uint8_t sat_z : 1;
};

/*! @name Structure to define accelerometer and gyroscope sensor axes and
 * sensor time for virtual frames
 */
struct bmi3x0_i3c_sync_data
{
    /*! Data in x-axis */
    uint16_t sync_x;

    /*! Data in y-axis */
    uint16_t sync_y;

    /*! Data in z-axis */
    uint16_t sync_z;

    /*! Temperature data */
    uint16_t sync_temp;

    /*! Sensor time for frames */
    uint16_t sync_time;
};

/*! @name Structure to define FIFO accel, gyro x, y and z axis and
 *  sensor time
 */
struct bmi3x0_fifo_sens_axes_data
{
    /*! Data in x-axis */
    int16_t x;

    /*! Data in y-axis */
    int16_t y;

    /*! Data in z-axis */
    int16_t z;

    /*! Sensor time data */
    uint16_t sensor_time;

};

/*! @name Structure to define FIFO temperature and sensor time
 */
struct bmi3x0_fifo_temperature_data
{
    /*! Temperature data */
    uint16_t temp_data;

    /*! Sensor time data */
    uint16_t sensor_time;
};

/*! @name Structure to define orientation output */
struct bmi3x0_orientation_output
{
    /*! Orientation portrait landscape */
    uint8_t portrait_landscape;

    /*! Orientation face-up down  */
    uint8_t faceup_down;
};

/*! @name Structure to define gyroscope saturation status of user gain */
struct bmi3x0_gyr_user_gain_status
{
    /*! Status in x-axis */
    uint8_t sat_x;

    /*! Status in y-axis */
    uint8_t sat_y;

    /*! Status in z-axis */
    uint8_t sat_z;

    /*! G trigger status */
    uint8_t g_trigger_status;
};

/*! @name Structure to define accelerometer and gyroscope self test feature status */
struct bmi3x0_acc_gyr_self_test_status
{
    /*! Self test completed */
    uint8_t self_test_rslt;

    /*! Bit is set to 1 when accelerometer X-axis test passed */
    uint8_t acc_x_ok;

    /*! Bit is set to 1 when accelerometer y-axis test passed */
    uint8_t acc_y_ok;

    /*! Bit is set to 1 when accelerometer z-axis test passed */
    uint8_t acc_z_ok;

    /*! Bit is set to 1 when gyroscope X-axis test passed */
    uint8_t gyr_x_ok;

    /*! Bit is set to 1 when gyroscope y-axis test passed */
    uint8_t gyr_y_ok;

    /*! Bit is set to 1 when gyroscope z-axis test passed */
    uint8_t gyr_z_ok;

    /*! Bit is set to 1 when gyroscope drive test passed */
    uint8_t gyr_drive_ok;

    /*! Stores the self-test error code result */
    uint8_t self_test_err_rslt;
};

/*! @name Union to define BMI3X0 sensor data */
union bmi3x0_sens_data
{
    /*! Accelerometer axes data */
    struct bmi3x0_sens_axes_data acc;

    /*! Gyroscope axes data */
    struct bmi3x0_sens_axes_data gyr;

    /*! Step counter output */
    uint32_t step_counter_output;

    /*! Orientation output */
    struct bmi3x0_orientation_output orient_output;

    /*! Gyroscope user gain saturation status */
    struct bmi3x0_gyr_user_gain_status gyro_user_gain_status;

    /*! I3C sync data */
    struct bmi3x0_i3c_sync_data i3c_sync;
};

/*! @name Structure to define type of sensor and their respective data */
struct bmi3x0_sensor_data
{
    /*! Defines the type of sensor */
    uint8_t type;

    /*! Defines various sensor data */
    union bmi3x0_sens_data sens_data;
};

/*! @name Structure to define accelerometer configuration */
struct bmi3x0_accel_config
{
    /*!  ODR in Hz */
    uint8_t odr;

    /*! Bandwidth parameter */
    uint8_t bwp;

    /*! Filter accel mode */
    uint8_t acc_mode;

    /*! g-range */
    uint8_t range;

    /*! Defines the number of samples to be averaged */
    uint8_t avg_num;
};

/*! @name Structure to define gyroscope configuration */
struct bmi3x0_gyro_config
{
    /*! Output data rate in Hz */
    uint8_t odr;

    /*! Bandwidth parameter */
    uint8_t bwp;

    /*! Filter gyro mode */
    uint8_t gyr_mode;

    /*! Gyroscope Range */
    uint8_t range;

    /*! Defines the number of samples to be averaged */
    uint8_t avg_num;
};

/*! @name Structure to define any-motion configuration */
struct bmi3x0_any_motion_config
{
    /*! Duration in 50Hz samples(20msec) */
    uint16_t duration;

    /*! Acceleration slope threshold */
    uint16_t threshold;

    /*! Mode of accel reference update */
    uint8_t acc_ref_up;

    /*! Hysteresis for the slope of the acceleration signal */
    uint16_t hysteresis;

    /*! Wait time for clearing the event after slope is below threshold */
    uint16_t wait_time;
};

/*! @name Structure to define no-motion configuration */
struct bmi3x0_no_motion_config
{
    /*! Duration in 50Hz samples(20msec) */
    uint16_t duration;

    /*! Acceleration slope threshold */
    uint16_t threshold;

    /*! Mode of accel reference update */
    uint8_t acc_ref_up;

    /*! Hysteresis for the slope of the acceleration signal */
    uint16_t hysteresis;

    /*! Wait time for clearing the event after slope is below threshold */
    uint16_t wait_time;
};

/*! @name Structure to define sig-motion configuration */
struct bmi3x0_sig_motion_config
{
    /*! Block size */
    uint16_t block_size;

    /*! Minimum value of the peak to peak acceleration magnitude */
    uint16_t peak_2_peak_min;

    /*! Minimum number of mean crossing per second in acceleration magnitude */
    uint8_t mcr_min;

    /*! Maximum value of the peak to peak acceleration magnitude */
    uint16_t peak_2_peak_max;

    /*! MAximum number of mean crossing per second in acceleration magnitude */
    uint8_t mcr_max;
};

/*! @name Structure to define step counter/detector/activity configuration */
struct bmi3x0_step_config
{
    /*! Water-mark level */
    uint16_t watermark_level;

    /*! Reset counter */
    uint16_t reset_counter;

    /*! Step Counter param 1 */
    uint16_t env_min_dist_up;

    /*! Step Counter param 2 */
    uint16_t env_coef_up;

    /*! Step Counter param 3 */
    uint16_t env_min_dist_down;

    /*! Step Counter param 4 */
    uint16_t env_coef_down;

    /*! Step Counter param 5 */
    uint16_t mean_val_decay;

    /*! Step Counter param 6 */
    uint16_t mean_step_dur;

    /*! Step Counter param 7 */
    uint16_t step_buffer_size;

    /*! Step Counter param 8 */
    uint16_t filter_cascade_enabled;

    /*! Step Counter param 9 */
    uint16_t step_counter_increment;

    /*! Step Counter param 10 */
    uint16_t peak_duration_min_walking;

    /*! Step Counter param 11 */
    uint16_t peak_duration_min_running;

    /*! Step Counter param 12 */
    uint16_t activity_detection_factor;

    /*! Step Counter param 13 */
    uint16_t activity_detection_thres;

    /*! Step Counter param 14 */
    uint16_t step_duration_max;

    /*! Step Counter param 15 */
    uint16_t step_duration_window;

    /*! Step Counter param 16 */
    uint16_t step_duration_pp_enabled;

    /*! Step Counter param 17 */
    uint16_t step_duration_thres;

    /*! Step Counter param 18 */
    uint16_t mean_crossing_pp_enabled;

    /*! Step Counter param 19 */
    uint16_t mcr_threshold;

    /*! Step Counter param 20 */
    uint16_t device_context;
};

/*! @name Structure to define gyroscope user gain configuration */
struct bmi3x0_gyro_user_gain_config
{
    /*! Gain update value for x-axis */
    uint16_t ratio_x;

    /*! Gain update value for y-axis */
    uint16_t ratio_y;

    /*! Gain update value for z-axis */
    uint16_t ratio_z;
};

/*! @name Structure to define tilt configuration */
struct bmi3x0_tilt_config
{
    /*! Duration for which the acceleration vector is averaged to be reference vector */
    uint16_t segment_size;

    /*! Minimum tilt angle */
    uint16_t min_tilt_angle;

    /*! Mean of acceleration vector */
    uint16_t beta_acc_mean;
};

/*! @name Structure to define orientation configuration */
struct bmi3x0_orient_config
{
    /*! Upside/down detection */
    uint8_t ud_en;

    /*! Symmetrical, high or low Symmetrical */
    uint8_t mode;

    /*! Blocking mode */
    uint8_t blocking;

    /*! Threshold angle */
    uint8_t theta;

    /*! Hold time of device */
    uint8_t hold_time;

    /*! Acceleration hysteresis for orientation detection */
    uint8_t hysteresis;

    /*! Slope threshold */
    uint8_t slope_thres;
};

/*! @name Structure to define flat configuration */
struct bmi3x0_flat_config
{
    /*! Theta angle for flat detection */
    uint16_t theta;

    /*! Blocking mode */
    uint16_t blocking;

    /*! Hysteresis for theta flat detection */
    uint16_t hysteresis;

    /*! Holds the duration in 50Hz samples(20msec) */
    uint16_t hold_time;

    /*! Minimum slope between consecutive acceleration samples to pervent the
     * change of flat status during large movement */
    uint16_t slope_thres;
};

/*! @name Structure to define wake-up configuration */
struct bmi3x0_wake_up_config
{
    /*! Axis selection */
    uint8_t axis_sel;

    /*! Wait time */
    uint8_t wait_for_timeout;

    /*! Maximum number of zero crossing expected around a tap */
    uint8_t max_peaks_for_tap;

    /*! Mode for detection of tap gesture */
    uint8_t mode;

    /*! Minimum threshold for peak resulting from the tap */
    uint16_t tap_peak_thres;

    /*! Maximum duration between each taps */
    uint8_t max_gest_dur;

    /*! Maximum duration between positive and negative peaks to tap */
    uint8_t max_dur_between_peaks;

    /*! Maximum duration for which tap impact is observed */
    uint8_t tap_shock_settling_dur;

    /*! Mimimum duration between two tap impact */
    uint8_t min_quite_dur_between_taps;

    /*! Minimum quite time between the two gesture detection */
    uint8_t quite_time_after_gest;
};

/*! @name Structure to define alternate accel configuration */
struct bmi3x0_alt_accel_config
{
    /*! ODR in Hz */
    uint8_t alt_acc_odr;

    /*! Filter accel mode */
    uint8_t alt_acc_mode;

    /*! Defines the number of samples to be averaged */
    uint8_t alt_acc_avg_num;
};

/*! @name Structure to define alternate gyro configuration */
struct bmi3x0_alt_gyro_config
{
    /*! ODR in Hz */
    uint8_t alt_gyro_odr;

    /*! Filter gyro mode */
    uint8_t alt_gyro_mode;

    /*! Defines the number of samples to be averaged */
    uint8_t alt_gyro_avg_num;
};

/*! @name Structure to define alternate auto configuration */
struct bmi3x0_alt_auto_config
{
    /*! Mode to set features on alternate configurations */
    uint8_t alt_switch_src_select;

    /*! Mode to switch from alternate configurations to user configurations */
    uint8_t user_switch_src_select;
};

/*!  @name Union to define the sensor configurations */
union bmi3x0_sens_config_types
{
    /*! Accelerometer configuration */
    struct bmi3x0_accel_config acc;

    /*! Gyroscope configuration */
    struct bmi3x0_gyro_config gyr;

    /*! Any-motion configuration */
    struct bmi3x0_any_motion_config any_motion;

    /*! No-motion configuration */
    struct bmi3x0_no_motion_config no_motion;

    /*! Sig_motion configuration */
    struct bmi3x0_sig_motion_config sig_motion;

    /*! Step counter/detector/activity configuration */
    struct bmi3x0_step_config step_counter;

    /*! Gyroscope user gain configuration */
    struct bmi3x0_gyro_user_gain_config gyro_gain_update;

    /*! Tilt configuration */
    struct bmi3x0_tilt_config tilt;

    /*! Orientation configuration */
    struct bmi3x0_orient_config orientation;

    /*! Flat configuration */
    struct bmi3x0_flat_config flat;

    /*! Wake-up configuration */
    struct bmi3x0_wake_up_config tap;

    /*! Alternate accelerometer configuration */
    struct bmi3x0_alt_accel_config alt_acc;

    /*! Alternate gyroscope configuration */
    struct bmi3x0_alt_gyro_config alt_gyr;

    /*! Alternate auto configuration */
    struct bmi3x0_alt_auto_config alt_auto_cfg;
};

/*!  @name Structure to define the type of the sensor and its configurations */
struct bmi3x0_sens_config
{
    /*! Defines the type of sensor */
    uint8_t type;

    /*! Defines various sensor configurations */
    union bmi3x0_sens_config_types cfg;
};

/*!
 *  @brief Error Status structure
 */
struct bmi3x0_err_reg
{
    /*! Indicates fatal error */
    uint8_t fatal_err;

    /*! Indicates interrupt request overrun error */
    uint8_t uc_irq_ovrn;

    /*! Indicates watch cell code */
    uint8_t uc_wc;

    /*! Indicates watchdog timer error */
    uint8_t uc_wd;

    /*! Indicates accel configuration error */
    uint8_t acc_conf_err;

    /*! Indicates gyro configuration error */
    uint8_t gyr_conf_err;

    /*! Indicates temperature configuration error */
    uint8_t temp_conf_err;

    /*! Indicates SDR parity error */
    uint8_t i3c_error0;

    /*! Indicates I3C error */
    uint8_t i3c_error3;
};

/*!
 *  @brief Feature engine error status structure
 */
struct bmi3x0_feature_engine_err_reg
{
    /*! Indicates feature engine is inactive */
    uint8_t feat_eng_inact;

    /*! Indicates feature engine is activated */
    uint8_t feat_eng_act;

    /*! Indicates that the configuration string download failed */
    uint8_t init_crc_err;

    /*! Indicates accelerometer and gyroscope user gain and offset update command cannot be processed because either a
     * sensor is active or self-calibration/self-test/axis-map is ongoing */
    uint8_t ugain_offs_upd_err;

    /*! Indicates no error */
    uint8_t no_error;

    /*! Indicates axis map command was not processed because either a sensor was active or self-calibration or self-test
     * was ongoing */
    uint8_t axis_map_err;

    /*! Indicates I3C TC-sync error because either I3C TC-sync enable request was sent while auto-low-power feature was
     * active or I3C TC-sync configuration command was sent with invalid TPH, TU and ODR values. */
    uint8_t tcsync_conf_err;

    /*! Indicates ongoing self-calibration (gyroscope only) or self-test (gyroscope only) was aborted.  */
    uint8_t sc_st_aborted;

    /*! Indicates self-calibration (gyroscope only) command ignored because either self-calibration or self-test or I3C
     * TC-sync was ongoing */
    uint8_t sc_ignored;

    /*! Indicates self-test (accelerometer and/or gyroscope) command ignored because either self-calibration or
     * self-test or I3C TC-sync was ongoing */
    uint8_t st_ignored;

    /*! Indicates self-calibration (gyroscope only) or self-test (accelerometer and/or gyroscope) command was not
     * processed because pre-conditions were not met. */
    uint8_t sc_st_precon_err;

    /*! Indicates auto-mode change feature was enabled or illegal sensor configuration change detected in
     * ACC_CONF/GYR_CONF while self-calibration or self-test was ongoing. */
    uint8_t mode_change_while_sc_st;

    /*! Indicates I3C TC-sync enable request was sent while self-test (accelerometer and/or gyroscope) was ongoing. */
    uint8_t postpone_i3c_sync;

    /*! Indicates illegal sensor configuration change detected in ACC_CONF/GYR_CONF while I3C TC-sync was active. */
    uint8_t mode_change_while_i3c_sync;
};

struct bmi3x0_feature_enable
{
    /*! Enables no-motion feature for X-axis */
    uint8_t no_mot_x_en;

    /*! Enables no-motion feature for Y-axis */
    uint8_t no_mot_y_en;

    /*! Enables no-motion feature for Z-axis */
    uint8_t no_mot_z_en;

    /*! Enables any-motion feature for X-axis */
    uint8_t any_mot_x_en;

    /*! Enables any-motion feature for Y-axis */
    uint8_t any_mot_y_en;

    /*! Enables any-motion feature for Z-axis */
    uint8_t any_mot_z_en;

    /*! Enables flat feature */
    uint8_t flat_en;

    /*! Enables orientation feature */
    uint8_t orientation_en;

    /*! Enables step detector feature */
    uint8_t step_detector_en;

    /*! Enables step counter feature */
    uint8_t step_counter_en;

    /*! Enables significant motion feature */
    uint8_t sig_mot_en;

    /*! Enables tilt feature */
    uint8_t tilt_en;

    /*! Enables single tap feature */
    uint8_t tap_detector_s_tap_en;

    /*! Enables double tap feature */
    uint8_t tap_detector_d_tap_en;

    /*! Enables triple tap feature */
    uint8_t tap_detector_t_tap_en;

    /*! Enables I3C TC-sync feature */
    uint8_t i3c_sync_en;
};

struct bmi3x0_map_int
{
    /*! Map interrupt output to either INT1 or INT2 or IBI
     *  Value   Name        Description
     *   00   DISABLED   Interrupt disabled
     *   01   MAP_INT1     Mapped to INT1
     *   10   MAP_INT2     Mapped to INT2
     *   11   MAP_IBI     Mapped to I3C IBI
     */

    /*! Maps no-motion output to either INT1 or INT2 or IBI */
    uint8_t no_mot_out;

    /*! Maps any-motion output to either INT1 or INT2 or IBI */
    uint8_t any_mot_out;

    /*! Maps flat output to either INT1 or INT2 or IBI */
    uint8_t flat_out;

    /*! Maps orientation output to either INT1 or INT2 or IBI */
    uint8_t orientation_out;

    /*! Maps step detector output to either INT1 or INT2 or IBI */
    uint8_t step_detector_out;

    /*! Maps step counter output to either INT1 or INT2 or IBI */
    uint8_t step_counter_out;

    /*! Maps significant motion output to either INT1 or INT2 or IBI */
    uint8_t sig_mot_out;

    /*! Maps tilt output to either INT1 or INT2 or IBI */
    uint8_t tilt_out;

    /*! Maps tap output to either INT1 or INT2 or IBI */
    uint8_t tap_out;

    /*! Maps i3c output to either INT1 or INT2 or IBI  */
    uint8_t i3c_out;

    /*! Maps feature engine's error or status change to either INT1 or INT2 or IBI */
    uint8_t err_status;

    /*! Maps temperature data ready interrupt to either INT1 or INT2 or IBI */
    uint8_t temp_drdy_int;

    /*! Maps gyro data ready interrupt to either INT1 or INT2 or IBI */
    uint8_t gyr_drdy_int;

    /*! Maps accel data ready interrupt to either INT1 or INT2 or IBI */
    uint8_t acc_drdy_int;

    /*! Maps FIFO watermark interrupt to either INT1 or INT2 or IBI */
    uint8_t fwm_int;

    /*! Maps FIFO full interrupt to either INT1 or INT2 or IBI */
    uint8_t ffull_int;
};

/*!
 *  @brief Structure to store config version
 */
struct bmi3x0_config_version
{
    /*! Maps config1 major version */
    uint16_t config1_major_version;

    /*! Maps config1 minor version */
    uint8_t config1_minor_version;

    /*! Maps config2 major version */
    uint16_t config2_major_version;

    /*! Maps config2 minor version */
    uint8_t config2_minor_version;
};

struct bmi3x0_self_calib_rslt
{
    /*! Stores the self-calibration result */
    uint8_t gyro_sc_rslt;

    /*! Stores the self-calibration error codes result */
    uint8_t sc_error_rslt;
};

/*!
 *  @brief Structure to store alternate status
 */
struct bmi3x0_alt_status
{
    /*! Stores alternate accel status */
    uint8_t alt_accel_status;

    /*! Stores alternate gyro status */
    uint8_t alt_gyro_status;
};

struct bmi3x0_acc_gyr_usr_gain_offset
{
    /* Reset user gain and offset */
    uint8_t ugain_off_reset;

    /* Accel user offset x-axis */
    uint16_t acc_usr_off_x;

    /* Accel user offset y-axis */
    uint16_t acc_usr_off_y;

    /* Accel user offset z-axis */
    uint16_t acc_usr_off_z;

    /* Accel user gain x-axis */
    uint8_t acc_usr_gain_x;

    /* Accel user gain y-axis */
    uint8_t acc_usr_gain_y;

    /* Accel user gain z-axis */
    uint8_t acc_usr_gain_z;

    /* Gyro user offset x-axis */
    uint16_t gyr_usr_off_x;

    /* Gyro user offset y-axis */
    uint16_t gyr_usr_off_y;

    /* Gyro user offset z-axis */
    uint16_t gyr_usr_off_z;

    /* Gyro user gain x-axis */
    uint8_t gyr_usr_gain_x;

    /* Gyro user gain y-axis */
    uint8_t gyr_usr_gain_y;

    /* Gyro user gain z-axis */
    uint8_t gyr_usr_gain_z;
};

/*! @name Structure to enable an accel axis for FOC */
struct bmi3x0_accel_foc_g_value
{
    /* '0' to disable x axis and '1' to enable x axis */
    uint8_t x;

    /* '0' to disable y axis and '1' to enable y axis */
    uint8_t y;

    /* '0' to disable z axis and '1' to enable z axis */
    uint8_t z;

    /* '0' for positive input and '1' for negative input */
    uint8_t sign;
};

/*! @name Structure to store temporary accelerometer/gyroscope values */
struct bmi3x0_foc_temp_value
{
    /*! X data */
    int32_t x;

    /*! Y data */
    int32_t y;

    /*! Z data */
    int32_t z;
};

/*! @name Structure to store accelerometer data deviation from ideal value */
struct bmi3x0_offset_delta
{
    /*! X axis */
    int16_t x;

    /*! Y axis */
    int16_t y;

    /*! Z axis */
    int16_t z;
};

#endif /* _BMI3X0_DEFS_H */
