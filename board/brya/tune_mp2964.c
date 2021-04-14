/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Tune the MP2964 IMVP9.1 parameters for brya */

#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"
#include "util.h"

#define MP2964_PAGE			0x00
#define MP2964_STORE_USER_ALL		0x15
#define MP2964_RESTORE_USER_ALL		0x16
#define MP2964_MFR_ALT_SET		0x3f

#define MP2964_STARTUP_WAIT_US		(50 * MSEC)
#define MP2964_STORE_WAIT_US		(300 * MSEC)
#define MP2964_RESTORE_WAIT_US		(2 * MSEC)

static int mp2964_write8(uint8_t reg, uint8_t value)
{
	uint8_t buf[2] = { reg, value };

	return i2c_xfer_unlocked(I2C_PORT_POWER, I2C_ADDR_MP2964_FLAGS,
				 buf, sizeof(buf), NULL, 0, I2C_XFER_SINGLE);
}

static void mp2964_read16(uint8_t reg, uint16_t *value)
{
	uint8_t buf[3] = { reg };

	i2c_xfer_unlocked(I2C_PORT_POWER, I2C_ADDR_MP2964_FLAGS,
			  buf, 1, buf + 1, 2, I2C_XFER_SINGLE);
	*value = (buf[2] << 8) | buf[1];
}

static void mp2964_write16(uint8_t reg, uint16_t value)
{
	uint8_t buf[3] = { reg, value & 0xff, value >> 8 };

	i2c_xfer_unlocked(I2C_PORT_POWER, I2C_ADDR_MP2964_FLAGS,
			  buf, sizeof(buf), NULL, 0, I2C_XFER_SINGLE);
}

struct reg_val16 {
	uint8_t reg;
	uint16_t val;
};

static int mp2964_select_page(uint8_t page)
{
	int status;

	if (page > 1)
		return EC_ERROR_INVAL;

	status = mp2964_write8(MP2964_PAGE, page);
	if (status != EC_SUCCESS) {
		ccprintf("%s: could not select page 0x%02x, error %d\n",
			 __func__, page, status);
	}
	return status;
}

static void mp2964_write_vec16(const struct reg_val16 *init_list, int count,
			       int *delta)
{
	const struct reg_val16 *reg_val;
	uint16_t oval;
	int i;

	reg_val = init_list;
	for (i = 0; i < count; ++i, ++reg_val) {
		mp2964_read16(reg_val->reg, &oval);
		if (oval == reg_val->val) {
			ccprintf("mp2964: reg 0x%02x already 0x%04x\n",
				 reg_val->reg, oval);
			continue;
		}
		ccprintf("mp2964: tuning reg 0x%02x from 0x%04x to 0x%04x\n",
			 reg_val->reg, oval, reg_val->val);
		mp2964_write16(reg_val->reg, reg_val->val);
		*delta += 1;
	}
}

static int mp2964_store_user_all(void)
{
	const uint8_t wr = MP2964_STORE_USER_ALL;
	const uint8_t rd = MP2964_RESTORE_USER_ALL;
	int status;

	ccprintf("%s: updating persistent settings\n", __func__);

	status = i2c_xfer_unlocked(I2C_PORT_POWER, I2C_ADDR_MP2964_FLAGS,
				   &wr, sizeof(wr), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;
	usleep(MP2964_STORE_WAIT_US);

	status = i2c_xfer_unlocked(I2C_PORT_POWER, I2C_ADDR_MP2964_FLAGS,
				   &rd, sizeof(rd), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;
	usleep(MP2964_RESTORE_WAIT_US);

	return EC_SUCCESS;
}

static void board_patch_rail1(int *delta)
{
	const static struct reg_val16 rail1[] = {
		{ MP2964_MFR_ALT_SET,     0xe081 },
	};

	if (mp2964_select_page(0x00) != EC_SUCCESS)
		return;
	mp2964_write_vec16(rail1, ARRAY_SIZE(rail1), delta);
}

static void board_patch_rail2(int *delta)
{
	const static struct reg_val16 rail2[] = {
		{ MP2964_MFR_ALT_SET,     0xe081 },
	};

	if (mp2964_select_page(0x01) != EC_SUCCESS)
		return;
	mp2964_write_vec16(rail2, ARRAY_SIZE(rail2), delta);
}

static void mp2964_on_startup(void)
{
	int tries = 2;
	int delta;

	ccprintf("%s: attempting to tune PMIC\n", __func__);
	udelay(MP2964_STARTUP_WAIT_US);

	i2c_lock(I2C_PORT_POWER, 1);

	do {
		int status;

		delta = 0;
		board_patch_rail1(&delta);
		board_patch_rail2(&delta);
		if (delta == 0)
			break;

		status = mp2964_store_user_all();
		if (status != EC_SUCCESS)
			ccprintf("%s: STORE_USER_ALL failed\n", __func__);
	} while (--tries > 0);

	if (delta > 0)
		ccprintf("%s: could not update all settings\n", __func__);

	i2c_lock(I2C_PORT_POWER, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp2964_on_startup,
	     HOOK_PRIO_FIRST);
