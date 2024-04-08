/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "i2c.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ##args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ##args)

#define I2C_PORT_IMVP I2C_PORT_BATTERY

#define I2C_IMVP_ADDR1 0x26
#define I2C_IMVP_ADDR2 0x33

struct imvp_t {
	uint8_t reg;
	uint8_t data;
};

static const struct imvp_t imvp_fw[] = {
	{ 0x72, 0x01 },
	/* 0x90 ~ 0x95 */
	{ 0x90, 0x01 },
	{ 0x91, 0x02 },
	{ 0x92, 0x03 },
	{ 0x93, 0x04 },
	{ 0x94, 0x05 },
	{ 0x95, 0x06 },
	/* 0xC0 ~ 0xFF */
	{ 0xC0, 0x20 },
	{ 0xC1, 0x21 },
	{ 0xC2, 0x22 },
	{ 0xC3, 0x23 },
	{ 0xC4, 0x24 },
	{ 0xC5, 0x25 },
	{ 0xC6, 0x26 },
	{ 0xC7, 0x27 },
	{ 0xC8, 0x28 },
	{ 0xC9, 0x29 },
	{ 0xCA, 0x2A },
	{ 0xCB, 0x2B },
	{ 0xCC, 0x2C },
	{ 0xCD, 0x2D },
	{ 0xCE, 0x2E },
	{ 0xCF, 0x2F },
	{ 0xD0, 0x30 },
	{ 0xD1, 0x31 },
	{ 0xD2, 0x32 },
	{ 0xD3, 0x33 },
	{ 0xD4, 0x34 },
	{ 0xD5, 0x35 },
	{ 0xD6, 0x36 },
	{ 0xD7, 0x37 },
	{ 0xD8, 0x38 },
	{ 0xD9, 0x39 },
	{ 0xDA, 0x3A },
	{ 0xDB, 0x3B },
	{ 0xDC, 0x3C },
	{ 0xDD, 0x3D },
	{ 0xDE, 0x3E },
	{ 0xDF, 0x3F },
	{ 0xE0, 0x40 },
	{ 0xE1, 0x41 },
	{ 0xE2, 0x42 },
	{ 0xE3, 0x43 },
	{ 0xE4, 0x44 },
	{ 0xE5, 0x45 },
	{ 0xE6, 0x46 },
	{ 0xE7, 0x47 },
	{ 0xE8, 0x48 },
	{ 0xE9, 0x49 },
	{ 0xEA, 0x4A },
	{ 0xEB, 0x4B },
	{ 0xEC, 0x4C },
	{ 0xED, 0x4D },
	{ 0xEE, 0x4E },
	{ 0xEF, 0x4F },
	{ 0xF0, 0x50 },
	{ 0xF1, 0x51 },
	{ 0xF2, 0x52 },
	{ 0xF3, 0x53 },
	{ 0xF4, 0x54 },
	{ 0xF5, 0x55 },
	{ 0xF6, 0x56 },
	{ 0xF7, 0x57 },
	{ 0xF8, 0x58 },
	{ 0xF9, 0x59 },
	{ 0xFA, 0x5A },
	{ 0xFB, 0x5B },
	{ 0xFC, 0x5C },
	{ 0xFD, 0x5D },
	{ 0xFE, 0x5E },
	{ 0xFF, 0x5F },
};

static const struct imvp_t imvp_ctrl1[] = {
	{ 0x81, 0x91 },
	{ 0x81, 0x33 },
};

static const struct imvp_t imvp_ctrl2[] = {
	{ 0x80, 0x06 },
};

static const struct imvp_t imvp_ctrl3[] = {
	{ 0x80, 0x03 },
};

static const struct imvp_t imvp_ctrl4[] = {
	{ 0x81, 0x80 },
};

uint8_t mismatch_index[ARRAY_SIZE(imvp_fw)];
uint8_t mismatch_count;

static int imvp_write_addr1(uint8_t reg, uint8_t val)
{
	int rv;
	int data = val;

	rv = i2c_write8(I2C_PORT_IMVP, I2C_IMVP_ADDR1, reg, data);

	CPRINTF("writing reg=0x%x val=0x%x, rv=%d\n", reg, val, rv);
	return rv;
}

static int imvp_write_addr2(uint8_t reg, uint8_t val)
{
	int rv;
	int data = val;

	rv = i2c_write8(I2C_PORT_IMVP, I2C_IMVP_ADDR2, reg, data);

	CPRINTF("writing reg=0x%x val=0x%x, rv=%d\n", reg, val, rv);
	return rv;
}

static int imvp_read(uint8_t reg, uint8_t *val)
{
	int rv;
	int data = 0;

	rv = i2c_read8(I2C_PORT_IMVP, I2C_IMVP_ADDR1, reg, &data);

	*val = (uint8_t)data;
	CPRINTF("Read reg=0x%x val=0x%x, rv=%d\n", reg, *val, rv);
	return rv;
}

static bool chip_is_ready(void)
{
	/*
	 * check chip status
	 * VCC is high
	 * EN is low
	 * EN_SMB is high
	 */
	return true;
}

