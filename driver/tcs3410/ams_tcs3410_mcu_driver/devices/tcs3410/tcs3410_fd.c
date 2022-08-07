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
#include <string.h>
#include <math.h>
#include "ams_errno.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"
#include "tcs3410_fd.h"
#include "tcs3410_utils.h"
#include "ams_device.h"
#include "ams_fifo_decode.h"
#include "ams_fft.h"

static int calculate_fft(uint16_t *in, uint16_t *out, uint16_t fd_nr_samples)
{
    int ret = 0;

    if ( ams_rfft((int16_t *)in, fd_nr_samples))
    {
        ams_get_magnitude((int16_t *)in, (uint16_t *)out, fd_nr_samples/2);
        ret =  (fd_nr_samples/2);
    }
    else
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "ams_rfft: Failure: return value of 0");
        ret = 0;
    }

    return(ret);
}

static uint32_t get_sqrt(uint32_t x)
{
    uint32_t result;
    uint32_t tmp;

    result = 0;
    tmp = (1 << 30);
    while (tmp > x)
    {
        tmp >>= 2;
    }

    while (tmp != 0)
    {
        if (x >= (result + tmp))
        {
            x -= result + tmp;
            result += 2 * tmp;
        }
        result >>= 1;
        tmp >>= 2;
    }
    return result;
}

static int get_mean(uint16_t *buff, int size)
{
    int i;
    int sum = 0;

    for (i = 0; i < size; i++)
    {
        sum += buff[i];
    }
    return sum / size;
}

static int get_max(uint16_t *buff, int size)
{
    int i;
    int max = 0;
    int max_index = 0;

    for (i = 0; i < size; i++)
    {
        if (buff[i] > max)
        {
            max = buff[i];
            max_index = i;
        }
    }
    return(max_index);
}

static int get_std_dev(uint16_t *buff, int mean, int size)
{
    int i;
    uint32_t sum = 0;

    for (i = 0; i < size; i++)
    {
        sum += ((buff[i] - mean) * (buff[i] - mean));
    }
    sum = sum / (size - 1);
    return(get_sqrt(sum));
}

ams_errno_t process_fd_data(volatile ams_current_state_t *pcurr_state, uint8_t *pbuffer, uint32_t len)
{
    ams_errno_t ret_val = AMS_SUCCESS;
    uint16_t    input_fft[FFT_MAX_SAMPLE_SIZE];
    uint16_t    output_fft[FFT_MAX_SAMPLE_SIZE];
    int         packet_size[] = {FD_PACKET_COMPRESSED_SZ};
    uint16_t    max_index = 0;
    int32_t     mean, std_dev;
    fp_t        flicker_freq = INT_TO_FP(0);
    uint16_t    fd_nr_samples = pcurr_state->fd.fd_nr_samples + 1; /* register value is base 0 */

    /* With a single channel on a single step performing flicker, no need to normalize */
    /* data with gains */
    pcurr_state->fd.gain_mod = gain_reg_2_gain[pcurr_state->fd.gain_reg];

    /* Decode the difference compressed flicker data */
    /* Assumes compressed difference mode with length set to 5 bits */
    if (fifo_data_decode((void *)pbuffer, (void *)input_fft, 1, len, fd_nr_samples, packet_size, FIFO_FORMAT_DIFFERENCE_COMPRESSED) != FIFO_DECODE_SUCCESS)
    {
        AMS_LOG_PRINTF_IRQ(LOG_INFO, "Failure to Decode FIFO data...");
        return(AMS_COMPRESS_DECODE_FAILURE);
    }

    /* Calculate the fft and determine peak bin */
    memset(output_fft, 0, sizeof(output_fft));
    if (calculate_fft(input_fft, output_fft, fd_nr_samples))
    {
        /* Eliminate DC offset/component of signal - usually the highest content */
        output_fft[0] = 0;

        /* search for the highest magnitude - index will reflect frequency */
        max_index = get_max(output_fft, fd_nr_samples/2);

        /* calculate mean and std_dev to provide confidence in peak - 6 std_devs */
        mean = get_mean(output_fft, fd_nr_samples/2);
        std_dev = get_std_dev(output_fft, mean, fd_nr_samples/2);

        /* sample_freq/fd_nr_samples is the size of each bin */
        /* find the correct bin using max_index */
        flicker_freq = fp_div(max_index * pcurr_state->fd.sample_freq, fd_nr_samples);

        pcurr_state->fd.freq = flicker_freq;
        /* See if peak is at least 6 standard deviations from mean */
        if (output_fft[max_index] > (mean + (std_dev * 6)))
        {
            AMS_LOG_PRINTF_IRQ(LOG_INFO, "Success: Calculated Flicker Frequency is %d mHz. (count = %d, mean = %d, std dev = %d)",
                      FP_TO_INT(flicker_freq * 1000), output_fft[max_index], mean, std_dev);
            ret_val  = AMS_SUCCESS;
        }
        else
        {
            AMS_LOG_PRINTF_IRQ(LOG_ERROR, "FAILURE: Flicker Frequency is %d mHz., not outside of 6 std devs.", FP_TO_INT(flicker_freq * 1000));
            ret_val = AMS_FLICKER_FAILURE;
        }
    }
    else
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "FFT Failed");
        ret_val = AMS_FFT_FAILURE;
    }

    return(ret_val);
}

