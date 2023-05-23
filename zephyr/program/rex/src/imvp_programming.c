/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "console.h"
#include <zephyr/logging/log.h>

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)

#define I2C_PORT_IMVP	I2C_PORT_EEPROM

#define I2C_ADDR_IMVP	0x20

struct imvp_t {
	uint8_t reg;
	uint8_t data;
};

static const struct imvp_t imvp_ctrl1[] = {
	{0xFC, 0x01}, /* Unlock I2C control */
	{0xF1, 0x24}, /* Enable F/W update mode */
	{0xF1, 0x54},
	{0xF1, 0x02},
	{0xEF, 0x82}, /* Set to page to function set */
};

static const struct imvp_t imvp_fw[] = {
	{0x00, 0x80},
	{0x01, 0x80},
	{0x02, 0x80},
	{0x03, 0x00},
	{0x04, 0x00},
	{0x05, 0x28},
	{0x06, 0x61},
	{0x07, 0x10},
	{0x08, 0x00},
	{0x09, 0x0F},
	{0x0A, 0x20},

	{0x10, 0x5D},
	{0x11, 0x22},
	{0x12, 0x26},
	{0x13, 0x23},
	{0x14, 0x01},
	{0x15, 0x22},
	{0x16, 0x32},
	{0x17, 0xFF},
	{0x18, 0x08},
	{0x19, 0x20},
	{0x1A, 0x52},
	{0x1B, 0x51},
	{0x1C, 0xCC},
	{0x1D, 0x59},
	{0x1E, 0x41},
	{0x1F, 0x0A},

	{0x20, 0x16},
	{0x21, 0x0D},

	{0x30, 0xA1},
	{0x31, 0xA1},
	{0x32, 0xA1},
	{0x33, 0xA1},
	{0x34, 0xA1},
	{0x35, 0xA1},

	{0x7D, 0x01},

	{0x90, 0x92},
	{0x91, 0x48},
	{0x92, 0x26},
	{0x93, 0x9B},
	{0x94, 0x12},
	{0x95, 0x48},
	{0x96, 0xC0},
	{0x97, 0x92},
	{0x98, 0x00},
	{0x99, 0x7B},
	{0x9A, 0x12},
	{0x9B, 0xC0},
	{0x9C, 0x07},
};

static const struct imvp_t imvp_fw_28[] = {
        {0x00, 0xc0},
        {0x01, 0xc0},
        {0x02, 0x80},
        {0x03, 0x81},
        {0x04, 0x82},
        {0x05, 0xaa},
        {0x06, 0x61},
        {0x07, 0x10},
        {0x08, 0x00},
        {0x09, 0x0F},
        {0x0A, 0x20},

        {0x10, 0x6D},
        {0x11, 0x22},
        {0x12, 0x5d},
        {0x13, 0x23},
        {0x14, 0x02},
        {0x15, 0x33},
        {0x16, 0x43},
        {0x17, 0xFF},
        {0x18, 0x10},
        {0x19, 0x20},
        {0x1A, 0xd2},
        {0x1B, 0x51},
        {0x1C, 0xCC},
        {0x1D, 0x19},
        {0x1E, 0x41},
        {0x1F, 0x02},

        {0x20, 0x16},
        {0x21, 0x0c},

        {0x30, 0xA1},
        {0x31, 0xA1},
        {0x32, 0xA1},
        {0x33, 0xA1},
        {0x34, 0xA1},
        {0x35, 0xA1},

        {0x7D, 0x01},

        {0x90, 0x92},
        {0x91, 0x48},
        {0x92, 0x26},
        {0x93, 0x9B},
        {0x94, 0x12},
        {0x95, 0x48},
        {0x96, 0xC0},
        {0x97, 0x92},
        {0x98, 0x00},
        {0x99, 0x7B},
        {0x9A, 0x12},
        {0x9B, 0xC0},
        {0x9C, 0x7b},
};

static const struct imvp_t imvp_ctrl2[] = {
	{0xEF, 0x80}, /* Set page to SVID */
};

static const struct imvp_t imvp_ctrl3[] = {
	{0xED, 0xAA}, /* Store data in NVM GROUP 1*/
};

static const struct imvp_t imvp_ctrl4[] = {
	{0xED, 0x55}, /* Store data in NVM Group 0*/
};

static const struct imvp_t imvp_ctrl5[] = {
	{0xF1, 0x00}, /* Exit F/W update mode */
	{0xF1, 0xFF},
};