static bool chip_is_blank(void)
{
	int rv;
	uint8_t val;

	rv = imvp_read(0xe2, &val);

	if (val == 0x7f) {
		return true;
	}
	return false;
}

static int write_imvp_fw(void)
{
	uint8_t i;

	/* Write reg data */
	for (i = 0; i < ARRAY_SIZE(imvp_fw); i++)
		imvp_write_addr1(imvp_fw[i].reg, imvp_fw[i].data);

	for (i = 0; i < ARRAY_SIZE(imvp_ctrl1); i++)
		imvp_write_addr1(imvp_ctrl1[i].reg, imvp_ctrl1[i].data);

	for (i = 0; i < ARRAY_SIZE(imvp_ctrl2); i++)
		imvp_write_addr2(imvp_ctrl2[i].reg, imvp_ctrl2[i].data);

	/* sleep 20 ms for programming */
	k_msleep(20);

	/* Store data to NVM */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl3); i++)
		imvp_write_addr2(imvp_ctrl3[i].reg, imvp_ctrl3[i].data);

	/* sleep 1 ms for programming */
	k_msleep(1);

	/* Exit Burning mode */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl4); i++)
		imvp_write_addr1(imvp_ctrl4[i].reg, imvp_ctrl4[i].data);

	CPRINTF("IMVP f/w update SUCCESS\n");
	return EC_SUCCESS;
}

static bool imvp_is_rewritable(void)
{
	uint8_t val;

	imvp_read(0x7e, &val);

	if ((val & 0x3F) == 0) {
		return false;
	}
	return true;
}

static bool imvp_is_rewrite_success(void)
{
	uint8_t val;

	imvp_read(0x7e, &val);

	if (val & 0x80) {
		return true;
	}
	return false;
}

static int rewrite_mismatch_data(uint8_t index)
{
	if (imvp_is_rewritable()) {
		imvp_write_addr1(0x7f, 0x47);
		imvp_write_addr1(0x7f, imvp_fw[index].reg);
		imvp_write_addr1(imvp_fw[index].reg, imvp_fw[index].data);
		k_msleep(20);
	} else {
		return EC_ERROR_UNKNOWN;
	}

	if (imvp_is_rewrite_success()) {
		return EC_SUCCESS;
	} else {
		return EC_ERROR_UNKNOWN;
	}
}

static void rewrite_end_process(void)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(imvp_ctrl1); i++)
		imvp_write_addr1(imvp_ctrl1[i].reg, imvp_ctrl1[i].data);

	for (i = 0; i < ARRAY_SIZE(imvp_ctrl3); i++)
		imvp_write_addr2(imvp_ctrl3[i].reg, imvp_ctrl3[i].data);

	/* sleep 1 ms for programming */
	k_msleep(1);

	/* Exit F/W update mode */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl4); i++)
		imvp_write_addr1(imvp_ctrl4[i].reg, imvp_ctrl4[i].data);
}

static void flash_end_process(void)
{
	/*
	 * pull VCC low
	 * pull EN_SMB low
	 */
}

static int read_and_compare_imvp_fw(void)
{
	uint8_t i;
	uint8_t fw_read[ARRAY_SIZE(imvp_fw)];

	/* Read IMVP firmware from NVM and compare */
	for (i = 0; i < ARRAY_SIZE(imvp_fw); i++) {
		imvp_read(imvp_fw[i].reg, &fw_read[i]);
		if (fw_read[i] != imvp_fw[i].data) {
			mismatch_index[mismatch_count++] = i;
		}
	}

	if (mismatch_count > 0) {
		CPRINTF("IMVP f/w mismatch detected, mismatch count: %d\n",
			mismatch_count);
		for (i = 0; i < mismatch_count; i++) {
			CPRINTF("Mismatch index: %d, expt: 0x%x, act: 0x%x\n",
				mismatch_index[i],
				imvp_fw[mismatch_index[i]].data,
				fw_read[mismatch_index[i]]);
		}
		return EC_ERROR_UNKNOWN;
	}

	CPRINTF("IMVP f/w read back and compare SUCCESS\n");
	return EC_SUCCESS;
}

static void flash_imvp_fw(void)
{
	uint8_t i;

	if (!chip_is_ready()) {
		CPRINTF("IMVP chip is not ready\n");
		return;
	}

	if (chip_is_blank()) {
		CPRINTF("IMVP chip is blank\n");
		write_imvp_fw();
	}

	read_and_compare_imvp_fw();

	if (mismatch_count > 0) {
		/* rewrite mismatch data */
		for (i = 0; i < mismatch_count; i++) {
			if (rewrite_mismatch_data(mismatch_index[i]) !=
			    EC_SUCCESS) {
				CPRINTF("Rewrite at index %d\n", i);
			}
		}
		rewrite_end_process();
		flash_end_process();
	} else {
		/* No mismatch data */
		flash_end_process();
	}
}
DECLARE_HOOK(HOOK_INIT, flash_imvp_fw, HOOK_PRIO_POST_I2C);
