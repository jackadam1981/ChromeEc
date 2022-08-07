/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "ams_errno.h"
#include "ams_device.h"

/*
 * Convert up to 8 bytes into a long long
 */
uint64_t convert_bytes_to_ll(uint8_t *source, uint16_t length)
{
    uint16_t idx;
    uint64_t result = 0;
    uint16_t shifter= 0;

    if (length > sizeof(uint64_t))
    {
        return(-1);
    }

    for (idx = 0; idx < length; idx++)
    {
        result |= (uint64_t)((uint64_t)source[idx] << shifter);
        shifter += 8;
    }

    return(result);
}
