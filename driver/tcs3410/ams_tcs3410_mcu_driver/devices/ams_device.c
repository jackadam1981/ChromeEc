/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdio.h>
#include "ams_errno.h"
#include "ams_device.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"

/* Generic prototype for all sensors */
ams_errno_t ams_sensor_init(struct ams_device *device);

struct ams_device device = { 0 };

{


    if (ret_val == AMS_SUCCESS)
    {
        AMS_LOG_PRINTF(LOG_INFO, "Sensor init success.");
    }
    else
    {
        AMS_LOG_PRINTF(LOG_ERROR, "Sensor init failed.\n");
    }
}

{
}

{
}

{

}

{


}

{
}

{
}

{

}

{

}

ams_errno_t ams_device_status(void *stat)
{
    ams_errno_t ret = AMS_SUCCESS;

    if (device.status)
    {
        ret = device.status(stat);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

/* bitmap of registers that are in use */
static uint8_t reg_in_use[MAX_REGS / 8] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 0x00 - 0x3f */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 0x40 - 0x7f */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    /* 0x80 - 0xbf */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    /* 0xc0 - 0xff */
};

size_t ams_registers_get(char *buf, int bufsiz)
{
    uint8_t regval[16];
    int i, j, k, cnt;

    /* find first non-zero bank of registers */
    for (i = 0; i < ARRAY_SIZE(reg_in_use); i++)
    {
        if (reg_in_use[i] != 0)
        {
            break;
        }
    }

    i &= ~1;  /* round down to the start of a group of 16 */
    i *= 8;   /* set to actual register id - each bit in the map represents a register byte*/

    cnt = 0;

    /* Print the index along the top of the registers */
    cnt += snprintf(buf + cnt, bufsiz - cnt, "     ");
    for (k = 0; k < 16; k++)
    {
        cnt += snprintf(buf + cnt, bufsiz - cnt, " %01x ", k);
        if (k == 7)
        {
            cnt += snprintf(buf + cnt, bufsiz - cnt, "  ");
        }
    }
    cnt += snprintf(buf + cnt, bufsiz - cnt, "\n     -------------------------------------------------\n");

    /* Dump the registers */
    for (; i < MAX_REGS; i += 16)
    {
        cnt += snprintf(buf + cnt, bufsiz - cnt, "%02x: ", i);
        ams_device_read(i, &regval[0], 16);

        for (j = 0; j < 16; j++)
        {
            if (reg_in_use[(i >> 3) + (j >> 3)] & (1 << (j & 7)))
            {
                cnt += snprintf(buf + cnt, bufsiz - cnt, " %02x", regval[j]);
            }
            else
            {
                cnt += snprintf(buf + cnt, bufsiz - cnt, " --");
            }

            if (j == 7)
            {
                cnt += snprintf(buf + cnt, bufsiz - cnt, "  ");
            }
        }
        cnt += snprintf(buf + cnt, bufsiz - cnt, "\n");
    }

    cnt += snprintf(buf + cnt, bufsiz - cnt, "\n");
    return(cnt);
}

