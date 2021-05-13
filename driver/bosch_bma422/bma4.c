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
 * @file       bma4.c
 * @date       2020-03-01
 * @version    V2.19.0
 *
 */

/*
 * @file       bma4.c
 * @brief      Minimal source file for the BMA4 Sensor API
 */

/***************************************************************************/

/*!
 * @defgroup bma4 BMA4
 */

/**\name        Header files
 ****************************************************************************/
#include "bma4.h"

/***************************************************************************/

/**\name        Local structures
 ****************************************************************************/

/*!
 * @brief Accel data deviation from ideal value
 */
struct bma4_offset_delta
{
    /*! X axis */
    int16_t x;

    /*! Y axis */
    int16_t y;

    /*! Z axis */
    int16_t z;
};

/*!
 * @brief Accel offset xyz structure
 */
struct bma4_accel_offset
{
    /*! Accel offset X  data */
    uint8_t x;

    /*! Accel offset Y  data */
    uint8_t y;

    /*! Accel offset Z  data */
    uint8_t z;
};

/***************************************************************************/

/*! Static Function Declarations
 ****************************************************************************/

/*!
 *  @brief This API validates the bandwidth, odr and perf_mode
 *  combinations set by the user.
 *
 *  param bandwidth[in] : bandwidth value set by the user.
 *  param perf_mode[in] : perf_mode value set by the user.
 *  param odr[in]       : odr value set by the user.
 */
static int8_t validate_odr_bandwidth_perfmode(uint8_t odr, uint8_t bandwidth, uint8_t perf_mode);

/*!
 *  @brief This API validates the bandwidth and perfmode
 *  value set by the user.
 *
 *  param bandwidth[in] : bandwidth value set by the user.
 *  param perf_mode[in] : perf_mode value set by the user.
 */
static int8_t validate_bandwidth_perfmode(uint8_t bandwidth, uint8_t perf_mode);

/*!
 *  @brief @brief This API validates the ODR value set by the user.
 *
 *  param odr[in] : odr value set by the user.
 */
static int8_t validate_odr(uint8_t odr);

/*!
 *  @brief This API validates the bandwidth and odr when perf_mode = BMA4_CIC_AVG_MODE
 *
 *  param bandwidth[in] : bandwidth value set by the user.
 *  param odr[in]       : odr value set by the user.
 */
static int8_t validate_bandwidth_odr(uint8_t bandwidth, uint8_t odr);

/*!
 *  @brief This API reads the 8-bit data from the given register
 *  in the sensor.
 *
 *  @param[in] addr : Register address.
 *  @param[in] data : Read data buffer.
 *  @param[in] len  : No of bytes to read.
 *  @param[in] dev  : Structure instance of bma4_dev
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
static int8_t read_regs(uint8_t addr, uint8_t *data, uint32_t len, struct bma4_dev *dev);

/*!
 *  @brief This API writes the 8-bit data to the given register
 *  in the sensor.
 *
 *  @param[in] addr : Register address.
 *  @param[in] data : Write data buffer
 *  @param[in] len  : No of bytes to write
 *  @param[in] dev  : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
static int8_t write_regs(uint8_t addr, const uint8_t *data, uint32_t len, struct bma4_dev *dev);

/*!
 * @brief This API is used to calculate the power of given
 * base value.
 *
 * @param[in] base : value of base
 * @param[in] resolution : resolution of the sensor
 *
 * @return : Return the value of base^resolution
 */
static int32_t power(int16_t base, uint8_t resolution);

/*!
 * @brief This API finds the the null error of the device pointer structure
 *
 * @param[in] dev : Structure instance of bma4_dev.
 *
 *  @return Result of API execution status
 *  @retval BMA4_OK -> Success
 *  @retval BMA4_E_NULL_PTR -> Null pointer Error
 */
static int8_t null_pointer_check(const struct bma4_dev *dev);

/*!
 * @brief This internal API saves the configurations before performing FOC.
 *
 * @param[out] acc_cfg      : Accelerometer configuration value
 * @param[out] aps          : Advance power mode value
 * @param[out] acc_en       : Accelerometer enable value
 * @param[in] dev           : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval BMA4_OK - Success.
 * @retval BMA4_E_COM_FAIL - Error: Communication fail
 * @retval BMA4_E_INVALID_SENSOR - Error: Invalid sensor
 */
static int8_t save_accel_foc_config(struct bma4_accel_config *acc_cfg,
                                    uint8_t *aps,
                                    uint8_t *acc_en,
                                    struct bma4_dev *dev);

/*!
 * @brief This internal API sets configurations for performing accelerometer FOC.
 *
 * @param[in] dev       : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval BMA4_OK - Success.
 * @retval BMA4_E_COM_FAIL - Error: Communication fail
 * @retval BMA4_E_INVALID_SENSOR - Error: Invalid sensor
 */
static int8_t set_accel_foc_config(struct bma4_dev *dev);

/*!
 * @brief This internal API enables/disables the offset compensation for
 * filtered and un-filtered accelerometer data.
 *
 * @param[in] offset_en     : Enables/Disables offset compensation.
 * @param[in] dev           : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval BMA4_OK - Success.
 * @retval BMA4_E_COM_FAIL - Error: Communication fail
 */
static int8_t set_bma4_accel_offset_comp(uint8_t offset_en, struct bma4_dev *dev);

/*!
 * @brief This internal API performs Fast Offset Compensation for accelerometer.
 *
 * @param[in] accel_g_value : This parameter selects the accel FOC
 * axis to be performed
 *
 * Input format is {x, y, z, sign}. '1' to enable. '0' to disable
 *
 * Eg:- To choose x axis  {1, 0, 0, 0}
 * Eg:- To choose -x axis {1, 0, 0, 1}
 *
 * @param[in] acc_cfg       : Accelerometer configuration value
 * @param[in] dev           : Structure instance of bma4_dev.
 *
 * @return Result of API execution status
 *
 * @retval BMA4_OK - Success.
 * @retval BMA4_E_NULL_PTR - Error: Null pointer error
 * @retval BMA4_E_COM_FAIL - Error: Communication fail
 */
