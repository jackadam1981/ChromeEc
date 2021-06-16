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
 * @file       bma4.h
 * @date       2020-03-01
 * @version    V2.19.0
 *
 */

/*
 * @file       bma4.h
 * @brief   Source file for the BMA4 Sensor API
 */

/*!
 * @defgroup bma4xy BMA4XY
 */

/**
 * \ingroup bma4xy
 * \defgroup bma4 BMA4
 * @brief Sensor driver for BMA4 sensor
 */

#ifndef BMA4_H__
#define BMA4_H__

/*********************************************************************/
/* header files */

#include "bma4_defs.h"
#ifdef AKM9916
#include "aux_akm9916.h"
#endif

#ifdef BMM150
#include "aux_bmm150.h"
#endif

/*********************************************************************/
/* (extern) variable declarations */
/*********************************************************************/
/* function prototype declarations */

/**
 * \ingroup bma4
 * \defgroup bma4ApiInit Initialization
 * @brief Initialize the sensor and device structure
 */

/*!
 * \ingroup bma4ApiInit
 * \page bma4_api_bma4_init bma4_init
 * \code
 * int8_t bma4_init(struct bma4_dev *dev);
 * \endcode
 * @details This API is the entry point.
 * Call this API before using all other APIs.
 * This API reads the chip-id of the sensor which is the first step to
 * verify the sensor and also it configures the read mechanism of SPI and
 * I2C interface.
 *
 * @param[in,out] dev : Structure instance of bma4_dev
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 *
 * @note
 * While changing the parameter of the bma4
 * consider the following point:
 * Changing the reference value of the parameter
 * will changes the local copy or local reference
 * make sure your changes will not
 * affect the reference value of the parameter
 * (Better case don't change the reference value of the parameter)
 */
