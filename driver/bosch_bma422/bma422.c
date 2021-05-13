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
 * @file       bma422.c
 * @date       2020-03-01
 * @version    V2.19.0
 *
 */

/*! \file bma422.c
 * \brief Sensor Driver for BMA422 sensor
 */

#include "bma422.h"

/***************************************************************************/

/**\name        Function definitions
 ****************************************************************************/

/*!
 *  @brief This API is the entry point.
 *  Call this API before using all other APIs.
 *  This API reads the chip-id of the sensor and sets the resolution.
 */
int8_t bma422_init(struct bma4_dev *dev)
{
    int8_t rslt;

    /* Structure to define the default values for axes re-mapping */
//    struct bma4_axes_remap axes_remap = {
//        .x_axis = BMA4_MAP_X_AXIS, .x_axis_sign = BMA4_MAP_POSITIVE, .y_axis = BMA4_MAP_Y_AXIS,
//        .y_axis_sign = BMA4_MAP_POSITIVE, .z_axis = BMA4_MAP_Z_AXIS, .z_axis_sign = BMA4_MAP_POSITIVE
//    };

    rslt = bma4_init(dev);
    if (rslt == BMA4_OK)
    {
        if (dev->chip_id == BMA422_CHIP_ID)
        {
            /* Resolution of BMA422 sensor is 12 bit */
            dev->resolution = BMA4_12_BIT_RESOLUTION;

//            dev->feature_len = BMA422_FEATURE_SIZE;

//            dev->config_size = sizeof(bma422_config_file);

            /* Set the default values for axis
             *  re-mapping in the device structure
             */
//            dev->remap = axes_remap;
        }
        else
        {
            rslt = BMA4_E_INVALID_SENSOR;
        }
    }

    return rslt;
}


/*! @endcond */
