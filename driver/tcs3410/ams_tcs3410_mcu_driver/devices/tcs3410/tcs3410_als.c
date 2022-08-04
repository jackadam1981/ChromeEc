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
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "ams_errno.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"
#include "tcs3410_als.h"
#include "tcs3410_utils.h"
#include "ams_device.h"
#include "ams_platform.h"

/******************************************************************************/
/*                                                                            */
/*                       Global APIs                                          */
/*                                                                            */
/******************************************************************************/
ams_errno_t process_als_data(volatile ams_current_state_t *pcurr_state, uint8_t *pfifo)
{
    ams_errno_t ret_val = AMS_SUCCESS;

    if (pfifo == NULL)
    {
        return(AMS_DEVICE_NULL_PTR);
    }

    /* Save the als fifo data */
    uint16_t idx;
    for (idx = 1; idx <= ALS_FIFO_DATA_LENGTH; idx++)
    {
      pcurr_state->als.als_fifo_data[idx-1] = pfifo[idx-1];
    }

#if defined(DEBUG_SHOW_ALS_FIFO)
    NRF_LOG_RAW_INFO("In process als fifo data \n");
    for (idx = 1; idx <= ALS_FIFO_DATA_LENGTH; idx++)
    {
      NRF_LOG_RAW_INFO("%2x ", pcurr_state->als.als_fifo_data[idx-1]);
      if ((idx % 9) == 0)
      {
         NRF_LOG_RAW_INFO("\n");
      }
    }
    NRF_LOG_RAW_INFO("\n");
#endif


    /* lux is calculated within the CLI */

    return(ret_val);
}
