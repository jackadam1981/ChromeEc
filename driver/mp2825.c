/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Driver for tuning the MP2825 IMVP8 - IMVP9.1 parameters */

#include "console.h"
#include "i2c.h"
#include "mp2825.h"
#include "timer.h"
#include "util.h"

#define MP2825_STARTUP_WAIT_US (5 * MSEC)
#define MP2825_STORE_WAIT_US (520 * MSEC)
#define MP2825_RESTORE_WAIT_US (5 * MSEC)

enum reg_page { REG_PAGE_0, REG_PAGE_1, REG_PAGE_2, REG_PAGE_COUNT };

static int mp2825_write8(uint8_t reg, uint8_t value)
{
	const uint8_t tx[2] = { reg, value };

	return i2c_xfer_unlocked(I2C_PORT_MP2825, I2C_ADDR_MP2825_FLAGS, tx,
				 sizeof(tx), NULL, 0, I2C_XFER_SINGLE);
}

static void mp2825_read8(uint8_t reg, uint8_t *value)
{
	const uint8_t tx[1] = { reg };
	uint8_t rx[1];

	i2c_xfer_unlocked(I2C_PORT_MP2825, I2C_ADDR_MP2825_FLAGS, tx,
			  sizeof(tx), rx, sizeof(rx), I2C_XFER_SINGLE);
	*value = rx[0];
}

static void mp2825_read16(uint8_t reg, uint16_t *value)
{
	const uint8_t tx[1] = { reg };
	uint8_t rx[2];

	i2c_xfer_unlocked(I2C_PORT_MP2825, I2C_ADDR_MP2825_FLAGS, tx,
			  sizeof(tx), rx, sizeof(rx), I2C_XFER_SINGLE);
	*value = (rx[1] << 8) | rx[0];
}

static int mp2825_write16(uint8_t reg, uint16_t value)
{
	const uint8_t tx[3] = { reg, value & 0xff, value >> 8 };

	return i2c_xfer_unlocked(I2C_PORT_MP2825, I2C_ADDR_MP2825_FLAGS, tx,
			  sizeof(tx), NULL, 0, I2C_XFER_SINGLE);
}

static int mp2825_deviceid_check(void)
{
	uint16_t id;
	
	mp2825_read16(MP2825_MFR_DEVICE_ID, &id);
	ccprints("MP2825 device id : %X", id);
	id = id >>8;
	if (id == 0x25)
		return EC_SUCCESS;
	
	return EC_ERROR_UNKNOWN;
}

static int mp2825_password_unlock(void)
{
	return mp2825_write16(MP2825_MFR_PASSWORD_UNLOCK, 0x5aa5);
}

static int mp2825_crc_fault_check(void)
{
	uint8_t val;
	
	mp2825_read8(MP2825_MFR_CRC_FAULT, &val);
	ccprints("MP2825 crc fault : %X", val);
	val &= 0x2;
	if (val == 0)
		return EC_SUCCESS;
	
	return EC_ERROR_UNKNOWN;
}

static int mp2825_clear_crc_fault(void)
{
	return mp2825_write8(MP2825_CLEAR_CRC_FAULT, 0);
}

static int mp2825_checksum_check(void)
{
	uint16_t val;
	
	mp2825_read16(MP2825_MFR_READ_CRC, &val);
	ccprints("MP2825 checksum : %X", val);
	if (val == 0xa0f2)
		return EC_SUCCESS;
	
	return EC_ERROR_UNKNOWN;
}

static int mp2825_password_lock(void)
{
	int retry = 0;
	uint16_t val;
	
	do
	{
		if (mp2825_write16(MP2825_MFR_PASSWORD_UNLOCK, 0x0000) == EC_SUCCESS) {
			mp2825_read16(MP2825_MFR_I2C_PASSWORD, &val);
			val &= 0x8000;
			if (val == 0x8000)
				return EC_SUCCESS;
			else
				retry++;
		}
	} while (retry <=1);
	return EC_ERROR_UNKNOWN;
}

static int mp2825_select_page(enum reg_page page)
{
	int status;

	if (page >= REG_PAGE_COUNT)
		return EC_ERROR_INVAL;

	status = mp2825_write8(MP2825_PAGE, page);
	if (status != EC_SUCCESS) {
		ccprintf("%s: could not select page 0x%02x, error %d\n",
			 __func__, page, status);
	}
	return status;
}

