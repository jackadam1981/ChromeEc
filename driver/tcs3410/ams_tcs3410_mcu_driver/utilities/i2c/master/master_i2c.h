/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __MASTER_I2C_H__
#define __MASTER_I2C_H__

int ams_i2c_block_read(uint8_t addr, uint8_t reg, uint8_t *data, int size);
int ams_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data);
int ams_i2c_block_write(uint8_t addr, uint8_t reg, uint8_t *data, int size);
int ams_i2c_write(uint8_t addr, uint8_t *sh, uint8_t reg, uint8_t data);
int ams_i2c_write_direct(uint8_t addr, uint8_t reg, uint8_t data);
int ams_i2c_modify(uint8_t addr, uint8_t *sh, uint8_t reg, uint8_t mask, uint8_t val);
ams_errno_t ams_i2c_init(uint8_t scl, uint8_t sda);

#endif /* __MASTER_I2C_H__ */
