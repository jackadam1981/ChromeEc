/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "ams_errno.h"
#include "ams_device.h"
#include "ams_platform.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"

/* Generic prototype for all sensors */
ams_errno_t ams_sensor_init(struct ams_device *device);

struct ams_device device = { 0 };
struct ams_platform platform = { 0 };

static bool device_ok = false;

ams_errno_t ams_device_init(void)
{
    ams_errno_t ret_val = AMS_UNKNOWN_ERROR;
    uint32_t pin = AMS_IRQ0_PIN; 

    ams_platform_init(&platform);
    device.log = platform.ams_platform_log;

    if (platform.ams_platform_i2c_init)
    {
        platform.ams_platform_i2c_init(AMS_SCL_PIN, AMS_SDA_PIN);
    }

    if (platform.ams_platform_irq_init)
    {
        platform.ams_platform_irq_init(pin); 
    }

    if (platform.ams_platform_spi_init)
    {
        platform.ams_platform_spi_init();
    }

    if (platform.ams_platform_gpio_init)
    {
        platform.ams_platform_gpio_init();
    }

    if (platform.ams_platform_timer_init)
    {
        platform.ams_platform_timer_init();
    }

    ret_val = ams_sensor_init(&device);
    if (ret_val == AMS_SUCCESS)
    {
        AMS_LOG_PRINTF(LOG_INFO, "Sensor init success.");
        device_ok = true;
    }
    else
    {
        AMS_LOG_PRINTF(LOG_ERROR, "Sensor init failed.\n");
    }
    return(ret_val);
}

void ams_device_write(uint8_t reg, uint8_t val)
{
    if (device.write)
    {
        device.write(reg, sh, val);
    }
    else
    {
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return;
}

void ams_device_read(uint8_t reg, uint8_t *buffer, uint8_t bytes)
{
    if (device.read)
    {
        device.read(reg, buffer, bytes);
    }
    else
    {
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return;
}

ams_errno_t ams_device_enable(ams_feature_t feature, ams_feature_enable_t enable)
{
    ams_errno_t ret = AMS_SUCCESS;
    if (device.enable)
    {
        ret = device.enable(feature, enable);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);;
}

ams_errno_t ams_device_sai(ams_sai_state_t state)
{
    ams_errno_t ret = AMS_SUCCESS;

    if (device.sai)
    {
        ret = device.sai(state);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

ams_errno_t ams_device_pon(bool state)
{
    ams_errno_t ret = AMS_SUCCESS;

    if (device.pon)
    {
        ret = device.pon(state);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

ams_errno_t ams_device_log_irq(bool state)
{
    ams_errno_t ret = AMS_SUCCESS;

    if (device.log_irq)
    {
        ret = device.log_irq(state);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

ams_errno_t ams_device_configure(ams_config_feature_t cfg_type, void *cfg)
{
    ams_errno_t ret = AMS_SUCCESS;
    
    if (device.configure)
    {
        ret = device.configure(cfg_type, cfg);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

ams_errno_t ams_device_isUP(bool *pOK)
{
    *pOK = device_ok;

    return(AMS_SUCCESS);
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

ams_errno_t ams_device_setup(void *cfg)
{
    ams_errno_t ret = AMS_UNKNOWN_ERROR;

    if (device.setup)
    {
        ret = device.setup(cfg);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

void ams_device_get_version(void)
{
    NRF_LOG_RAW_INFO("Device Version: %s\n", device.version);
    return;
}

static char formatted_message[FORMAT_LOG_MSG_SIZE];
static char log_message[LOG_MSG_SIZE];
void ams_device_log(const char *filename, const char *function, int line, uint32_t level, const char *format, ...)
{
    const char *local_filename = filename;
    int cnt;

    memset(log_message, 0, LOG_MSG_SIZE);
    memset(formatted_message, 0, FORMAT_LOG_MSG_SIZE);
    if (device.log)
    {
        va_list argptr;

        va_start(argptr, format);
        cnt = vsnprintf(log_message, LOG_MSG_SIZE-1, (char *)format, argptr);
        log_message[cnt] = '\0';
        va_end(argptr);

        cnt =snprintf(formatted_message, FORMAT_LOG_MSG_SIZE-1, "{%*.*s}:%*.*s():%*d --> %s", LOG_FILE_NAME_LEN, LOG_FILE_NAME_LEN,
        local_filename, LOG_FUNC_NAME_LEN, LOG_FUNC_NAME_LEN, function, LOG_LINE_NUM_LEN, line, log_message);
        formatted_message[cnt] = '\0';

        device.log(level, formatted_message);
    }
    return;
}

/* bitmap of registers that are in use */

static uint8_t reg_in_use[MAX_REGS / 8] = 
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 0x00 - 0x3f */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 0x40 - 0x7f */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    /* 0x80 - 0xbf */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    /* 0xc0 - 0xff */
};

ssize_t ams_registers_get(char *buf, int bufsiz)
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

    NRF_LOG_RAW_INFO("i2c_addr = 0x%02X\n", SLAVE_ADDR_0);

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

