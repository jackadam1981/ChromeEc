/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Modifications Copyright (C) 2021 Bosch Sensortec GmbH
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
 * @file       accelgyro_bmi3.h
 * @date       16 Jun 2021
 * @version    v0.0.0.2
 *
 */


#ifndef _ACCEL_GYRO_BMI3_H
#define _ACCEL_GYRO_BMI3_H

#include "accelgyro.h"
#include "common.h"
#include "driver/accelgyro_bmi323_public.h"


/********************************************************* */
/*!             General Macro Definitions                  */
/********************************************************* */
/*! LSB and MSB mask definitions */
#define BMI3_SET_LOW_BYTE                            UINT16_C(0x00FF)
#define BMI3_SET_HIGH_BYTE                           UINT16_C(0xFF00)

/*! For enable and disable */
#define BMI3_ENABLE                                  UINT8_C(1)
#define BMI3_DISABLE                                 UINT8_C(0)

/*! Utility macros */
#define BMI3_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MASK)) | \
     ((data << bitname##_POS) & bitname##_MASK))

#define BMI3_GET_BITS(reg_data, bitname) \
    ((reg_data & (bitname##_MASK)) >> \
     (bitname##_POS))

#define BMI3_SET_BIT_POS0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MASK)) | \
     (data & bitname##_MASK))

#define BMI3_GET_BIT_POS0(reg_data, bitname)         (reg_data & (bitname##_MASK))

/*! Defines mode of operation for Accelerometer. DO NOT COPY OPERATION DESCRIPTION TO CUSTOMER SPEC! */
#define BMI3_POWER_MODE_MASK                          UINT8_C(0x70)
#define BMI3_POWER_MODE_POS                           UINT8_C(4)


#define BMI3_SENS_ODR_MASK                            UINT8_C(0x0F)

/*! Full scale, Resolution */
#define BMI3_SENS_RANGE_MASK                          UINT8_C(0x70)
#define BMI3_SENS_RANGE_POS                           UINT8_C(4)

#define BMI3_CHIP_ID_MASK								UINT8_C()

/*! Map FIFO water-mark interrupt to either INT1 or INT2 or IBI */
#define BMI3_FWM_INT_MASK                            UINT8_C(0x30)
#define BMI3_FWM_INT_POS                             UINT8_C(4)

/*! Map FIFO full interrupt to either INT1 or INT2 or IBI */
#define BMI3_FFULL_INT_MASK                          UINT8_C(0xC0)
#define BMI3_FFULL_INT_POS                           UINT8_C(6)

/*! Configure level of INT1 pin */
#define BMI3_INT1_LVL_MASK                           UINT8_C(0x01)

/*! Configure behavior of INT1 pin */
#define BMI3_INT1_OD_MASK                            UINT8_C(0x02)
#define BMI3_INT1_OD_POS                             UINT8_C(1)

/*! Output enable for INT1 pin */
#define BMI3_INT1_OUTPUT_EN_MASK                     UINT8_C(0x04)
#define BMI3_INT1_OUTPUT_EN_POS                      UINT8_C(2)

/*! Input enable for INT1 pin */
#define BMI3_INT1_INPUT_EN_MASK                      UINT8_C(0x08)
#define BMI3_INT1_INPUT_EN_POS                       UINT8_C(3)

/*!  Mask definitions for interrupt pin configuration */
#define BMI3_INT_LATCH_MASK                          UINT16_C(0x0001)

/*! Current fill level of FIFO buffer
 * An empty FIFO corresponds to 0x000. The word counter may be reset by reading out all frames from the FIFO buffer or
 * when the FIFO is reset through fifo_flush. The word counter is updated each time a complete frame was read or
 * written. */
#define BMI3_FIFO_FILL_LVL_MASK                      UINT8_C(0x07)
/********************************************************* */
/*!                 Register Addresses                     */
/********************************************************* */
/*! To define the chip id address */
#define BMI3_REG_CHIP_ID                             UINT8_C(0x00)

/*! Sensor status flags */
#define BMI3_REG_STATUS                              UINT8_C(0x02)

/*! ACC Data X. */
#define BMI3_REG_ACC_DATA_X                          UINT8_C(0x03)

/*! GYR Data X. */
#define BMI3_REG_GYR_DATA_X                          UINT8_C(0x06)

/*! INT1 Status Register.
 *  This register is clear-on-read.
 */
#define BMI3_REG_INT_STATUS_INT1                     UINT8_C(0x0D)

/*! FIFO fill state in words */
#define BMI3_REG_FIFO_FILL_LVL                       UINT8_C(0x15)

/*! FIFO data output register */
#define BMI3_REG_FIFO_DATA                           UINT8_C(0x16)


/*! Sets the output data rate, bandwidth, range and the mode of the Accelerometer */
#define BMI3_REG_ACC_CONF                            UINT8_C(0x20)

/*! Sets the output data rate, bandwidth, range and the mode of the Gyroscope in the sensor */
#define BMI3_REG_GYR_CONF                            UINT8_C(0x21)

/*! Interrupt Mapping Register for feature engine interrupts A-H. */
#define BMI3_REG_INT_MAP1                            UINT8_C(0x3A)

/*! FIFO watermark level */
#define BMI3_REG_FIFO_WATERMARK                      UINT8_C(0x35)

/*! FIFO configuration */
#define BMI3_REG_FIFO_CONF                           UINT8_C(0x36)

/*! FIFO Control */
#define BMI3_REG_FIFO_CTRL                           UINT8_C(0x37)

/*! Configures the electrical behavior of the interrupt pins */
#define BMI3_REG_IO_INT_CTRL                         UINT8_C(0x38)

/*! Global feature engine control register */
#define BMI3_REG_FEATURE_ENGINE_GLOB_CTRL            UINT8_C(0x40)

/*! Command Register */
#define BMI3_REG_CMD                                 UINT8_C(0x7E)

/********************************************************* */
/*!                 Sensor Specific macros                 */
/********************************************************* */
/*! BMI3 I2C address */
#define BMI3_ADDR_I2C_PRIM                           UINT8_C(0x68)
#define BMI3_ADDR_I2C_SEC                            UINT8_C(0x69)

/*! To define the chip id of bmi3 */
#define BMI3_CHIP_ID_PRIM                            UINT8_C(0x40)
#define BMI3_CHIP_ID_SEC                             UINT8_C(0x41)

#define BMI3_16_BIT_RESOLUTION                       UINT8_C(16)

#define BMI3_CMD_SOFT_RESET                          UINT16_C(0xDEAF)

/*!  Accelerometer G Range */
#define BMI3_ACC_RANGE_2G                            UINT8_C(0x00)
#define BMI3_ACC_RANGE_4G                            UINT8_C(0x01)
#define BMI3_ACC_RANGE_8G                            UINT8_C(0x02)
#define BMI3_ACC_RANGE_16G                           UINT8_C(0x03)
#define BMI3_ACC_RANGE_32G                           UINT8_C(0x04)
/*!  Accelerometer power modes */
#define BMI3_ACC_MODE_DISABLE                        UINT8_C(0x00)
#define BMI3_ACC_MODE_ULTRA_LOW_PWR                  UINT8_C(0X02)
#define BMI3_ACC_MODE_LOW_PWR                        UINT8_C(0x03)
#define BMI3_ACC_MODE_NORMAL                         UINT8_C(0X04)
#define BMI3_ACC_MODE_HIGH_PERF                      UINT8_C(0x07)

/*! Gyroscope DPS Range */
#define BMI3_GYR_RANGE_125DPS                        UINT8_C(0x00)
#define BMI3_GYR_RANGE_250DPS                        UINT8_C(0x01)
#define BMI3_GYR_RANGE_500DPS                        UINT8_C(0x02)
#define BMI3_GYR_RANGE_1000DPS                       UINT8_C(0x03)
#define BMI3_GYR_RANGE_2000DPS                       UINT8_C(0x04)
#define BMI3_GYR_RANGE_4000DPS                       UINT8_C(0x05)
#define BMI3_GYR_RANGE_8000DPS                       UINT8_C(0x06)
#define BMI3_GYR_RANGE_16000DPS                      UINT8_C(0x07)
/*!  Gyroscope power modes */
#define BMI3_GYR_MODE_DISABLE                        UINT8_C(0x00)
#define BMI3_GYR_MODE_SUSPEND                        UINT8_C(0X01)
#define BMI3_GYR_MODE_ULTRA_LOW_PWR                  UINT8_C(0X02)
#define BMI3_GYR_MODE_LOW_PWR                        UINT8_C(0x03)
#define BMI3_GYR_MODE_NORMAL                         UINT8_C(0X04)
#define BMI3_GYR_MODE_HIGH_PERF                      UINT8_C(0x07)

/*! @name BMI3 Interrupt Pin latch settings */
#define BMI3_INT_LATCH_EN                            UINT8_C(1)
#define BMI3_INT_LATCH_DISABLE                       UINT8_C(0)

/*! @name BMI3 Interrupt Pin Behavior */
#define BMI3_INT_PUSH_PULL                           UINT8_C(0)
#define BMI3_INT_OPEN_DRAIN                          UINT8_C(1)

/*! @name BMI3 Interrupt Pin Level */
#define BMI3_INT_ACTIVE_LOW                          UINT8_C(0)
#define BMI3_INT_ACTIVE_HIGH                         UINT8_C(1)

/*! @name BMI3 Interrupt Output Enable */
#define BMI3_INT_OUTPUT_DISABLE                      UINT8_C(0)
#define BMI3_INT_OUTPUT_ENABLE                       UINT8_C(1)

/*! Mask definitions for FIFO frame content configuration */
#define BMI3_FIFO_STOP_ON_FULL                       UINT8_C(0x01)
#define BMI3_FIFO_TIME_EN                            UINT8_C(0x01)
#define BMI3_FIFO_ACC_EN                             UINT8_C(0x02)
#define BMI3_FIFO_GYR_EN                             UINT8_C(0x04)
#define BMI3_FIFO_TEMP_EN                            UINT8_C(0x08)
#define BMI3_FIFO_ALL_EN                             UINT8_C(0x0F)

/*! FIFO sensor data lengths */
#define BMI3_LENGTH_FIFO_ACC                         UINT8_C(6)
#define BMI3_LENGTH_FIFO_GYR                         UINT8_C(6)

/*! Macro to define accelerometer configuration value for FOC */
#define BMI3_FOC_ACC_CONF_VAL_LSB                    UINT8_C(0xB7)
#define BMI3_FOC_ACC_CONF_VAL_MSB                    UINT8_C(0x40)

/*! Macro to define the accel FOC range */
#define BMI3_ACC_FOC_2G_REF                          UINT16_C(16384)
#define BMI3_ACC_FOC_4G_REF                          UINT16_C(8192)
#define BMI3_ACC_FOC_8G_REF                          UINT16_C(4096)
#define BMI3_ACC_FOC_16G_REF                         UINT16_C(2048)

#define BMI3_FOC_SAMPLE_LIMIT                        UINT8_C(128)

    /* 20ms delay for 50Hz ODR */
#define FOC_TRY_COUNT 5
#define FOC_DELAY     20

#define BMI3_INT_STATUS_FWM                          UINT16_C(0x4000)
#define BMI3_INT_STATUS_FFULL                        UINT16_C(0x8000)


//FIFO RELATED MACROS
/*! FIFO sensor data lengths */
#define BMI3_FIFO_ACC_LENGTH                         UINT8_C(6)
#define BMI3_FIFO_GYR_LENGTH                         UINT8_C(6)
#define BMI3_SENSOR_TIME_LENGTH                      UINT8_C(2)
/*! Masks for FIFO dummy data frames */
#define BMI3_FIFO_GYRO_DUMMY_FRAME                   UINT16_C(0x7f02)
#define BMI3_FIFO_ACCEL_DUMMY_FRAME                  UINT16_C(0x7f01)


/*!  @name Enum to define interrupt lines */
enum bmi3_hw_int_pin {
    BMI3_INT_NONE,
    BMI3_INT1,
    BMI3_INT2,
    BMI3_I3C_INT,
    BMI3_INT_PIN_MAX
};

/*! Structure to define FIFO frame configuration */
struct bmi3_fifo_frame
{
    /*! Pointer to FIFO data */
    uint8_t *data;

    /*! Number of user defined bytes of FIFO to be read */
    uint16_t length;

    /*! Enables type of data to be streamed - accelerometer,
     *  gyroscope
     */
    uint8_t available_fifo_sens;

    /*! Water-mark level for water-mark interrupt */
    uint16_t wm_lvl;

    /*! Available fifo length */
    uint16_t available_fifo_len;
};

typedef enum sensor_index_t
{
	FIRST_CONT_SENSOR = 0,
    SENSOR_ACCEL = FIRST_CONT_SENSOR,
	SENSOR_GYRO,
    NUM_OF_PRIMARY_SENSOR,
}sensor_index_en;

/*! @name Structure to define FIFO accel, gyro x, y and z axes */
struct bmi3_fifo_data
{
    /*! Data in x-axis */
    int16_t x;

    /*! Data in y-axis */
    int16_t y;

    /*! Data in z-axis */
    int16_t z;
};


#endif /* End of _ACCEL_GYRO_BMI3_H */