static int mp2825_set_operation(enum reg_page page)
{
	int status;

	if (page >= REG_PAGE_COUNT)
		return EC_ERROR_INVAL;

	status = mp2825_write8(MP2825_OPERATION, 0x80);
	if (status != EC_SUCCESS) {
		ccprintf("%s: could not set operation 0x%02x, error %d\n",
			 __func__, page, status);
	}
	return status;
}

static void mp2825_write_vec16(const struct mp2825_reg_val *init_list,
			       int count, int *delta)
{
	const struct mp2825_reg_val *reg_val;
	uint16_t outval;
	int i;

	reg_val = init_list;
	for (i = 0; i < count; ++i, ++reg_val) {
		mp2825_read16(reg_val->reg, &outval);
		if (outval == reg_val->val) {
			ccprintf("mp2825: reg 0x%02x already 0x%04x\n",
				 reg_val->reg, outval);
			continue;
		}
		ccprintf("mp2825: tuning reg 0x%02x from 0x%04x to 0x%04x\n",
			 reg_val->reg, outval, reg_val->val);
		mp2825_write16(reg_val->reg, reg_val->val);
		*delta += 1;
	}
}

static int mp2825_store_user_all(void)
{
	const uint8_t wr = MP2825_STORE_USER_ALL;
	const uint8_t rd = MP2825_RESTORE_USER_ALL;
	int status;

	ccprintf("%s: updating persistent settings\n", __func__);

	status = i2c_xfer_unlocked(I2C_PORT_MP2825, I2C_ADDR_MP2825_FLAGS, &wr,
				   sizeof(wr), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;

	usleep(MP2825_STORE_WAIT_US);

	status = i2c_xfer_unlocked(I2C_PORT_MP2825, I2C_ADDR_MP2825_FLAGS, &rd,
				   sizeof(rd), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;

	usleep(MP2825_RESTORE_WAIT_US);

	return EC_SUCCESS;
}

static void mp2825_patch_rail(enum reg_page page,
			      const struct mp2825_reg_val *page_vals, int count,
			      int *delta)
{
	if (mp2825_select_page(page) != EC_SUCCESS)
		return;
	if (mp2825_set_operation(page) != EC_SUCCESS)
		return;
	mp2825_write_vec16(page_vals, count, delta);
}

int mp2825_tune(const struct mp2825_reg_val *rail_a, int count_a,
		const struct mp2825_reg_val *rail_b, int count_b,
		const struct mp2825_reg_val *rail_c, int count_c)
{
	int tries = 2;
	int delta;

	udelay(MP2825_STARTUP_WAIT_US);

	if (mp2825_deviceid_check() != EC_SUCCESS) {
		ccprints("tune1");
		return EC_ERROR_UNKNOWN;
	}
	if (mp2825_password_unlock() != EC_SUCCESS) {
		ccprints("tune2");
		return EC_ERROR_UNKNOWN;
	}
	if (mp2825_crc_fault_check() != EC_SUCCESS) {
		ccprints("tune3");
		mp2825_clear_crc_fault();
		goto tuning_start;
	}

tuning_start:
	do {
		int status;

		delta = 0;
		mp2825_patch_rail(REG_PAGE_0, rail_a, count_a, &delta);
		mp2825_patch_rail(REG_PAGE_1, rail_b, count_b, &delta);
		mp2825_patch_rail(REG_PAGE_2, rail_c, count_b, &delta);
		if (delta == 0)
			break;

		status = mp2825_store_user_all();
		if (status != EC_SUCCESS)
			ccprintf("%s: STORE_USER_ALL failed\n", __func__);
	} while (--tries > 0);

	if (mp2825_crc_fault_check() != EC_SUCCESS) {
		ccprints("tune4");
		mp2825_clear_crc_fault();
		goto tuning_start;
	}

	if (mp2825_checksum_check() != EC_SUCCESS) {
		ccprints("tune5");
		return EC_ERROR_UNKNOWN;
	}
	if (mp2825_password_lock() != EC_SUCCESS) {
		ccprints("tune6");
		return EC_ERROR_UNKNOWN;
	}

	if (delta)
		return EC_ERROR_UNKNOWN;
	else
		return EC_SUCCESS;
}
