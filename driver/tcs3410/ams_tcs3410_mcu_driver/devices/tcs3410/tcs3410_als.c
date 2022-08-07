/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stddef.h>
#include "common.h"
#include "printf.h"
#include "ams_errno.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"
#include "tcs3410_als.h"
#include "tcs3410_utils.h"
#include "ams_device.h"

/* TODO(gwendal): Remove when lux is calculated. */
#define DEBUG_SHOW_ALS_FIFO

/******************************************************************************/
/*                                                                            */
/*                       Global APIs                                          */
/*                                                                            */
/******************************************************************************/
ams_errno_t process_als_data(volatile ams_current_state_t *pcurr_state, uint8_t *pfifo)
{
#if defined(DEBUG_SHOW_ALS_FIFO)
	char str_buf[hex_str_buf_size(9 * sizeof(uint16_t))];
#endif
	uint16_t idx;

	if (pfifo == NULL)
	{
		return(AMS_DEVICE_NULL_PTR);
	}

	/* Save the als fifo data */
	for (idx = 0; idx < ALS_FIFO_DATA_LENGTH; idx++)
	{
		pcurr_state->als.als_fifo_data[idx] = pfifo[idx];
	}

#if defined(DEBUG_SHOW_ALS_FIFO)
	for (idx = 0; idx < ALS_FIFO_DATA_LENGTH; idx += 9)
	{
		snprintf_hex_buffer(str_buf, sizeof(str_buf),
				    HEX_BUF(&pfifo[idx], sizeof(str_buf)));
		AMS_LOG_PRINTF(LOG_INFO,"fifo raw data: %s\n", str_buf);
	}
#endif


	/* lux is calculated within the CLI */
	return AMS_SUCCESS;
}
