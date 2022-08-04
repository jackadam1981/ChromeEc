/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

/*
 * This file contains the i2c stub routines that are platform and architecure
 * specific and are essential to proper operation of the ams device driver.
 *
 * These are just examples of possible candidates for the i2c interface.  The
 * i2c APIs are called from mainly the sensor driver.  Here are some examples
 * where the i2c APIs are used:
 *    - sensor_read()
 *    - sensor_write()
 *    - sensor_modify()
 */

#include <stdint.h>
#include <stdio.h>

#include "ams_errno.h"
#include "master_i2c.h"
#include "ams_device.h"

int i2c_block_read(uint8_t addr, uint8_t reg, uint8_t *data, int size)
{
    int ret;

    /*
     * Insert platform-specific i2c block read here:
     */

    return(ret);
}

int i2c_read(uint8_t addr, uint8_t reg, uint8_t *data)
{
    return i2c_block_read(addr, reg, data, 1);
}

int i2c_block_write(uint8_t addr, uint8_t reg, uint8_t *data, int size)
{
    int ret;

    /*
     * Insert platform-specific i2c block write here:
     */

    return(ret);
}

int i2c_write(uint8_t addr, uint8_t reg, uint8_t data)
{
    return i2c_block_write(addr, reg, &data, 1);
}

int i2c_modify(uint8_t addr, uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t temp;

    i2c_read(addr, reg, &temp);
    temp &= ~mask;
    temp |= val;
    return i2c_write(addr, reg, temp);
}

int i2c_init(uint8_t scl, uint8_t sda)
{
    int ret;

    /*
     * Insert platform-specific i2c initialization here:
     * ^ For example, pins used for the i2c bus.^
     */

    return ret;
}

