/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __AMS_ERRNO_H__
#define __AMS_ERRNO_H__

/* The typedef enum must match the order of the strings defined */
/* in ams_errno.c --> static const char* const ams_errno_str[] */
typedef enum 
{
    /* No error */
    AMS_SUCCESS    = 0,

    /* CLI Command failed to process */
    AMS_CLI_FAILURE    ,

    /* Error Initing i2c */
    AMS_I2C_INIT_ERROR ,

    /* AMS Device init failure */
    AMS_DEVICE_INIT_FAILURE,

    /* Cannot validate device */
    AMS_DEVICE_VALIDATE_ERROR,

    /* Null pointer */
    AMS_DEVICE_NULL_PTR, 

    /* current software not supported index of device */
    AMS_DEVICE_OUT_OF_BOUNDS,

    /* FIFO not empty */
    AMS_DEVICE_FIFO_NOT_EMPTY,

    /* Serial number of device does not match the one specified in calibration header */
    AMS_INVALID_CALIBRATION_DATA,

    /* FIFO End Marker not present */
    AMS_NO_END_MARKER,

    /* Unable to decode compressed flicker data */
    AMS_COMPRESS_DECODE_FAILURE, 

    /* Cannot calculate a flicker frequency */
    AMS_FLICKER_FAILURE,

    /* ams fft could not be calculated */
    AMS_FFT_FAILURE,

    /* */
    AMS_UNKNOWN_ERROR,

    /* */
    AMS_LAST_ERROR = AMS_UNKNOWN_ERROR + 1,
} ams_errno_t;

const char *ams_errno_code_2_str(ams_errno_t err);

#endif /* __AMS_ERRNO_H__ */
