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
#include "tcs3410_fifo.h"
#include "tcs3410_als.h"
#include "tcs3410_fd.h"
#include "ams_device.h"

static uint8_t fifo_data[MAX_FIFO_DATA_LEN];
/*
 *  3 bytes after flicker data are the end marker.  They must be 0.
 *
 *  The gains are located after the end marker
 */
static bool check_for_end_marker(uint8_t *pbuffer, uint32_t len)
{
    bool ret_val = false;

    if ((pbuffer[len]) == 0 &&
        (pbuffer[len + 1]  == 0) &&
        (pbuffer[len + 2]  == 0)
       )
    {
        ret_val = true;
    }

    return(ret_val);
}

ams_errno_t sensor_read_fifo(uint8_t *shadow_regs, volatile ams_current_state_t *pcurr_state)
{
    uint8_t overflow = 0, underflow = 0;
    uint32_t i2c_size, level;
    ams_errno_t ret_val = AMS_SUCCESS;
    static uint8_t *pfifo = &fifo_data[0];

    /* Read the FIFO Level and status registers */
    sensor_read(REG_FIFO_LEVEL, &shadow_regs[REG_FIFO_LEVEL], 2);
    pcurr_state->fifo.level = ((uint16_t)shadow_regs[REG_FIFO_LEVEL] << FIFO_LEVEL1_SHIFT) |
            ((uint16_t)shadow_regs[REG_FIFO_STATUS0] & FIFO_STATUS0_LVL0_MASK);
    level = pcurr_state->fifo.level;

    /* Keep track of running total for sequence round */
    pcurr_state->fifo.total_len += level;

    pcurr_state->fifo.threshold = ((shadow_regs[REG_FIFO_THR] << FIFO_THRESH1_SHIFT) |
                                   (shadow_regs[REG_CFG2] & CFG2_FIFO_THRESH0_MASK));

    AMS_LOG_PRINTF_IRQ(LOG_INFO, "FIFO Level = %d   FIFO Threshold = %d", level,  pcurr_state->fifo.threshold);
    /* Determine overflow and underflow status bits */
    underflow = ((shadow_regs[REG_FIFO_STATUS0] & FIFO_STATUS0_UNDERFLOW_MASK) >> FIFO_STATUS0_UNDERFLOW_SHIFT);
    pcurr_state->fifo.underflow += underflow;
    overflow = ((shadow_regs[REG_FIFO_STATUS0] & FIFO_STATUS0_OVERFLOW_MASK)   >> FIFO_STATUS0_OVERFLOW_SHIFT);
    pcurr_state->fifo.overflow += overflow;

    /* Read the data from the fifo */
    /* Nordic may have a max i2c transaction length in bytes - set a max to 32 bytes for safety */
    if (pcurr_state->fifo.fifo_state == AMS_FIFO_IDLE)
    {
        /* Fresh capture - init to all 0 and start from index 0 */
        memset(&fifo_data, 0x0, sizeof(fifo_data));
        pfifo = &fifo_data[0];
    }
    else if (pcurr_state->fifo.fifo_state == AMS_FIFO_IN_PROGRESS )
    {
        /* Add current data to exisiting data in buffer          */
        /* pfifo is pointing to the next available data location */
        /* from previous fifo read                               */
    }

    while (level > 0)
    {
        /* Determine packet size - limited by I2C interface */
        if (level >= MAX_I2C_READ_LEN)
        {
            i2c_size = MAX_I2C_READ_LEN;  /* usually limited to 32 bytes */
        }
        else
        {
            i2c_size = level;
        }

        sensor_read(REG_FIFO_DATA, (uint8_t *)pfifo, i2c_size);
        pfifo += i2c_size;
        level -= i2c_size;
    }

    return (ret_val);
 }

ams_errno_t sensor_process_fifo(uint8_t *shadow_regs, volatile ams_current_state_t *pcurr_state)
{
    uint32_t data_len, fd_len;
    ams_errno_t ret_val = AMS_SUCCESS;
    uint8_t *pfifo;

    data_len = pcurr_state->fifo.total_len;

    pfifo = &fifo_data[0];

    /* If ALS is enabled - parse, the counts, gains and status */
    if (pcurr_state->features[AMS_FEATURE_ALS] == AMS_FEATURE_ENABLE)
    {
        /* Perform the lux, etc calculations */
        process_als_data(pcurr_state, pfifo);

        /* remove the als data from the overall fifo length */
        data_len = pcurr_state->fifo.total_len - ALS_DATA_SZ;

        /* indicate the start of the flicker data */
        pfifo = &fifo_data[ALS_DATA_SZ];

        ret_val = AMS_SUCCESS;
    }

    /* If flicker data is enabled, parse the flicker end marker and gains */
    /* It is configured for compressed difference - 5 bits (7 total: 1 for base 0, 1 for HW bit) */
    if (pcurr_state->features[AMS_FEATURE_FLICKER] == AMS_FEATURE_ENABLE)
    {
        /* Flicker data starts at byte 12 (13nd byte) */
        /* Byte stream for Flicker Data               */
        /* 1. Flicker data                            */
        /* 2. 3 byte end marker 0's                   */
        /* 3. 2 bytes of gain - only 1 modulator  used*/
        /*    Modulator 0                             */
        /*    Only need 4 bits                        */
        /*  | end marker 0 | end marker 1 | end marker 2 | gain[1, 0] | gain[dc,2 ]| */
        /* gain 1 is upper 4 bits, gain 0 lower 4 bits */
        fd_len = data_len - END_MARKER_SZ - FLCKR_GAIN_SZ;

        if (check_for_end_marker(pfifo, fd_len))
        {
            pcurr_state->fd.end_marker = true;
            pcurr_state->fd.len = fd_len;
            pcurr_state->fd.gain_reg = pfifo[data_len - FD_GAIN0_1_OFFSET] & 0x0F;

            process_fd_data(pcurr_state, pfifo, fd_len);

            ret_val = AMS_SUCCESS;
        }
        else
        {
            pcurr_state->fd.end_marker = false;
            pcurr_state->fd.freq       = 0.0;
            AMS_LOG_PRINTF_IRQ(LOG_ERROR, "FIFO End Marker Not Detected");
            ret_val = AMS_NO_END_MARKER;
        }
    }

    pcurr_state->fifo.total_len = 0;
    pcurr_state->fifo.fifo_state = AMS_FIFO_IDLE;

    return (ret_val);
}

ams_errno_t sensor_fifo_reset(void)
{
    uint8_t i2c_data[2];
    ams_errno_t ret_val = AMS_SUCCESS;
    uint16_t level = 0;
    volatile ams_current_state_t *pcurr_state = sensor_get_current_state();

    /* clears the fifo */
    sensor_modify(REG_CONTROL, sh, CONTROL_FIFO_CLR_MASK, CONTROL_FIFO_CLR_MASK);

    /* confirm the fifo level is 0 */
    sensor_read(REG_FIFO_LEVEL, i2c_data, 2);
    level = ((i2c_data[0] << FIFO_LEVEL1_SHIFT) |
            (i2c_data[1] & FIFO_STATUS0_LVL0_MASK));

    if (level != 0)
    {
        ret_val = AMS_DEVICE_FIFO_NOT_EMPTY;
        AMS_LOG_PRINTF(LOG_ERROR, "FIFO Cleared but FIFO_LVL != 0");
    }
    else
    {
        pcurr_state->fifo.total_len = 0;
        pcurr_state->fifo.fifo_state = AMS_FIFO_IDLE;
        AMS_LOG_PRINTF(LOG_INFO, "FIFO Reset Complete");
    }

    return(ret_val);
}
