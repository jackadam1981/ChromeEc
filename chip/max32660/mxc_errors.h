/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 List of common error return codes */

#ifndef _ERRORS_H_
#define _ERRORS_H_

/**
 * @ingroup syscfg
 * @defgroup MXC_Error_Codes Error Codes
 * @brief      A list of common error codes used by the API.
 * @note       A Negative Error Convention is used to avoid conflict with
 *             positive, Non-Error, returns.
 * @{
 */

/** No Error */
#define E_NO_ERROR 0
/** No Error, success */
#define E_SUCCESS 0
/** Pointer is NULL */
#define E_NULL_PTR -1
/** No such device */
#define E_NO_DEVICE -2
/** Parameter not acceptable */
#define E_BAD_PARAM -3
/** Value not valid or allowed */
#define E_INVALID -4
/** Module not initialized */
#define E_UNINITIALIZED -5
/** Busy now, try again later */
#define E_BUSY -6
/** Operation not allowed in current state */
#define E_BAD_STATE -7
/** Generic error */
#define E_UNKNOWN -8
/** General communications error */
#define E_COMM_ERR -9
/** Operation timed out */
#define E_TIME_OUT -10
/** Expected response did not occur */
#define E_NO_RESPONSE -11
/** Operations resulted in unexpected overflow */
#define E_OVERFLOW -12
/** Operations resulted in unexpected underflow */
#define E_UNDERFLOW -13
/** Data or resource not available at this time */
#define E_NONE_AVAIL -14
/** Event was shutdown */
#define E_SHUTDOWN -15
/** Event was aborted */
#define E_ABORT -16
/** The requested operation is not supported */
#define E_NOT_SUPPORTED -17
/**@} end of MXC_Error_Codes group */

#endif /* _ERRORS_H_ */