static int8_t perform_accel_foc(const struct bma4_accel_foc_g_value *accel_g_value,
                                const struct bma4_accel_config *acc_cfg,
                                struct bma4_dev *dev);

/*!
 * @brief This internal API converts the range value into accelerometer
 * corresponding integer value.
 *
 * @param[in] range_in      : Input range value.
 * @param[out] range_out    : Stores the integer value of range.
 *
 * @return None
 * @retval None
 */
static void map_accel_range(uint8_t range_in, uint8_t *range_out);

/*!
 * @brief This internal API compensate the accelerometer data against gravity.
 *
 * @param[in] lsb_per_g     : LSB value per 1g.
 * @param[in] g_val         : Gravity reference value of all axes.
 * @param[in] data          : Accelerometer data
 * @param[out] comp_data    : Stores the data that is compensated by taking the
 *                            difference in accelerometer data and lsb_per_g
 *                            value.
 *
 * @return None
 * @retval None
 */
static void comp_for_gravity(uint16_t lsb_per_g,
                             const struct bma4_accel_foc_g_value *g_val,
                             const struct bma4_accel *data,
                             struct bma4_offset_delta *comp_data);

/*!
 * @brief This internal API scales the compensated accelerometer data according
 * to the offset register resolution.
 *
 * @param[in] range         : Gravity range of the accelerometer.
 * @param[out] comp_data    : Data that is compensated by taking the
 *                            difference in accelerometer data and lsb_per_g
 *                            value.
 * @param[out] data         : Stores offset data
 * @param[in] resolution    : Resolution of bma4 sensor
 *
 * @return None
 * @retval None
 */
static void scale_bma4_accel_offset(uint8_t range,
                                    const struct bma4_offset_delta *comp_data,
                                    struct bma4_accel_offset *data,
                                    uint8_t resolution);

/*!
 * @brief This internal API inverts the accelerometer offset data.
 *
 * @param[out] offset_data  : Stores the inverted offset data
 *
 * @return None
 * @retval None
 */
static void invert_bma4_accel_offset(struct bma4_accel_offset *offset_data);

/*!
 * @brief This internal API writes the offset data in the offset compensation
 * register.
 *
 * @param[in] offset        : Offset data
 * @param[in] dev           : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval BMA4_OK - Success.
 * @retval BMA4_E_COM_FAIL - Error: Communication fail
 */
static int8_t write_bma4_accel_offset(const struct bma4_accel_offset *offset, struct bma4_dev *dev);

/*!
 * @brief This internal API finds the bit position of 3.9mg according to given
 * range and resolution.
 *
 * @param[in] range        : Gravity range of the accelerometer.
 * @param[in] resolution   : Resolution of sensor
 *
 * @return Result of API execution status
 * @retval Bit position of 3.9mg
 */
static int8_t get_bit_pos_3_9mg(uint8_t range, uint8_t resolution);

/*!
 * @brief This internal API restores the configurations saved before performing
 * accelerometer FOC.
 *
 * @param[in] acc_cfg       : Accelerometer configuration value
 * @param[in] aps           : Advance power mode value
 * @param[in] acc_en        : Accelerometer enable value
 * @param[in] dev           : Structure instance of bma4_dev
 *
 * @return Result of API execution status
 * @retval BMA4_OK - Success.
 * @retval BMA4_E_COM_FAIL - Error: Communication fail
 * @retval BMA4_E_INVALID_SENSOR - Error: Invalid sensor
 * @retval BMA4_E_SET_APS_FAIL - Error: Set Advance Power Save Fail
 */
static int8_t restore_accel_foc_config(const struct bma4_accel_config *acc_cfg,
                                       uint8_t aps,
                                       uint8_t acc_en,
                                       struct bma4_dev *dev);

/***************************************************************************/

/**\name        Extern Declarations
 ****************************************************************************/

/***************************************************************************/

/**\name        Globals
 ****************************************************************************/

/***************************************************************************/

/**\name        Function definitions
 ****************************************************************************/

/*!
 *  @brief This API is the entry point.
 *  Call this API before using all other APIs.
 *  This API reads the chip-id of the sensor which is the first step to
 *  verify the sensor and also it configures the read mechanism of SPI and
 *  I2C interface.
 */
int8_t bma4_init(struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;
    uint8_t dummy_read = 0;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        if (dev->intf == BMA4_SPI_INTF)
        {
            dev->dummy_byte = 1;
            rslt = bma4_read_regs(BMA4_CHIP_ID_ADDR, &dummy_read, 1, dev);
        }
        else
        {
            dev->dummy_byte = 0;
        }

        if (rslt == BMA4_OK)
        {
            rslt = bma4_read_regs(BMA4_CHIP_ID_ADDR, &data, 1, dev);
            if (rslt == BMA4_OK)
            {
                /* Assign Chip Id */
                dev->chip_id = data;
            }
        }
    }

    return rslt;
}


/*!
 *  @brief This API checks whether the write operation requested is for feature
 *  config or register write and accordingly writes the data in the sensor.
 */
