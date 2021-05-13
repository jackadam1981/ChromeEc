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
 * @file       bma422.h
 * @date       2020-03-01
 * @version    V2.19.0
 *
 */

/**
 * \ingroup bma4xy
 * \defgroup bma422 BMA422
 * @brief Sensor driver for BMA422 sensor
 */

#ifndef BMA422_H
#define BMA422_H

#ifdef __cplusplus
extern "C" {
#endif

#include "accelgyro.h"
#include "common.h"
#include "driver/accel_bma422_public.h"

#include "bma4.h"

#define BMA_GET_DATA(_s) \
	((struct bma422_accel_drv_data *)(_s)->drv_data)

#define BMA_GET_SAVED_DATA(_s) \
	(&BMA_GET_DATA(_s)->saved_data)

#define BMA4_FOC_TARGET_POSITIVE_1G    UINT8_C(0)
#define BMA4_FOC_TARGET_NEGATIVE_1G    UINT8_C(1)


/**\name Chip ID of BMA422 sensor */
#define BMA422_CHIP_ID                    UINT8_C(0x12)

/**
 * \ingroup bma422
 * \defgroup bma422ApiInit Initialization
 * @brief Initialize the sensor and device structure
 */

/*!
 * \ingroup bma422ApiInit
 * \page bma422_api_bma422_init bma422_init
 * \code
 * int8_t bma422_init(struct bma4_dev *dev);
 * \endcode
 * @details This API is the entry point.
 *  Call this API before using all other APIs.
 *  This API reads the chip-id of the sensor and sets the resolution.
 *
 *  @param[in,out] dev : Structure instance of bma4_dev
 *
 *  @return Result of API execution status
 *  @retval 0 -> Success
 *  @retval < 0 -> Fail
 */
int8_t bma422_init(struct bma4_dev *dev);


#ifdef __cplusplus
}
#endif /*End of CPP guard */

#endif /*End of header guard macro */
