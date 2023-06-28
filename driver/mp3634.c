/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Driver for tuning the MP3634 IMVP8 - IMVP9.1 parameters */

#include "console.h"
#include "i2c.h"
#include "mp3634.h"
#include "timer.h"
#include "util.h"

#define MP3634_STARTUP_WAIT_US 500
#define MP3634_RESTORE_WAIT_US 100
#define MP3634_STORE_WAIT_US (375 * MSEC)
#define MP3634_RELOAD_WAIT_US 100

#define REG_PAGE_0 0x82
#define REG_PAGE_1 0x80

static int mp3634_write8(uint8_t reg, uint8_t value)
{
	const uint8_t tx[2] = { reg, value };

	return i2c_xfer_unlocked(I2C_PORT_MP3634, I2C_ADDR_MP3634_FLAGS, tx,
				 sizeof(tx), NULL, 0, I2C_XFER_SINGLE);
}

static void mp3634_read8(uint8_t reg, uint8_t *value)
{
	const uint8_t tx[1] = { reg };
	uint8_t rx[1];

	i2c_xfer_unlocked(I2C_PORT_MP3634, I2C_ADDR_MP3634_FLAGS, tx,
			  sizeof(tx), rx, sizeof(rx), I2C_XFER_SINGLE);
	*value = rx[0];
}

static void mp3634_i2c_unlock(void)
{
	mp3634_write8(MP3634_UNLOCK_NVM, 0x01);
}

static void mp3634_check_nvm_restore_flag(void)
{
	uint8_t value;
	mp3634_read8(MP3634_NVM_PROGRAM_STATUS, &value);
	value &= 0x80;
	if (value == 0x80) {
		ccprints("NVM restore flag = 1");
	} else {
		ccprints("NVM restore flag != 1");
	} 
}

static void mp3634_check_procdut_id(void)
{
	uint8_t value;
	mp3634_read8(MP3634_PRODUCT_ID, &value);
	value &= 0x7f;
	ccprints("Product ID= 0x%X", value);
}

static void mp3634_check_revision_id(void)
{
	uint8_t value;
	mp3634_read8(MP3634_REVISION_ID, &value);
	value &= 0x03;
	ccprints("Revision ID= 0x%X", value);
}

static void mp3634_check_i2c_fw_version(void)
{
	uint8_t value;
	mp3634_read8(MP3634_I2C_FW_VERSION, &value);
	value &= 0x0f;
	ccprints("I2C FW Version = 0x%X", value);
}

static void mp3634_unlock(void)
{
	mp3634_write8(MP3634_ENTER_CONF_MODE, 0x24);
	mp3634_write8(MP3634_ENTER_CONF_MODE, 0x54);
	mp3634_write8(MP3634_ENTER_CONF_MODE, 0x02);
}

static void mp3634_check_fw_version(void)
{
	uint8_t value;
	mp3634_read8(MP3634_SET_FW_VER_LSB, &value);
	ccprints("FW Version LSB= 0x%X", value);
	mp3634_read8(MP3634_SET_FW_VER_MSB, &value);
	ccprints("FW Version MSB= 0x%X", value);
}

static void mp3634_check_crc(void)
{
	uint8_t value;
	mp3634_read8(MP3634_CRC_PAGE_GROUP_1, &value);
	ccprints("CRC-8= 0x%X", value);
}

static int mp3634_select_page(int page)
{
	int status;

	status = mp3634_write8(MP3634_PAGE, page);
	if (status != EC_SUCCESS) {
		ccprintf("%s: could not select page 0x%02x, error %d\n",
			 __func__, page, status);
	}
	return status;
}

static void mp3634_write_vec8(const struct mp3634_reg_val *init_list,
			       int count, int *delta)
{
	const struct mp3634_reg_val *reg_val;
	uint8_t outval;
	int i;

	reg_val = init_list;
	for (i = 0; i < count; ++i, ++reg_val) {
		mp3634_read8(reg_val->reg, &outval);
		ccprintf("mp3634: reg 0x%02x : 0x%02x\n",
			 reg_val->reg, outval);
		/*
		if (outval == reg_val->val) {
			ccprintf("mp3634: reg 0x%02x already 0x%02x\n",
				 reg_val->reg, outval);
			continue;
		}
		ccprintf("mp3634: tuning reg 0x%02x from 0x%02x to 0x%02x\n",
			 reg_val->reg, outval, reg_val->val);
		mp3634_write8(reg_val->reg, reg_val->val);
		*/
		*delta += 1;
	}
}

/*static int mp3634_store_user_all(void)
{
	const uint8_t wr = MP3634_STORE_USER_ALL;
	const uint8_t rd = MP3634_RESTORE_USER_ALL;
	int status;

	ccprintf("%s: updating persistent settings\n", __func__);

	status = i2c_xfer_unlocked(I2C_PORT_MP3634, I2C_ADDR_MP3634_FLAGS, &wr,
				   sizeof(wr), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;

	usleep(MP3634_STORE_WAIT_US);

	status = i2c_xfer_unlocked(I2C_PORT_MP3634, I2C_ADDR_MP3634_FLAGS, &rd,
				   sizeof(rd), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;

	usleep(MP3634_RESTORE_WAIT_US);

	return EC_SUCCESS;
}*/

static void mp3634_patch_rail(int page,
			      const struct mp3634_reg_val *page_vals, int count,
			      int *delta)
{
	if (mp3634_select_page(page) != EC_SUCCESS)
		return;
	mp3634_write_vec8(page_vals, count, delta);
}

int mp3634_tune(const struct mp3634_reg_val *rail_a, int count_a,
		const struct mp3634_reg_val *rail_b, int count_b)
{
	int tries = 2;
	int delta;

	i2c_lock(I2C_PORT_MP3634, 1);
	udelay(MP3634_STARTUP_WAIT_US);

	mp3634_i2c_unlock();

	udelay(MP3634_RESTORE_WAIT_US);

	mp3634_check_nvm_restore_flag();
	mp3634_check_procdut_id();
	mp3634_check_revision_id();
	mp3634_check_i2c_fw_version();
	mp3634_unlock();
	mp3634_select_page(REG_PAGE_0);
	mp3634_check_fw_version();
	mp3634_check_crc();

	do {
		int status;

		delta = 0;
		mp3634_patch_rail(REG_PAGE_0, rail_a, count_a, &delta);
		mp3634_patch_rail(REG_PAGE_1, rail_b, count_b, &delta);
		if (delta == 0)
			break;

		/*status = mp3634_store_user_all();*/
		if (status != EC_SUCCESS)
			ccprintf("%s: STORE_USER_ALL failed\n", __func__);
	} while (--tries > 0);

	/*i2c_lock(I2C_PORT_MP3634, 0);*/

	if (delta)
		return EC_ERROR_UNKNOWN;
	else
		return EC_SUCCESS;
}
