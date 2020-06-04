/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MPS MP4245 driver. */

#include "mp4245.h"
#include "common.h"
#include "console.h"
#include "i2c.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

struct mp4245_info {
	uint8_t cmd;
	uint8_t len;
};

static struct mp4245_info  mp4245_cmds[] = {
	{MP4245_CMD_OPERATION,          1},
	{MP4245_CMD_CLEAR_FAULTS,       1},
	{MP4245_CMD_WRITE_PROTECT,      1},
	{MP4245_CMD_STORE_USER_ALL,     1},
	{MP4245_CMD_RESTORE_USER_ALL,   1},
	{MP4245_CMD_VOUT_MODE,          1},
	{MP4245_CMD_VOUT_COMMAND,       2},
	{MP4245_CMD_VOUT_SCALE_LOOP,    2},
	{MP4245_CMD_STATUS_BYTE,        1},
	{MP4245_CMD_STATUS_WORD,        2},
	{MP4245_CMD_STATUS_VOUT,        1},
	{MP4245_CMD_STATUS_INPUT,       1},
	{MP4245_CMD_STATUS_TEMP,        1},
	{MP4245_CMD_STATUS_CML,         1},
	{MP4245_CMD_READ_VIN,           2},
	{MP4245_CMD_READ_VOUT,          2},
	{MP4245_CMD_READ_IOUT,          2},
	{MP4245_CMD_READ_TEMP,          2},
	{MP4245_CMD_MFR_MODE_CTRL,      1},
	{MP4245_CMD_MFR_CURRENT_LIM,    1},
	{MP4245_CMD_MFR_LINE_DROP,      1},
	{MP4245_CMD_MFR_OT_FAULT_LIM,   1},
	{MP4245_CMD_MFR_OT_WARN_LIM,    1},
	{MP4245_CMD_MFR_CRC_ERROR,      1},
	{MP4245_CMD_MFF_MTP_CFG_CODE,   1},
	{MP4245_CMD_MFR_MTP_REV_NUM,    1},
	{MP4245_CMD_MFR_STATUS_MASK,    1},
};

static int mp4245_reg16_write(int offset, int data)
{
	return i2c_write16(I2C_PORT_MP4245, MP4245_SLAVE_ADDR, offset,
			   data);
}

int mp4245_set_voltage_out(int desired_mv)
{
	int vout;

	vout = (desired_mv * MP4245_VOUT_FROM_MV + (MP4245_VOUT_ONE_VOLT >> 1))
		/ MP4245_VOUT_ONE_VOLT;

	return mp4245_reg16_write(MP4245_CMD_VOUT_COMMAND, vout);
}

int mp4245_set_current_lim(int desired_ma)
{
	int limit;
	int rv1;
	int val;
	int rv2;

	limit = (desired_ma + (MP4245_ILIM_STEP_MA / 2)) / MP4245_ILIM_STEP_MA;

	CPRINTS("mp4245: I_bus desired = %d, limit = %x", desired_ma,
		limit);

	rv1 = i2c_write8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
			 MP4245_CMD_MFR_CURRENT_LIM, limit);

	rv2 = i2c_read8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		   MP4245_CMD_MFR_CURRENT_LIM, &val);

	CPRINTS("mp4245: I_bus = %d, lim = %x, rb = %x, %d %d",
		desired_ma, limit, val, rv1, rv2);

	return rv1;
}

int mp4245_votlage_out_enable(int enable)
{
	int cmd_val = enable ? MP4245_CMD_OPERATION_ON : 0;
	int rv;

	rv = i2c_write8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
			MP4245_CMD_OPERATION, cmd_val);

	i2c_read8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		  MP4245_CMD_OPERATION, &cmd_val);

	CPRINTS("mp4245: VBUS enable = %d, cmd_oper = %x", enable, cmd_val);

	return rv;
}

void mp4245_dump_reg(void)
{
	int i;
	int val;
	int rv;

	for (i = 0; i < ARRAY_SIZE(mp4245_cmds); i++) {
		if (mp4245_cmds[i].len == 1) {
			rv = i2c_read8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
				       mp4245_cmds[i].cmd, &val);
		} else {
			rv = i2c_read16(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
				       mp4245_cmds[i].cmd, &val);
		}

		if (rv)
			val = 0xffff;
		CPRINTS("[%02x]:\t%04x", mp4245_cmds[i].cmd, val);
	}

}

#define MP4245_VOUT_TO_MV(v) ((v * 1000) / (1 << 10))
#define MP4245_IOUT_TO_MA(i) (((i & 0x7ff) * 1000) / (1 << 6))
#define MP4245

void mp4245_get_status(void)
{
	int status;
	int on;
	int vbus;
	int ibus;
	int ilim;
	int vout;

	/* Get Operation register */
	i2c_read8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		  MP4245_CMD_OPERATION, &on);
	/* Vbus on/off is bit 7 */
	on >>= 7;

	/* Get status word */
	i2c_read16(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		  MP4245_CMD_STATUS_WORD, &status);

	/* Get Vbus measurement */
	i2c_read16(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		   MP4245_CMD_READ_VOUT, &vbus);
	vbus = MP4245_VOUT_TO_MV(vbus);

	/* Get Ibus measurement */
	i2c_read16(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		  MP4245_CMD_READ_IOUT, &ibus);
	ibus = MP4245_IOUT_TO_MA(ibus);

	/* Get Vout command (sets Vbus level) */
	i2c_read16(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		   MP4245_CMD_VOUT_COMMAND, &vout);
	vout = MP4245_VOUT_TO_MV(vout);

	/* Get Input current limit */
	i2c_read8(I2C_PORT_MP4245, MP4245_SLAVE_ADDR,
		   MP4245_CMD_MFR_CURRENT_LIM, &ilim);
	ilim *= MP4245_ILIM_STEP_MA;

	ccprintf("mp4245 Vbus %s:\n", on ? "On" : "Off");
	ccprintf("\tstatus = 0x%04x\n", status);
	ccprintf("\tVout   = %d mV, Vbus = %d mV\n", vout, vbus);
	ccprintf("\tIlim   = %d mA, Ibus = %d mA\n", ilim, ibus);
}

static int command_mp4245(int argc, char **argv)
{
	char *e;
	int val;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "info")) {
		mp4245_get_status();
	} else if (!strcasecmp(argv[1], "dump")) {
		mp4245_dump_reg();
	} else if (!strcasecmp(argv[1], "vbus")) {
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		val = strtoi(argv[2], &e, 10);
		/* If value out of bounds, turn off */
		if (val < 0 || val > 20) {
			return EC_ERROR_PARAM2;
		}
		if (val == 0) {
			mp4245_votlage_out_enable(0);
		} else {
			mp4245_set_voltage_out(val * 1000);
			mp4245_votlage_out_enable(1);
		}
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(mp4245, command_mp4245,
			"[info|dump|vbus|ilim",
			"Turn on/off|set vbus.");

