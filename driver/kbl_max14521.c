/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Maxim MAX14521 EL lamp driver
 */

#include "common.h"
#include "console.h"
#include "driver/kbl_max14521.h"
#include "host_command.h"
#include "i2c.h"
#include "lid_switch.h"
#include "pwm.h"
#include "system.h"
#include "util.h"


/* I2C interface */
#define MAX14521_I2C_ADDR       0xF0
#define MAX14521_REG_DEV_ID     0x00
#define MAX14251_DEVICE_ID      0xB2
#define MAX14521_REG_PWR_MODE   0x01
#define MAX14521_REG_EL_FREQ	0x02
#define MAX14521_REG_EL_SHAPE   0x03
#define MAX14521_REG_BST_FREQ   0x04
#define MAX14521_REG_AUDIO      0x05
#define MAX14521_REG_EL1_T_V    0x06
#define MAX14521_REG_EL2_T_V    0x07
#define MAX14521_REG_EL3_T_V    0x08
#define MAX14521_REG_EL4_T_V    0x09
#define MAX14521_REG_EL_UPDATE  0x0A

#define MAX14521_PWR_MODE_EN    0x01
#define MAX14521_PWR_MODE_DIS   0x00
#define MAX14521_BST_FREQ_1KHZ  0x04
#define MAX14521_EL_FREQ_180HZ  0x73
#define MAX14521_RT_2SEC        (0x07 << 5)        /* Ramping Time Configuration : 2 seconds */

#define MAX14521_VSTEP_MAX      31
static int curr_percent;

int max14521_set_kblight(int percent)
{
        int rv, step;

        if(percent == 0) {
                curr_percent = 0;

                return i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                                MAX14521_REG_PWR_MODE, MAX14521_PWR_MODE_DIS);
        }

        if(percent < 0)
                return EC_ERROR_PARAM1;

        if(percent > 100)
                return EC_ERROR_PARAM1;

        step = (percent * (MAX14521_VSTEP_MAX - 1)) / 100 + 1;

        if(curr_percent == 0) {
                rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                                MAX14521_REG_PWR_MODE, MAX14521_PWR_MODE_EN);

                if(rv)
                        return rv;
        }

        /* Write ramping time and voltage */
        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL1_T_V, MAX14521_RT_2SEC | step);

        if(rv)
                return rv;

        /* It is needed that write update register to change output level of EL.*/
        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL_UPDATE, MAX14521_RT_2SEC | step);

        if(rv)
                return rv;

        curr_percent = percent;

        return EC_SUCCESS;
}

int max14521_init(void)
{
	int rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_BST_FREQ, MAX14521_BST_FREQ_1KHZ);

        if(rv)
                return rv;

        rv = i2c_write8(I2C_PORT_KBLIGHT, MAX14521_I2C_ADDR,
                        MAX14521_REG_EL_FREQ, MAX14521_EL_FREQ_180HZ);

        if(rv)
                return rv;

        return max14521_set_kblight(0);
}

int max14521_get_kblight(void)
{
        return curr_percent;
}

/*****************************************************************************/
/* Console commands */

static int command_kblight(int argc, char **argv)
{
        if (argc >= 2) {
                char *e;
                int i = strtoi(argv[1], &e, 0);
                if (*e)
                        return EC_ERROR_PARAM1;
                max14521_set_kblight(i);
        }

        ccprintf("Keyboard backlight: %d%%\n", max14521_get_kblight());
        return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kblight, command_kblight,
                        "percent",
                        "Set keyboard backlight",
                        NULL);

/*****************************************************************************/
/* Host commands */

int pwm_command_get_keyboard_backlight(struct host_cmd_handler_args *args)
{
        struct ec_response_pwm_get_keyboard_backlight *r = args->response;

        r->percent = curr_percent;
        r->enabled = (curr_percent != 0);
        args->response_size = sizeof(*r);

        return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_GET_KEYBOARD_BACKLIGHT,
                     pwm_command_get_keyboard_backlight,
                     EC_VER_MASK(0));

int pwm_command_set_keyboard_backlight(struct host_cmd_handler_args *args)
{
        const struct ec_params_pwm_set_keyboard_backlight *p = args->params;

        max14521_set_kblight(p->percent);

        return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PWM_SET_KEYBOARD_BACKLIGHT,
                     pwm_command_set_keyboard_backlight,
                     EC_VER_MASK(0));