int8_t bma4_write_regs(uint8_t addr, const uint8_t *data, uint32_t len, struct bma4_dev *dev)
{
    int8_t rslt;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (data != NULL))
    {
        if (addr == BMA4_FEATURE_CONFIG_ADDR)
        {
			/*@note: This involves config stream access so currently not needed */
			rslt = BMA4_E_COM_FAIL;
        }
        else
        {
            rslt = write_regs(addr, data, len, dev);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*! @cond DOXYGEN_SUPRESS */

/* Suppressing doxygen warnings triggered for same static function names present across various sensor variant
 * directories */

/*!
 *  @brief This API writes the 8-bit data to the given register
 *  in the sensor.
 */
static int8_t write_regs(uint8_t addr, const uint8_t *data, uint32_t len, struct bma4_dev *dev)
{
    int8_t rslt;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (data != NULL))
    {
        if (dev->intf == BMA4_SPI_INTF)
        {
            addr = addr & BMA4_SPI_WR_MASK;
        }

        /* write data in the register*/
        dev->intf_rslt = dev->bus_write(addr, data, len, dev->intf_ptr);

        if (dev->intf_rslt == BMA4_INTF_RET_SUCCESS)
        {
            /* After write operation 2us delay is required when
             * device operates in performance mode whereas
             * 450us is required when the device operates in suspend and low power mode.
             * NOTE: For more information refer datasheet */
            if (dev->perf_mode_status == BMA4_ENABLE)
            {
                dev->delay_us(2, dev->intf_ptr);
            }
            else
            {
                dev->delay_us(450, dev->intf_ptr);
            }
        }
        else
        {
            rslt = BMA4_E_COM_FAIL;
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}


/*!
 *  @brief This API checks whether the read operation requested is for feature
 *  or register read and accordingly reads the data from the sensor.
 */
int8_t bma4_read_regs(uint8_t addr, uint8_t *data, uint32_t len, struct bma4_dev *dev)
{
    int8_t rslt;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (data != NULL))
    {
        if (addr == BMA4_FEATURE_CONFIG_ADDR)
        {
			/*@note: This involves config stream access so currently not needed */
			rslt = BMA4_E_COM_FAIL;
        }
        else
        {
            rslt = read_regs(addr, data, len, dev);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*! @cond DOXYGEN_SUPRESS */

/* Suppressing doxygen warnings triggered for same static function names present across various sensor variant
 * directories */

/*!
 *  @brief This API reads the 8-bit data from the given register
 *  in the sensor.
 */
static int8_t read_regs(uint8_t addr, uint8_t *data, uint32_t len, struct bma4_dev *dev)
{
    int8_t rslt;
    uint16_t indx;
    uint8_t temp_buff[BMA4_MAX_LEN];

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (data != NULL))
    {
        if (dev->intf == BMA4_SPI_INTF)
        {
            /* SPI mask added */
            addr = addr | BMA4_SPI_RD_MASK;
        }

        /* Read the data from the register */
        dev->intf_rslt = dev->bus_read(addr, temp_buff, (len + dev->dummy_byte), dev->intf_ptr);

        if (dev->intf_rslt == BMA4_INTF_RET_SUCCESS)
        {
            for (indx = 0; indx < len; indx++)
            {
                /* Parsing and storing the valid data */
                data[indx] = temp_buff[indx + dev->dummy_byte];
            }
        }
        else
        {
            rslt = BMA4_E_COM_FAIL;
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*! @endcond */

/*!
 *  @brief This API reads the error status from the sensor.
 */
int8_t bma4_get_error_status(struct bma4_err_reg *err_reg, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (err_reg != NULL))
    {
        /* Read the error codes*/
        rslt = bma4_read_regs(BMA4_ERROR_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            /* Fatal error*/
            err_reg->fatal_err = BMA4_GET_BITS_POS_0(data, BMA4_FATAL_ERR);

            /* Cmd error*/
            err_reg->cmd_err = BMA4_GET_BITSLICE(data, BMA4_CMD_ERR);

            /* User error*/
            err_reg->err_code = BMA4_GET_BITSLICE(data, BMA4_ERR_CODE);

            /* FIFO error*/
            err_reg->fifo_err = BMA4_GET_BITSLICE(data, BMA4_FIFO_ERR);

            /* Mag data ready error*/
            err_reg->aux_err = BMA4_GET_BITSLICE(data, BMA4_AUX_ERR);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API reads the sensor status from the sensor.
 */
int8_t bma4_get_status(uint8_t *status, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (status != NULL))
    {
        /* Read the error codes*/
        rslt = bma4_read_regs(BMA4_STATUS_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            *status = data;
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API reads the Accel data for x,y and z axis from the sensor.
 *  The data units is in LSB format.
 */
int8_t bma4_read_accel_xyz(struct bma4_accel *accel, struct bma4_dev *dev)
{
    int8_t rslt;
    uint16_t lsb = 0;
    uint16_t msb = 0;
    uint8_t data[BMA4_ACCEL_DATA_LENGTH] = { 0 };

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (accel != NULL))
    {
        rslt = bma4_read_regs(BMA4_DATA_8_ADDR, data, BMA4_ACCEL_DATA_LENGTH, dev);
        if (rslt == BMA4_OK)
        {
            msb = data[1];
            lsb = data[0];

            /* Accel data x axis */
            accel->x = (int16_t)((msb << 8) | lsb);
            msb = data[3];
            lsb = data[2];

            /* Accel data y axis */
            accel->y = (int16_t)((msb << 8) | lsb);
            msb = data[5];
            lsb = data[4];

            /* Accel data z axis */
            accel->z = (int16_t)((msb << 8) | lsb);
            if (dev->resolution == BMA4_12_BIT_RESOLUTION)
            {
                accel->x = (accel->x / 0x10);
                accel->y = (accel->y / 0x10);
                accel->z = (accel->z / 0x10);
            }
            else if (dev->resolution == BMA4_14_BIT_RESOLUTION)
            {
                accel->x = (accel->x / 0x04);
                accel->y = (accel->y / 0x04);
                accel->z = (accel->z / 0x04);
            }

            /* Get the re-mapped accelerometer data */
//            get_remapped_data(accel, dev);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API reads the chip temperature of sensor.
 *
 *  @note Using a scaling factor of 1000, to obtain integer values, which
 *  at the user end, are used to get accurate temperature value .
 *  BMA4_FAHREN_SCALED = 1.8 * 1000, BMA4_KELVIN_SCALED = 273.15 * 1000
 */
int8_t bma4_get_temperature(int32_t *temp, uint8_t temp_unit, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data[BMA4_TEMP_DATA_SIZE] = { 0 };
    int32_t temp_raw_scaled = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (temp != NULL))
    {
        /* Read temperature value from the register */
        rslt = bma4_read_regs(BMA4_TEMPERATURE_ADDR, data, BMA4_TEMP_DATA_SIZE, dev);
        if (rslt == BMA4_OK)
        {
            temp_raw_scaled = (int32_t)data[BMA4_TEMP_BYTE] * BMA4_SCALE_TEMP;
        }

        /* '0' value read from the register corresponds to 23 degree C */
        (*temp) = temp_raw_scaled + (BMA4_OFFSET_TEMP * BMA4_SCALE_TEMP);
        switch (temp_unit)
        {
            case BMA4_DEG:
                break;
            case BMA4_FAHREN:

                /* Temperature in degree Fahrenheit */
                (*temp) = (((*temp) / BMA4_SCALE_TEMP) * BMA4_FAHREN_SCALED) + (32 * BMA4_SCALE_TEMP);
                break;
            case BMA4_KELVIN:

                /* Temperature in degree Kelvin */
                (*temp) = (*temp) + BMA4_KELVIN_SCALED;
                break;
            default:
                break;
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API reads the Output data rate, Bandwidth, perf_mode
 *  and Range of accel.
 */
int8_t bma4_get_accel_config(struct bma4_accel_config *accel, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data[2] = { 0 };

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (accel != NULL))
    {
        rslt = bma4_read_regs(BMA4_ACCEL_CONFIG_ADDR, data, BMA4_ACCEL_CONFIG_LENGTH, dev);
        if (rslt == BMA4_OK)
        {
            /* To get the ODR */
            accel->odr = BMA4_GET_BITS_POS_0(data[0], BMA4_ACCEL_ODR);

            /* To get the bandwidth */
            accel->bandwidth = BMA4_GET_BITSLICE(data[0], BMA4_ACCEL_BW);

            /* To get the under sampling mode */
            accel->perf_mode = BMA4_GET_BITSLICE(data[0], BMA4_ACCEL_PERFMODE);

            /* Read the Accel range */
            accel->range = BMA4_GET_BITS_POS_0(data[1], BMA4_ACCEL_RANGE);

            /* Flag bit to store the performance mode status */
            dev->perf_mode_status = accel->perf_mode;
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API sets the output_data_rate, bandwidth, perf_mode
 *  and range of Accel.
 */
int8_t bma4_set_accel_config(const struct bma4_accel_config *accel, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t accel_config_data[2] = { 0, 0 };

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (accel != NULL))
    {
        /* Check whether the bandwidth , odr and perf_mode combinations are valid */
        rslt = validate_odr_bandwidth_perfmode(accel->odr, accel->bandwidth, accel->perf_mode);

        if (rslt == BMA4_OK)
        {
            accel_config_data[0] = accel->odr & BMA4_ACCEL_ODR_MSK;
            accel_config_data[0] |= (uint8_t)(accel->bandwidth << BMA4_ACCEL_BW_POS);
            accel_config_data[0] |= (uint8_t)(accel->perf_mode << BMA4_ACCEL_PERFMODE_POS);
            accel_config_data[1] = accel->range & BMA4_ACCEL_RANGE_MSK;

            rslt = bma4_write_regs(BMA4_ACCEL_RANGE_ADDR, &accel_config_data[1], 1, dev);

            if (rslt == BMA4_OK)
            {
                /* Flag bit to store the performance mode status */
                dev->perf_mode_status = ((accel_config_data[0] & BMA4_ACCEL_PERFMODE_MSK) >> BMA4_ACCEL_PERFMODE_POS);

                rslt = bma4_write_regs(BMA4_ACCEL_CONFIG_ADDR, &accel_config_data[0], 1, dev);
            }
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*! @cond DOXYGEN_SUPRESS */

/* Suppressing doxygen warnings triggered for same static function names present across various sensor variant
 * directories */

/*!
 *  @brief This API validates the bandwidth, odr and perf_mode
 *  combinations set by the user.
 */
static int8_t validate_odr_bandwidth_perfmode(uint8_t odr, uint8_t bandwidth, uint8_t perf_mode)
{
    int8_t rslt;

    /* Check bandwidth and perf_mode combinations */
    rslt = validate_bandwidth_perfmode(bandwidth, perf_mode);

    if (rslt == BMA4_OK)
    {
        /* Check ODR validity */
        rslt = validate_odr(odr);
    }

    if (rslt == BMA4_OK)
    {
        /* Validate ODR and bandwidth combinations when perf_mode = BMA4_CIC_AVG_MODE */
        if (perf_mode == BMA4_CIC_AVG_MODE)
        {
            rslt = validate_bandwidth_odr(bandwidth, odr);
        }
    }

    return rslt;

}

/*!
 *  @brief This API validates the bandwidth and perf_mode
 *  value set by the user.
 */
static int8_t validate_bandwidth_perfmode(uint8_t bandwidth, uint8_t perf_mode)
{
    int8_t rslt = BMA4_OK;

    if (perf_mode == BMA4_CONTINUOUS_MODE)
    {
        if (bandwidth > BMA4_ACCEL_NORMAL_AVG4)
        {
            /* Invalid bandwidth error for continuous mode */
            rslt = BMA4_E_OUT_OF_RANGE;
        }
    }
    else if (perf_mode == BMA4_CIC_AVG_MODE)
    {
        if (bandwidth > BMA4_ACCEL_RES_AVG128)
        {
            /* Invalid bandwidth error for CIC avg. mode */
            rslt = BMA4_E_OUT_OF_RANGE;
        }
    }
    else
    {
        rslt = BMA4_E_OUT_OF_RANGE;
    }

    return rslt;
}

/*!
 *  @brief This API validates the ODR value set by the user.
 */
static int8_t validate_odr(uint8_t odr)
{
    int8_t rslt = BMA4_OK;

    if ((odr < BMA4_OUTPUT_DATA_RATE_0_78HZ) || (odr > BMA4_OUTPUT_DATA_RATE_1600HZ))
    {
        /* If odr is not valid return error */
        rslt = BMA4_E_OUT_OF_RANGE;
    }

    return rslt;
}

/*!
 *  @brief This API validates the bandwidth and odr when perf_mode = BMA4_CIC_AVG_MODE
 */
static int8_t validate_bandwidth_odr(uint8_t bandwidth, uint8_t odr)
{
    int8_t rslt = BMA4_OK;
    uint8_t index;

    /*
     * Array contains ODR values
     * From BMA4_OUTPUT_DATA_RATE_0_78HZ (value = 1)
     * To BMA4_OUTPUT_DATA_RATE_400HZ (value = 10)
     */
    uint8_t accel_odr[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    /*
     * Array contains valid bandwidth values for each ODR
     */
    uint8_t valid_bw[10] = { 7, 7, 7, 7, 6, 5, 4, 3, 2, 1 };

    /*
     * For ODR - 800Hz and 1600Hz, none of the bandwidth is applicable
     */
    if ((odr == BMA4_OUTPUT_DATA_RATE_800HZ) || (odr == BMA4_OUTPUT_DATA_RATE_1600HZ))
    {
        rslt = BMA4_E_AVG_MODE_INVALID_CONF;
    }

    /*
     * Maximum valid bandwidth value for accel_odr[index] is the
     * corresponding value in valid_bw[index]
     */
    for (index = 0; index < 10; index++)
    {
        if (odr == accel_odr[index])
        {
            if (bandwidth > valid_bw[index])
            {
                rslt = BMA4_E_AVG_MODE_INVALID_CONF;
            }
        }
    }

    return rslt;
}

/*! @endcond */

/*!
 *  @brief This API sets the advance power save mode in the sensor.
 */
int8_t bma4_set_advance_power_save(uint8_t adv_pwr_save, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        rslt = bma4_read_regs(BMA4_POWER_CONF_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            data = BMA4_SET_BITS_POS_0(data, BMA4_ADVANCE_POWER_SAVE, adv_pwr_save);
            rslt = bma4_write_regs(BMA4_POWER_CONF_ADDR, &data, 1, dev);
        }
    }

    return rslt;
}

/*!
 *  @brief This API reads the status of advance power save mode
 *  from the sensor.
 */
int8_t bma4_get_advance_power_save(uint8_t *adv_pwr_save, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (adv_pwr_save != NULL))
    {
        rslt = bma4_read_regs(BMA4_POWER_CONF_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            *adv_pwr_save = BMA4_GET_BITS_POS_0(data, BMA4_ADVANCE_POWER_SAVE);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}


/*!
 *  @brief This API enables or disables the Accel in the sensor.
 */
int8_t bma4_set_accel_enable(uint8_t accel_en, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        rslt = bma4_read_regs(BMA4_POWER_CTRL_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            data = BMA4_SET_BITSLICE(data, BMA4_ACCEL_ENABLE, accel_en);

            rslt = bma4_write_regs(BMA4_POWER_CTRL_ADDR, &data, 1, dev);
            dev->delay_us(450, dev->intf_ptr);
        }
    }

    return rslt;
}

/*!
 *  @brief This API checks whether Accel is enabled or not in the sensor.
 */
int8_t bma4_get_accel_enable(uint8_t *accel_en, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (accel_en != NULL))
    {
        rslt = bma4_read_regs(BMA4_POWER_CTRL_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            *accel_en = BMA4_GET_BITSLICE(data, BMA4_ACCEL_ENABLE);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API reads the data ready status of Accel from the sensor.
 */
int8_t bma4_get_accel_data_rdy(uint8_t *data_rdy, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (data_rdy != NULL))
    {
        /*Reads the status of Accel data ready*/
        rslt = bma4_read_regs(BMA4_STATUS_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            *data_rdy = BMA4_GET_BITSLICE(data, BMA4_STAT_DATA_RDY_ACCEL);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API reads the ASIC status from the sensor.
 *  The status information is mentioned in the below table.
 */
int8_t bma4_get_asic_status(struct bma4_asic_status *asic_status, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (asic_status != NULL))
    {
        /* Read the Mag I2C device address*/
        rslt = bma4_read_regs(BMA4_INTERNAL_ERROR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            asic_status->sleep = (data & 0x01);
            asic_status->irq_ovrn = ((data & 0x02) >> 0x01);
            asic_status->wc_event = ((data & 0x04) >> 0x02);
            asic_status->stream_transfer_active = ((data & 0x08) >> 0x03);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 *  @brief This API enables the offset compensation for filtered and
 *  unfiltered  Accel data.
 */
int8_t bma4_set_offset_comp(uint8_t offset_en, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        rslt = bma4_read_regs(BMA4_NV_CONFIG_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            /* Write Accel FIFO filter data */
            data = BMA4_SET_BITSLICE(data, BMA4_NV_ACCEL_OFFSET, offset_en);
            rslt = bma4_write_regs(BMA4_NV_CONFIG_ADDR, &data, 1, dev);
        }
    }

    return rslt;
}

/*!
 *  @brief This API gets the status of Accel offset compensation
 */
int8_t bma4_get_offset_comp(uint8_t *offset_en, struct bma4_dev *dev)
{
    int8_t rslt;
    uint8_t data = 0;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (offset_en != NULL))
    {
        rslt = bma4_read_regs(BMA4_NV_CONFIG_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            /* Write Accel FIFO filter data */
            *offset_en = BMA4_GET_BITSLICE(data, BMA4_NV_ACCEL_OFFSET);
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*****************************************************************************/
/*! @cond DOXYGEN_SUPRESS */

/* Suppressing doxygen warnings triggered for same static function names present across various sensor variant
 * directories */



/*!
 * @brief This API is used to calculate the power of 2
 */
static int32_t power(int16_t base, uint8_t resolution)
{
    uint8_t i = 1;

    /* Initialize variable to store the power of 2 value */
    int32_t value = 1;

    for (; i <= resolution; i++)
    {
        value = (int32_t)(value * base);
    }

    return value;
}



/*!
 * @brief This internal API checks null pointer error
 */
static int8_t null_pointer_check(const struct bma4_dev *dev)
{
    int8_t rslt = BMA4_OK;

    if ((dev == NULL) || (dev->bus_read == NULL) || (dev->bus_write == NULL) || (dev->intf_ptr == NULL))
    {
        rslt = BMA4_E_NULL_PTR;
    }
    else
    {
        rslt = BMA4_OK;
    }

    return rslt;
}

/*! @endcond */

/*!
 *  @brief This API does soft reset
 */
int8_t bma4_soft_reset(struct bma4_dev *dev)
{
    int8_t rslt;

    /* Variable to read the dummy byte */
    uint8_t dummy_read = 0;

    /* Variable contains soft reset command */
    uint8_t command_reg = BMA4_SOFT_RESET;

    /* Check the dev structure as NULL */
    rslt = null_pointer_check(dev);

    /* Check the bma4 structure as NULL */
    if (rslt == BMA4_OK)
    {
        /* Write command register */
        rslt = bma4_write_regs(BMA4_CMD_ADDR, &command_reg, 1, dev);

        if (rslt == BMA4_OK)
        {
            if (dev->intf == BMA4_SPI_INTF)
            {
                /* Dummy read to bring interface to SPI */
                rslt = bma4_read_regs(BMA4_CHIP_ID_ADDR, &dummy_read, 1, dev);
            }
        }
    }

    return rslt;
}

/*!
 * @brief This API performs Fast Offset Compensation for accelerometer.
 */
int8_t bma4_perform_accel_foc(const struct bma4_accel_foc_g_value *accel_g_value, struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Structure to define the accelerometer configurations */
    struct bma4_accel_config acc_cfg = { 0, 0, 0, 0 };

    /* Variable to store status of advance power save */
    uint8_t aps = 0;

    /* Variable to store status of accelerometer enable */
    uint8_t acc_en = 0;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (accel_g_value != NULL))
    {
        /* Check for input validity */
        if (((ABS(accel_g_value->x) + ABS(accel_g_value->y) + ABS(accel_g_value->z)) == 1) &&
            ((accel_g_value->sign == 1) || (accel_g_value->sign == 0)))
        {
            /* Save accelerometer configurations, accelerometer
             * enable status and advance power save status
             */
            rslt = save_accel_foc_config(&acc_cfg, &aps, &acc_en, dev);

            /* Set configurations for FOC */
            if (rslt == BMA4_OK)
            {
                rslt = set_accel_foc_config(dev);
            }

            /* Perform accelerometer FOC */
            if (rslt == BMA4_OK)
            {
                rslt = perform_accel_foc(accel_g_value, &acc_cfg, dev);
            }

            /* Restore the saved configurations */
            if (rslt == BMA4_OK)
            {
                rslt = restore_accel_foc_config(&acc_cfg, aps, acc_en, dev);
            }
        }
        else
        {
            rslt = BMA4_E_OUT_OF_RANGE;
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API saves the configurations before performing FOC.
 */
static int8_t save_accel_foc_config(struct bma4_accel_config *acc_cfg,
                                    uint8_t *aps,
                                    uint8_t *acc_en,
                                    struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Variable to get the status from PWR_CTRL register */
    uint8_t pwr_ctrl_data = 0;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (acc_cfg != NULL) && (aps != NULL) && (acc_en != NULL))
    {
        /* Get accelerometer configurations to be saved */
        rslt = bma4_get_accel_config(acc_cfg, dev);
        if (rslt == BMA4_OK)
        {
            /* Get accelerometer enable status to be saved */
            rslt = bma4_read_regs(BMA4_POWER_CTRL_ADDR, &pwr_ctrl_data, 1, dev);
            if (rslt == BMA4_OK)
            {
                *acc_en = BMA4_GET_BITSLICE(pwr_ctrl_data, BMA4_ACCEL_ENABLE);
            }

            /* Get advance power save mode to be saved */
            if (rslt == BMA4_OK)
            {
                rslt = bma4_get_advance_power_save(aps, dev);
            }
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API sets configurations for performing accelerometer FOC.
 */
static int8_t set_accel_foc_config(struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Variable to set the accelerometer configuration value */
    uint8_t acc_conf_data = BMA4_FOC_ACC_CONF_VAL;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        /* Disabling offset compensation */
        rslt = set_bma4_accel_offset_comp(BMA4_DISABLE, dev);
        if (rslt == BMA4_OK)
        {
            /* Set accelerometer configurations to 50Hz, continuous mode, CIC mode */
            rslt = bma4_write_regs(BMA4_ACCEL_CONFIG_ADDR, &acc_conf_data, 1, dev);
            if (rslt == BMA4_OK)
            {
                /* Set accelerometer to normal mode by enabling it */
                rslt = bma4_set_accel_enable(BMA4_ENABLE, dev);
                if (rslt == BMA4_OK)
                {
                    /* Disable advance power save mode */
                    rslt = bma4_set_advance_power_save(BMA4_DISABLE, dev);
                }
            }
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API enables/disables the offset compensation for
 * filtered and un-filtered accelerometer data.
 */
static int8_t set_bma4_accel_offset_comp(uint8_t offset_en, struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Variable to store data */
    uint8_t data = 0;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        /* Enable/Disable offset compensation */
        rslt = bma4_read_regs(BMA4_NV_CONFIG_ADDR, &data, 1, dev);
        if (rslt == BMA4_OK)
        {
            data = BMA4_SET_BITSLICE(data, BMA4_NV_ACCEL_OFFSET, offset_en);
            rslt = bma4_write_regs(BMA4_NV_CONFIG_ADDR, &data, 1, dev);
        }
    }

    return rslt;
}

/*!
 * @brief This internal API performs Fast Offset Compensation for accelerometer.
 */
static int8_t perform_accel_foc(const struct bma4_accel_foc_g_value *accel_g_value,
                                const struct bma4_accel_config *acc_cfg,
                                struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Variable to define count */
    uint8_t loop;

    /* Variable to store status read from the status register */
    uint8_t reg_status = 0;

    /* Array of structure to store accelerometer data */
    struct bma4_accel accel_value[128] = { { 0 } };

    /* Structure to store accelerometer data temporarily */
    struct bma4_foc_temp_value temp = { 0, 0, 0 };

    /* Structure to store the average of accelerometer data */
    struct bma4_accel accel_avg = { 0, 0, 0 };

    /* Variable to define LSB per g value */
    uint16_t lsb_per_g = 0;

    /* Variable to define range */
    uint8_t range = 0;

    /* Variable to set limit for FOC sample */
    uint8_t limit = 128;

    /* Structure to store accelerometer data deviation from ideal value */
    struct bma4_offset_delta delta = { 0, 0, 0 };

    /* Structure to store accelerometer offset values */
    struct bma4_accel_offset offset = { 0, 0, 0 };

    /* Variable tries max 5 times for interrupt then generates timeout */
    uint8_t try_cnt;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (accel_g_value != NULL) && (acc_cfg != NULL))
    {
        for (loop = 0; loop < limit; loop++)
        {
            try_cnt = 5;
            while (try_cnt && (!(reg_status & BMA4_STAT_DATA_RDY_ACCEL_MSK)))
            {
                /* 20ms delay for 50Hz ODR */
                dev->delay_us(BMA4_MS_TO_US(20), dev->intf_ptr);
                rslt = bma4_get_status(&reg_status, dev);
                try_cnt--;
            }

            if ((rslt == BMA4_OK) && (reg_status & BMA4_STAT_DATA_RDY_ACCEL_MSK))
            {
                rslt = bma4_read_accel_xyz(&accel_value[loop], dev);
            }

            if (rslt == BMA4_OK)
            {
                rslt = bma4_read_accel_xyz(&accel_value[loop], dev);
            }

            if (rslt == BMA4_OK)
            {
                /* Store the data in a temporary structure */
                temp.x = temp.x + (int32_t)accel_value[loop].x;
                temp.y = temp.y + (int32_t)accel_value[loop].y;
                temp.z = temp.z + (int32_t)accel_value[loop].z;
            }
            else
            {
                break;
            }
        }

        if (rslt == BMA4_OK)
        {
            /* Take average of x, y and z data for lesser noise */
            accel_avg.x = (int16_t)(temp.x / 128);
            accel_avg.y = (int16_t)(temp.y / 128);
            accel_avg.z = (int16_t)(temp.z / 128);

            /* Get the exact range value */
            map_accel_range(acc_cfg->range, &range);

            /* Get the smallest possible measurable acceleration level given the range and
             * resolution */
            lsb_per_g = (uint16_t)(power(2, dev->resolution) / (2 * range));

            /* Compensate acceleration data against gravity */
            comp_for_gravity(lsb_per_g, accel_g_value, &accel_avg, &delta);

            /* Scale according to offset register resolution */
            scale_bma4_accel_offset(range, &delta, &offset, dev->resolution);

            /* Invert the accelerometer offset data */
            invert_bma4_accel_offset(&offset);

            /* Write offset data in the offset compensation register */
            rslt = write_bma4_accel_offset(&offset, dev);

            /* Enable offset compensation */
            if (rslt == BMA4_OK)
            {
                rslt = set_bma4_accel_offset_comp(BMA4_ENABLE, dev);
            }
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}

/*!
 * @brief This internal API converts the accelerometer range value into
 * corresponding integer value.
 */
static void map_accel_range(uint8_t range_in, uint8_t *range_out)
{
    switch (range_in)
    {
        case BMA4_ACCEL_RANGE_2G:
            *range_out = 2;
            break;
        case BMA4_ACCEL_RANGE_4G:
            *range_out = 4;
            break;
        case BMA4_ACCEL_RANGE_8G:
            *range_out = 8;
            break;
        case BMA4_ACCEL_RANGE_16G:
            *range_out = 16;
            break;
        default:

            /* By default RANGE 4G is set */
            *range_out = 4;
            break;
    }
}

/*!
 * @brief This internal API compensate the accelerometer data against gravity.
 */
static void comp_for_gravity(uint16_t lsb_per_g,
                             const struct bma4_accel_foc_g_value *g_val,
                             const struct bma4_accel *data,
                             struct bma4_offset_delta *comp_data)
{
    /* Array to store the accelerometer values in LSB */
    int16_t accel_value_lsb[3] = { 0 };

    /* Convert g-value to LSB */
    accel_value_lsb[BMA4_X_AXIS] = (int16_t)(lsb_per_g * g_val->x);
    accel_value_lsb[BMA4_Y_AXIS] = (int16_t)(lsb_per_g * g_val->y);
    accel_value_lsb[BMA4_Z_AXIS] = (int16_t)(lsb_per_g * g_val->z);

    /* Get the compensated values for X, Y and Z axis */
    comp_data->x = (data->x - accel_value_lsb[BMA4_X_AXIS]);
    comp_data->y = (data->y - accel_value_lsb[BMA4_Y_AXIS]);
    comp_data->z = (data->z - accel_value_lsb[BMA4_Z_AXIS]);
}

/*!
 * @brief This internal API scales the compensated accelerometer data according
 * to the offset register resolution.
 */
static void scale_bma4_accel_offset(uint8_t range,
                                    const struct bma4_offset_delta *comp_data,
                                    struct bma4_accel_offset *data,
                                    uint8_t resolution)
{
    /* Variable to store the position of bit having 3.9mg resolution */
    int8_t bit_pos_3_9mg;

    /* Variable to store the position previous of bit having 3.9mg resolution */
    int8_t bit_pos_3_9mg_prev_bit = 0;

    /* Variable to store the round-off value */
    uint8_t round_off = 0;

    /* Find the bit position of 3.9mg */
    bit_pos_3_9mg = get_bit_pos_3_9mg(range, resolution);

    if (bit_pos_3_9mg < 0)
    {
        bit_pos_3_9mg = 0;
    }

    /* Round off, consider if the next bit is high */
    if (bit_pos_3_9mg > 0)
    {
        bit_pos_3_9mg_prev_bit = bit_pos_3_9mg - 1;
        round_off = (uint8_t)(power(2, ((uint8_t) bit_pos_3_9mg_prev_bit)));
    }

    /* Scale according to offset register resolution */
    data->x = (uint8_t)((comp_data->x + round_off) / power(2, ((uint8_t) bit_pos_3_9mg)));
    data->y = (uint8_t)((comp_data->y + round_off) / power(2, ((uint8_t) bit_pos_3_9mg)));
    data->z = (uint8_t)((comp_data->z + round_off) / power(2, ((uint8_t) bit_pos_3_9mg)));
}

/*!
 * @brief This internal API inverts the accelerometer offset data.
 */
static void invert_bma4_accel_offset(struct bma4_accel_offset *offset_data)
{
    /* Get the offset data */
    offset_data->x = (uint8_t)((offset_data->x) * (-1));
    offset_data->y = (uint8_t)((offset_data->y) * (-1));
    offset_data->z = (uint8_t)((offset_data->z) * (-1));
}

/*!
 * @brief This internal API writes the offset data in the offset compensation
 * register.
 */
static int8_t write_bma4_accel_offset(const struct bma4_accel_offset *offset, struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Array to store the offset data */
    uint8_t data_array[3] = { 0 };

    data_array[0] = offset->x;
    data_array[1] = offset->y;
    data_array[2] = offset->z;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if (rslt == BMA4_OK)
    {
        /* Offset values are written in the offset register */
        rslt = bma4_write_regs(BMA4_OFFSET_0_ADDR, data_array, 3, dev);
    }

    return rslt;
}

/*!
 * @brief This internal API finds the bit position of 3.9mg according to given
 * range and resolution.
 */
static int8_t get_bit_pos_3_9mg(uint8_t range, uint8_t resolution)
{
    /* Variable to store the bit position of 3.9mg resolution */
    int8_t bit_pos_3_9mg;

    /* Variable to shift the bits according to the resolution  */
    uint32_t divisor = 1;

    /* Scaling factor to get the bit position of 3.9 mg resolution */
    int16_t scale_factor = -1;

    /* Variable to store temporary value */
    uint16_t temp;

    /* Shift left by the times of resolution */
    divisor = divisor << (resolution - 1);

    /* Get the bit position to be shifted */
    temp = (uint16_t)(divisor / (range * 128));

    /* Get the scaling factor until bit position is shifted to last bit */
    while (temp != 1)
    {
        scale_factor++;
        temp = temp >> 1;
    }

    /* Scaling factor is the bit position of 3.9 mg resolution */
    bit_pos_3_9mg = (int8_t) scale_factor;

    return bit_pos_3_9mg;
}

/*!
 * @brief This internal API restores the configurations saved before performing
 * accelerometer FOC.
 */
static int8_t restore_accel_foc_config(const struct bma4_accel_config *acc_cfg,
                                       uint8_t aps,
                                       uint8_t acc_en,
                                       struct bma4_dev *dev)
{
    /* Variable to define error */
    int8_t rslt;

    /* Variable to get the status from PWR_CTRL register */
    uint8_t pwr_ctrl_data = 0;

    /* NULL pointer check */
    rslt = null_pointer_check(dev);

    if ((rslt == BMA4_OK) && (acc_cfg != NULL))
    {
        /* Restore the saved accelerometer configurations */
        rslt = bma4_set_accel_config(acc_cfg, dev);
        if (rslt == BMA4_OK)
        {
            /* Restore the saved accelerometer enable status */
            rslt = bma4_read_regs(BMA4_POWER_CTRL_ADDR, &pwr_ctrl_data, 1, dev);
            if (rslt == BMA4_OK)
            {
                pwr_ctrl_data = BMA4_SET_BITSLICE(pwr_ctrl_data, BMA4_ACCEL_ENABLE, acc_en);
                rslt = bma4_write_regs(BMA4_POWER_CTRL_ADDR, &pwr_ctrl_data, 1, dev);

                /* Restore the saved advance power save */
                if (rslt == BMA4_OK)
                {
                    rslt = bma4_set_advance_power_save(aps, dev);
                }
            }
        }
    }
    else
    {
        rslt = BMA4_E_NULL_PTR;
    }

    return rslt;
}