static int imvp_write(uint8_t reg, uint8_t val)
{
	int rv;
	int data = val;

	rv = i2c_write8(I2C_PORT_IMVP, I2C_ADDR_IMVP, reg, data);

	CPRINTF("writing reg=0x%x val=0x%x, rv=%d\n", reg, val, rv);
	return rv;
}

static int imvp_read(uint8_t reg, uint8_t *val)
{
	int rv;
	int data = 0;

	rv = i2c_read8(I2C_PORT_IMVP, I2C_ADDR_IMVP, reg, &data);

	*val = (uint8_t) data;
	CPRINTF("Read reg=0x%x val=0x%x, rv=%d\n", reg, *val, rv);
	return rv;
}

static int console_command_imvp_w(int argc, const char **argv)
{
	uint8_t i;

	/* Enable F/W update control */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl1); i++)
		imvp_write(imvp_ctrl1[i].reg, imvp_ctrl1[i].data);

	/* Write data */
	for (i = 0; i < ARRAY_SIZE(imvp_fw); i++)
		imvp_write(imvp_fw[i].reg, imvp_fw[i].data);

	/* Store data to NVM Group 1 */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl3); i++)
		imvp_write(imvp_ctrl3[i].reg, imvp_ctrl3[i].data);

	/* sleep 800 ms for programming */
	k_msleep(800);

	/* Store data to NVM Group 0 */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl4); i++)
		imvp_write(imvp_ctrl4[i].reg, imvp_ctrl4[i].data);

	/* sleep 800 ms for programming */
	k_msleep(800);

	/* Return page to default */
        for (i = 0; i < ARRAY_SIZE(imvp_ctrl2); i++)
		imvp_write(imvp_ctrl2[i].reg, imvp_ctrl2[i].data);

	/* Exit F/W update mode */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl5); i++)
		imvp_write(imvp_ctrl5[i].reg, imvp_ctrl5[i].data);

	CPRINTF("IMVP f/w update SUCCESS\n");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(imvp_w, console_command_imvp_w,
			NULL, "F/W update IMVP");

static int console_command_imvp_w_28(int argc, const char **argv)
{
	uint8_t i;

	/* Enable F/W update control */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl1); i++)
		imvp_write(imvp_ctrl1[i].reg, imvp_ctrl1[i].data);

	/* Write data */
	for (i = 0; i < ARRAY_SIZE(imvp_fw_28); i++)
		imvp_write(imvp_fw_28[i].reg, imvp_fw_28[i].data);

	/* Store data to NVM Group 1 */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl3); i++)
		imvp_write(imvp_ctrl3[i].reg, imvp_ctrl3[i].data);

	/* sleep 800 ms for programming */
	k_msleep(800);

	/* Store data to NVM Group 0 */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl4); i++)
		imvp_write(imvp_ctrl4[i].reg, imvp_ctrl4[i].data);

	/* sleep 800 ms for programming */
	k_msleep(800);

	/* Return page to default */
        for (i = 0; i < ARRAY_SIZE(imvp_ctrl2); i++)
		imvp_write(imvp_ctrl2[i].reg, imvp_ctrl2[i].data);

	/* Exit F/W update mode */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl5); i++)
		imvp_write(imvp_ctrl5[i].reg, imvp_ctrl5[i].data);

	CPRINTF("IMVP f/w update SUCCESS\n");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(imvp_w_28, console_command_imvp_w_28,
			NULL, "F/W update IMVP");

static int console_command_imvp_r(int argc, const char **argv)
{
	uint8_t i, val, err = 0;

	/* Enable F/W update control */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl1); i++)
		imvp_write(imvp_ctrl1[i].reg, imvp_ctrl1[i].data);

	/* Read data */
	for (i = 0; i < ARRAY_SIZE(imvp_fw); i++) {
		imvp_read(imvp_fw[i].reg, &val);
		if (val != imvp_fw[i].data)
			err++;
	}

	/* Set page to SVID */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl2); i++)
		imvp_write(imvp_ctrl2[i].reg, imvp_ctrl2[i].data);

	/* Exit F/W update mode */
	for (i = 0; i < ARRAY_SIZE(imvp_ctrl5); i++)
		imvp_write(imvp_ctrl5[i].reg, imvp_ctrl5[i].data);

	CPRINTF("IMVP f/w update %s\n", err == 0x00 ? "SUCCESS" : "FAIL");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(imvp_r, console_command_imvp_r,
			NULL, "F/W update IMVP");