int8_t bma4_init(struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiConfig ConfigFile
 * @brief Write binary configuration in the sensor
 */

/**
 * \ingroup bma4
 * \defgroup bma4ApiRegisters Registers
 * @brief Perform read / write operation to registers of the sensor
 */

/*!
 * \ingroup bma4ApiRegisters
 * \page bma4_api_bma4_write_regs bma4_write_regs
 * \code
 * int8_t bma4_write_regs(uint8_t addr, uint8_t *data, uint8_t len, struct bma4_dev *dev);
 * \endcode
 * @details  This API checks whether the write operation requested is for
 * feature config or register write and accordingly writes the data in the
 * sensor.
 *
 * @note user has to disable the advance power save mode in the sensor when
 * using this API in burst write mode.
 * bma4_set_advance_power_save(BMA4_DISABLE, dev);
 *
 * @param[in] addr : Register address.
 * @param[in] data : Write data buffer
 * @param[in] len  : No of bytes to write
 * @param[in] dev  : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_write_regs(uint8_t addr, const uint8_t *data, uint32_t len, struct bma4_dev *dev);

/*!
 * \ingroup bma4ApiRegisters
 * \page bma4_api_bma4_read_regs bma4_read_regs
 * \code
 * int8_t bma4_write_regs(uint8_t addr, uint8_t *data, uint8_t len, struct bma4_dev *dev);
 * \endcode
 * @details This API checks whether the read operation requested is for
 * feature or register read and accordingly reads the data from the sensor.
 *
 * @param[in] addr : Register address.
 * @param[in] data : Read data buffer.
 * @param[in] len  : No of bytes to read.
 * @param[in] dev  : Structure instance of bma4_dev
 *
 * @note For most of the registers auto address increment applies, with the
 * exception of a few special registers, which trap the address. For e.g.,
 * Register address - 0x26, 0x5E.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_read_regs(uint8_t addr, uint8_t *data, uint32_t len, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiErrorStatus Error Status
 * @brief Read error status of the sensor
 */

/*!
 * \ingroup bma4ApiErrorStatus
 * \page bma4_api_bma4_get_error_status bma4_get_error_status
 * \code
 * int8_t bma4_get_error_status(struct bma4_err_reg *err_reg, struct bma4_dev *dev);;
 * \endcode
 * @details This API reads the error status from the sensor.
 *
 * Below table mention the types of error which can occur in the sensor
 *
 *@verbatim
 *************************************************************************
 *        Error           |       Description
 *************************|***********************************************
 *                        |       Fatal Error, chip is not in operational
 *        fatal           |       state (Boot-, power-system).
 *                        |       This flag will be reset only by
 *                        |       power-on-reset or soft reset.
 *************************|***********************************************
 *        cmd             |       Command execution failed.
 *************************|***********************************************
 *                        |       Value        Name       Description
 *        error_code      |       000        no_error    no error
 *                        |       001        acc_err      error in
 *                        |                               ACC_CONF
 *************************|***********************************************
 *                        |        Error in FIFO detected: Input data was
 *        fifo            |        discarded in stream mode. This flag
 *                        |        will be reset when read.
 *************************|***********************************************
 *        mag             |        Error in I2C-Master detected.
 *                        |        This flag will be reset when read.
 *************************************************************************
 *@endverbatim
 *
 * @param[in,out] err_reg : Pointer to structure variable which stores the
 * error status read from the sensor.
 * @param[in] dev : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_get_error_status(struct bma4_err_reg *err_reg, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiStatus Status
 * @brief Read sensor status
 */

/*!
 * \ingroup bma4ApiStatus
 * \page bma4_api_bma4_get_status bma4_get_status
 * \code
 * int8_t bma4_get_status(uint8_t *status, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the sensor status from the dev sensor.
 *
 * Below table lists the sensor status flags
 *
 * @verbatim
 *                 Status          |       Description
 *     ----------------------------|----------------------------------------
 *     BMA4_MAG_MAN_OP_ONGOING     | Manual Mag. interface operation ongoing
 *     BMA4_CMD_RDY                | Command decoder is ready.
 *     BMA4_MAG_DATA_RDY           | Data ready for Mag.
 *     BMA4_ACC_DATA_RDY           | Data ready for Accel.
 *@endverbatim
 *
 * @param[in] status : Variable used to store the sensor status flags
 * which is read from the sensor.
 * @param[in] dev : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_get_status(uint8_t *status, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiAccelxyz Accel XYZ Data
 * @brief Read accel xyz data from the sensor
 */

/*!
 * \ingroup bma4ApiAccelxyz
 * \page bma4_api_bma4_read_accel_xyz bma4_read_accel_xyz
 * \code
 * int8_t bma4_read_accel_xyz(struct bma4_accel *accel, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the Accel data for x,y and z axis from the sensor.
 *  The data units is in LSB format.
 *
 * @param[in] accel : Variable used to store the Accel data which is read
 * from the sensor.
 * @param[in] dev : Structure instance of bma4_dev.
 *
 * @note For setting the Accel configuration use the below function
 * bma4_set_accel_config
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_read_accel_xyz(struct bma4_accel *accel, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiSensorTime Sensor Time
 * @brief Read sensor time of the sensor
 */

/**
 * \ingroup bma4
 * \defgroup bma4ApiTemperature Temperature
 * @brief Read chip temperature of the sensor
 */

/*!
 * \ingroup bma4ApiTemperature
 * \page bma4_api_bma4_get_temperature bma4_get_temperature
 * \code
 * int8_t bma4_get_temperature(int32_t *temp, uint8_t temp_unit, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the chip temperature of sensor.
 * @note If Accel and Mag are disabled, the temperature value will be set
 * to invalid.
 *
 * @param[out] temp : Pointer variable which stores the temperature value.
 * @param[in] temp_unit : indicates the unit of temperature
 *
 * @verbatim
 * temp_unit   |   description
 * ------------|-------------------
 * BMA4_DEG    |   degrees Celsius
 * BMA4_FAHREN |   degrees fahrenheit
 * BMA4_KELVIN |   degrees kelvin
 *@endverbatim
 *
 * @param[in] dev : Structure instance of bma4_dev.
 *
 * @note Using a scaling factor of 1000, to obtain integer values, which
 * at the user end, are used to get accurate temperature value.
 * BMA4_SCALE_FARHAN = 1.8 * 1000, BMA4_SCALE_KELVIN = 273.15 * 1000
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_get_temperature(int32_t *temp, uint8_t temp_unit, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiAccel Accel Configuration
 * @brief Read / Write configurations of accel sensor
 */

/*!
 * \ingroup bma4ApiAccel
 * \page bma4_api_bma4_get_accel_config bma4_get_accel_config
 * \code
 * int8_t bma4_get_accel_config(struct bma4_accel_config *accel, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the Output data rate, Bandwidth, perf_mode
 * and Range of accel.
 *
 * @param[in,out] accel :  Address of user passed structure which is used
 *  to store the Accel configurations read from the sensor.
 *
 * @note Enums and corresponding values for structure parameters like
 * Odr, Bandwidth and Range are mentioned in the below tables.
 *
 *@verbatim
 *  Value      |        Odr
 *  -----------|------------------------------------
 *   1         |     BMA4_OUTPUT_DATA_RATE_0_78HZ
 *   2         |     BMA4_OUTPUT_DATA_RATE_1_56HZ
 *   3         |     BMA4_OUTPUT_DATA_RATE_3_12HZ
 *   4         |     BMA4_OUTPUT_DATA_RATE_6_25HZ
 *   5         |     BMA4_OUTPUT_DATA_RATE_12_5HZ
 *   6         |     BMA4_OUTPUT_DATA_RATE_25HZ
 *   7         |     BMA4_OUTPUT_DATA_RATE_50HZ
 *   8         |     BMA4_OUTPUT_DATA_RATE_100HZ
 *   9         |     BMA4_OUTPUT_DATA_RATE_200HZ
 *   10        |     BMA4_OUTPUT_DATA_RATE_400HZ
 *   11        |     BMA4_OUTPUT_DATA_RATE_800HZ
 *   12        |     BMA4_OUTPUT_DATA_RATE_1600HZ
 *@endverbatim
 *
 *@verbatim
 *  Value |  accel_bw
 *  ------|--------------------------
 *    0   |  BMA4_ACCEL_OSR4_AVG1
 *    1   |  BMA4_ACCEL_OSR2_AVG2
 *    2   |  BMA4_ACCEL_NORMAL_AVG4
 *    3   |  BMA4_ACCEL_CIC_AVG8
 *    4   |  BMA4_ACCEL_RES_AVG16
 *    5   |  BMA4_ACCEL_RES_AVG32
 *    6   |  BMA4_ACCEL_RES_AVG64
 *    7   |  BMA4_ACCEL_RES_AVG128
 *@endverbatim
 *
 *@verbatim
 *   Value   | g_range
 *   --------|---------------------
 *   0x00    | BMA4_ACCEL_RANGE_2G
 *   0x01    | BMA4_ACCEL_RANGE_4G
 *   0x02    | BMA4_ACCEL_RANGE_8G
 *   0x03    | BMA4_ACCEL_RANGE_16G
 *@endverbatim
 *
 * @param[in] dev : Structure instance of bma4_dev
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_get_accel_config(struct bma4_accel_config *accel, struct bma4_dev *dev);

/*!
 * \ingroup bma4ApiAccel
 * \page bma4_api_bma4_set_accel_config bma4_set_accel_config
 * \code
 * int8_t bma4_set_accel_config(struct bma4_accel_config *accel, struct bma4_dev *dev);
 * \endcode
 * @details This API sets the output_data_rate, bandwidth, perf_mode
 * and range of Accel.
 *
 * @param[in] accel : Pointer to structure variable which specifies the
 * Accel configurations.
 *
 * @note Enums and corresponding values for structure parameters like
 * Odr, Bandwidth and Range are mentioned in the below tables.
 *
 * @verbatim
 *  Value   |   ODR
 *  --------|-----------------------------------------
 *   1      |   BMA4_OUTPUT_DATA_RATE_0_78HZ
 *   2      |   BMA4_OUTPUT_DATA_RATE_1_56HZ
 *   3      |   BMA4_OUTPUT_DATA_RATE_3_12HZ
 *   4      |   BMA4_OUTPUT_DATA_RATE_6_25HZ
 *   5      |   BMA4_OUTPUT_DATA_RATE_12_5HZ
 *   6      |   BMA4_OUTPUT_DATA_RATE_25HZ
 *   7      |   BMA4_OUTPUT_DATA_RATE_50HZ
 *   8      |   BMA4_OUTPUT_DATA_RATE_100HZ
 *   9      |   BMA4_OUTPUT_DATA_RATE_200HZ
 *   10     |   BMA4_OUTPUT_DATA_RATE_400HZ
 *   11     |   BMA4_OUTPUT_DATA_RATE_800HZ
 *   12     |   BMA4_OUTPUT_DATA_RATE_1600HZ
 *
 *@endverbatim
 *
 *@verbatim
 *  Value |  accel_bw
 *  ------|--------------------------
 *    0   |  BMA4_ACCEL_OSR4_AVG1
 *    1   |  BMA4_ACCEL_OSR2_AVG2
 *    2   |  BMA4_ACCEL_NORMAL_AVG4
 *    3   |  BMA4_ACCEL_CIC_AVG8
 *    4   |  BMA4_ACCEL_RES_AVG16
 *    5   |  BMA4_ACCEL_RES_AVG32
 *    6   |  BMA4_ACCEL_RES_AVG64
 *    7   |  BMA4_ACCEL_RES_AVG128
 *@endverbatim
 *
 *@verbatim
 *  Value   | g_range
 *  --------|---------------------
 *  0x00    | BMA4_ACCEL_RANGE_2G
 *  0x01    | BMA4_ACCEL_RANGE_4G
 *  0x02    | BMA4_ACCEL_RANGE_8G
 *  0x03    | BMA4_ACCEL_RANGE_16G
 *@endverbatim
 *
 * @param[in] dev : Structure instance of bma4_dev
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 *
 */
int8_t bma4_set_accel_config(const struct bma4_accel_config *accel, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiAdvancedPowerMode Advanced Power Mode
 * @brief Read / Write advance power mode of accel sensor
 */

/*!
 * \ingroup bma4ApiAdvancedPowerMode
 * \page bma4_api_bma4_set_advance_power_save bma4_set_advance_power_save
 * \code
 * int8_t bma4_set_advance_power_save(uint8_t adv_pwr_save, struct bma4_dev *dev);
 * \endcode
 * @details This API sets the advance power save mode in the sensor.
 *
 * @note If advanced power save is enabled and the Accel  and/or
 * magnetometer operate in duty cycling mode, the length of the unlatched
 * DRDY interrupt pulse is longer than 1/3.2 kHz (312.5 us).
 *
 * @param[in] adv_pwr_save : The value of advance power save mode
 * @param[in] dev : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_set_advance_power_save(uint8_t adv_pwr_save, struct bma4_dev *dev);

/*!
 * \ingroup bma4ApiAdvancedPowerMode
 * \page bma4_api_bma4_get_advance_power_save bma4_get_advance_power_save
 * \code
 * int8_t bma4_get_advance_power_save(uint8_t adv_pwr_save, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the status of advance power save mode
 * from the sensor.
 *
 * @note If the advanced power save is enabled and the Accel  and/or
 * magnetometer operate in duty cycling mode, the length of the unlatched
 * DRDY interrupt pulse is longer than 1/3.2 kHz (312.5 us).
 *
 * @param[out] adv_pwr_save : The value of advance power save mode
 * @param[in] dev : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma4_get_advance_power_save(uint8_t *adv_pwr_save, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiAccelEnable Accel Enable
 * @brief Enables / Disables accelerometer in the sensor
 */

/*!
 * \ingroup bma4ApiAccelEnable
 * \page bma4_api_bma4_set_accel_enable bma4_set_accel_enable
 * \code
 * int8_t bma4_set_accel_enable(uint8_t accel_en, struct bma4_dev *dev);
 * \endcode
 * @details This API enables or disables the Accel in the sensor.
 *
 * @note Before reading Accel data, user should call this API.
 *
 * @param[in] accel_en : Variable used to enable or disable the  Accel.
 * @param[in] dev : Structure instance of bma4_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bma4_set_accel_enable(uint8_t accel_en, struct bma4_dev *dev);

/*!
 * \ingroup bma4ApiAccelEnable
 * \page bma4_api_bma4_get_accel_enable bma4_get_accel_enable
 * \code
 * int8_t bma4_get_accel_enable(uint8_t *accel_en, struct bma4_dev *dev);
 * \endcode
 * @details This API checks whether Accel is enabled or not in the sensor.
 *
 * @param[out] accel_en : Pointer variable used to store the Accel enable
 * status
 * @param[in] dev : Structure instance of bma4_dev.
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bma4_get_accel_enable(uint8_t *accel_en, struct bma4_dev *dev);

/*!
 * \ingroup bma4ApiAccelDataRdy
 * \page bma4_api_bma4_get_accel_data_rdy bma4_get_accel_data_rdy
 * \code
 * int8_t bma4_get_accel_data_rdy(uint8_t *data_rdy, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the data ready status of Accel from the sensor.
 * @note The status get reset when Accel data register is read.
 *
 * @param[out] data_rdy : Pointer variable to store the data ready  status
 * @param[in] dev : structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bma4_get_accel_data_rdy(uint8_t *data_rdy, struct bma4_dev *dev);


/**
 * \ingroup bma4
 * \defgroup bma4ApiASICStatus ASIC status
 * @brief Read ASIC status from the sensor
 */

/*!
 * \ingroup bma4ApiASICStatus
 * \page bma4_api_bma4_get_asic_status bma4_get_asic_status
 * \code
 * int8_t bma4_get_asic_status(struct bma4_asic_status *asic_status, struct bma4_dev *dev);
 * \endcode
 * @details This API reads the ASIC status from the sensor.
 * The status information is mentioned in the below table.
 *
 *@verbatim
 *******************************************************************************
 *       Status            |  Description
 **************************|****************************************************
 *      sleep              |  ASIC is in sleep/halt state.
 *      irq_ovrn           |  Dedicated interrupt is set again before previous
 *                         |  interrupt was acknowledged.
 *      wc_event           |  Watchcell event detected (ASIC stopped).
 *  stream_transfer_active | stream transfer has started.
 *******************************************************************************
 *@endverbatim
 *
 * @param[out] asic_status : Structure pointer used to store the ASIC
 * status read from the sensor.
 * @param[in] dev : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bma4_get_asic_status(struct bma4_asic_status *asic_status, struct bma4_dev *dev);

/**
 * \ingroup bma4
 * \defgroup bma4ApiOffsetComp Accel Offset Compensation
 * @brief Set / Get Accel Offset Compensation
 */

/*!
 * \ingroup bma4ApiOffsetComp
 * \page bma4_api_bma4_set_offset_comp bma4_set_offset_comp
 * \code
 * int8_t bma4_set_offset_comp(uint8_t offset_en, struct bma4_dev *dev);
 * \endcode
 * @details This API enables the offset compensation for filtered and
 * unfiltered  Accel data.
 *
 * @param[in] offset_en : Variable used to enable or disable offset
 * compensation
 *
 *@verbatim
 *  offset_en   |  Description
 *  ------------|----------------------
 *      0       | BMA4_DISABLE
 *      1       | BMA4_ENABLE
 *@endverbatim
 *
 * @param[in] dev : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bma4_set_offset_comp(uint8_t offset_en, struct bma4_dev *dev);

/*!
 * \ingroup bma4ApiOffsetComp
 * \page bma4_api_bma4_get_offset_comp bma4_get_offset_comp
 * \code
 * int8_t bma4_get_offset_comp(uint8_t *offset_en, struct bma4_dev *dev);
 * \endcode
 * @details This API gets the status of Accel offset compensation
 *
 * @param[out] offset_en : Pointer variable used to store the Accel offset
 * enable or disable status.
 *
 *@verbatim
 *  offset_en |  Description
 *  ----------|--------------
 *      0     | BMA4_DISABLE
 *      1     | BMA4_ENABLE
 *@endverbatim
 *
 * @param[in] dev : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval 0 -> Success
 * @retval < 0 -> Fail
 */
int8_t bma4_get_offset_comp(uint8_t *offset_en, struct bma4_dev *dev);



/**
 * \ingroup bma4
 * \defgroup bma4ApiAccelFoc Accel FOC
 * @brief Performs Fast Offset Compensation for accel
 */

/*!
 * \ingroup bma4ApiAccelFoc
 * \page bma4_api_bma4_perform_accel_foc bma4_perform_accel_foc
 * \code
 * int8_t bma4_perform_accel_foc(const struct bma4_accel_foc_g_value *accel_g_value, struct bma4_dev *dev);
 * \endcode
 * @details This API performs Fast Offset Compensation for Accel.
 * @param[in] accel_g_value : Array which stores the Accel g units
 *  for x,y and z-axis.
 *
 *@verbatim
 *      accel_g_value             |   Description
 *      --------------------------|---------------------------------------
 *      accel_g_value[0]          |   x-axis g units
 *      accel_g_value[1]          |   y-axis g units
 *      accel_g_value[2]          |   z-axis g units
 *@endverbatim
 *
 *  @param[in] dev : Structure instance of dev.
 *
 *  @return Result of API execution status.
 *  @retval 0 -> Success
 *  @retval Any non zero value -> Fail
 *
 */
int8_t bma4_perform_accel_foc(const struct bma4_accel_foc_g_value *accel_g_value, struct bma4_dev *dev);

#endif

/* End of __BMA4_H__ */
