/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

/*
 *  DESCRIPTION:
 *
 *  Helper utility to convert unsigned 16 bit numbers to signed 16 bits numbers
 *  to be processed by ams' fft algorithm.  Most ams devices produce unsigned
 *  16 bit input data.  However, the fft algorithm requires signed 16 bit data.
 *  If the unsigned data is greater than 32767 (0x7FFF), the "sign" bit will be
 *  set and cause undesirable results within the FFT algorithm.  Use this
 *  utility to normalize the data so that the input data is not greater than 
 *  32767.
 *
 */
 
#include "ams_helper.h"

int ams_normalize_fft_data(uint16_t *input, uint16_t *output, uint32_t size)
{
    uint32_t idx;
    uint16_t max = 0;
    
    if ((input == NULL) || (output == NULL))
    {
        return(-1);
    }

    /* Find the max value from within the input data */
    for (idx = 0; idx < size; idx++)
    {
        if (input[idx] > max)
        {
            max = input[idx];
        }
    }

    /* Normalize the unsigned data by max/2 to fit within a 16 signed int */
    for (idx = 0; idx < size; idx++)
    {
        output[idx] = (input[idx] - (max / 2));
    }

    return(0);
}
