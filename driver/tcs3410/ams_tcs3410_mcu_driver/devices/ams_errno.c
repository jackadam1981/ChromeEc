/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ams_errno.h"

/* This must match the order defined in ams_errno.h */
static const char* const ams_errno_str[] =
{
    "AMS_SUCCESS",
    "AMS_CLI_FAILURE",
    "AMS_I2C_INIT_ERROR",
    "AMS_DEVICE_INIT_FAILURE",
    "AMS_DEVICE_FAILED_VALIDATION",
    "AMS_DEVICE_NULL_PTR",
    "AMS_DEVICE_OUT_OF_BOUNDS",
    "AMS_DEVICE_FIFO_NOT_EMPTY",
    "AMS_INVALID_CALIBRATION_DATA",
    "AMS_NO_END_MARKER",
    "AMS_COMPRESS_DECODE_FAILURE",
    "AMS_FLICKER_FAILURE",
    "AMS_FFT_FAILURE",
    "AMS_UNKNOWN_ERROR",
};

const char *ams_errno_code_2_str(ams_errno_t err)
{
    const char *err_str = NULL;
    if (err < AMS_LAST_ERROR)
    {
        err_str = ams_errno_str[err];
    }

    return(err_str);
}
