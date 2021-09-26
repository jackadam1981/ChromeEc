/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Driver for tuning the MP2964 IMVP8 - IMVP9.1 parameters */

#include "console.h"
#include "i2c.h"
#include "mp2964.h"
#include "timer.h"
#include "util.h"

#define MP2964_STARTUP_WAIT_US		(50 * MSEC)
#define MP2964_STORE_WAIT_US		(300 * MSEC)
#define MP2964_RESTORE_WAIT_US		(2 * MSEC)

static struct mp2964_reg_dump_val mp2964_dump_reg[] = {
	{ MP2964_PAGE,                REG_PAGE_0_1 },
	{ MP2964_OPERATION,           REG_PAGE_0_1 },
	{ MP2964_LAST_FAULT_BLOCK,    REG_PAGE_0 },
	{ MP2964_MFR_SINGLE_RW_ADDR,  REG_PAGE_0 },
	{ MP2964_MFR_SINGLE_WR_DATA,  REG_PAGE_0 },
	{ MP2964_MFR_GATECLK_DIS,     REG_PAGE_0 },
	{ MP2964_MFR_IDROOP_CTRL,     REG_PAGE_0_1 },
	{ MP2964_MFR_MTP_CTRL,        REG_PAGE_0 },
	{ MP2964_MFR_PSYS_WARN_FILT_CNT,  REG_PAGE_0 },
	{ MP2964_MFR_LOW_VOUT_TRIM,       REG_PAGE_0_1 },
	{ MP2964_VOUT_COMMAND,            REG_PAGE_0_1 },
	{ MP2964_MFR_VOUT_TRIM,           REG_PAGE_0_1 },
	{ MP2964_VOUT_CAL_OFFSET,         REG_PAGE_0_1 },
	{ MP2964_MFR_VOUT_MAX,            REG_PAGE_0_1 },
	{ MP2964_VOUT_MARGIN_HIGH,        REG_PAGE_0_1 },
	{ MP2964_VOUT_MARGIN_LOW,         REG_PAGE_0_1 },
	{ MP2964_MFR_VBOOT,               REG_PAGE_0_1 },
	{ MP2964_MFR_FS,                  REG_PAGE_0_1 },
	{ MP2964_MFR_PHASE_NUM,           REG_PAGE_0_1 },
	{ MP2964_MFR_DBG_ADC_CHANNEL,     REG_PAGE_0 },
	{ MP2964_MFR_CONFIG1,             REG_PAGE_0_1 },
	{ MP2964_MFR_IMON_SNS_OFFS,       REG_PAGE_0_1 },
	{ MP2964_MFR_ADC_HOLD_TIME,       REG_PAGE_0 },
	{ MP2964_MFR_PLATFORM_TIME_SET,   REG_PAGE_0_1 },
	{ MP2964_MFR_DEBUG,               REG_PAGE_0 },
	{ MP2964_MFR_APSI_CTRL,           REG_PAGE_0_1 },
	{ MP2964_MFR_FS_LOOP_CYC_RIPPLE_SET,  REG_PAGE_0_1 },
	{ MP2964_MFR_DC_CB_DYNC_SET,      REG_PAGE_0_1 },
	{ MP2964_MFR_VOUT_CMPS_SET,       REG_PAGE_0_1 },
	{ MP2964_MFR_PROTECT_MODE,        REG_PAGE_0_1 },
	{ MP2964_VIN_ON,                  REG_PAGE_0 },
	{ MP2964_VIN_OFF,                 REG_PAGE_0 },
	{ MP2964_MFR_VCAL_FS_PI,          REG_PAGE_0_1 },
	{ MP2964_IOUT_CAL_GAIN_SET,       REG_PAGE_0_1 },
	{ MP2964_MFR_VR_CONFIG,           REG_PAGE_0_1 },
	{ MP2964_MFR_AUDIBLE_REDUCE,      REG_PAGE_0_1 },
	{ MP2964_MFR_VID_DOWN_DELAY,      REG_PAGE_0_1 },
	{ MP2964_MFR_FILTER_SET,          REG_PAGE_0_1 },
	{ MP2964_MFR_TRANS_FAST,          REG_PAGE_0_1 },
	{ MP2964_MFR_EN_SEQUENCE_CFG,     REG_PAGE_0_1 },
	{ MP2964_MFR_ALT_SET,             REG_PAGE_0_1 },
	{ MP2964_MFR_SW_LF_SET,           REG_PAGE_0_1 },
	{ MP2964_MFR_SW_HF_SET,           REG_PAGE_0_1 },
	{ MP2964_MFR_TON_ADJ_PS1_SW_LF_TH,  REG_PAGE_0_1 },
	{ MP2964_MFR_PS2_OFFSLOPE_TON_TH,   REG_PAGE_0_1 },
	{ MP2964_MFR_TON_ADJ_PS1_SW_HF_TH,  REG_PAGE_0_1 },
	{ MP2964_MFR_TON_ADJ_PS2_SW_HF_TH,  REG_PAGE_0_1 },
	{ MP2964_MFR_SLOPE_CNT_SETPS1,      REG_PAGE_0_1 },
	{ MP2964_MFR_SLOPE_SR_SETPS1,       REG_PAGE_0_1 },
	{ MP2964_MFR_CONFIG2,             REG_PAGE_0_1 },
	{ MP2964_MFR_CONFIG3,             REG_PAGE_0_1 },
	{ MP2964_MFR_PSI_ICC_CTRL,        REG_PAGE_0_1 },
	{ MP2964_MFR_APS_PHASE_HYS,       REG_PAGE_0_1 },
	{ MP2964_MFR_VOUT_MAX_9BIT,       REG_PAGE_0_1 },
	{ MP2964_MFR_SLOPE_CNT_DCM_SET,   REG_PAGE_0_1 },
	{ MP2964_MFR_SLOPE_SR_DCM,        REG_PAGE_0_1 },
	{ MP2964_MFR_SHUTDOWN_LEVEL,      REG_PAGE_0_1 },
	{ MP2964_MFR_SYS_PASSWORD,        REG_PAGE_0 },
	{ MP2964_MFR_ADDR_SVID_CTRL,      REG_PAGE_0_1 },
	{ MP2964_MFR_ICC_MAX_SET,         REG_PAGE_0_1 },
	{ MP2964_MFR_CYC_OCP_FACTOR,      REG_PAGE_0_1 },
	{ MP2964_MFR_PWR_INDUCTOR_GAIN,   REG_PAGE_0_1 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT,   REG_PAGE_0_1 },
	{ MP2964_MFR_OVP_UVP_SET,         REG_PAGE_0_1 },
	{ MP2964_MFR_OCP_SET,             REG_PAGE_0_1 },
	{ MP2964_PROTOCOL_ID_EN_RDY,      REG_PAGE_0 },
	{ MP2964_MFR_SVID_CFG,            REG_PAGE_0 },
	{ MP2964_VENDOR_ID_PRODUCT_ID,    REG_PAGE_0 },
	{ MP2964_PRODUCT_DATA_CODE,       REG_PAGE_0 },
	{ MP2964_LOT_CODE_VR,             REG_PAGE_0 },
	{ MP2964_PS3_PS4_EXIT_DELAY,      REG_PAGE_0_1 },
	{ MP2964_MFR_SR_FAST_TOLERANCE,   REG_PAGE_0_1 },
	{ MP2964_MFR_SVID_06H_34H_VR,     REG_PAGE_0_1 },
	{ MP2964_MFR_SVID_06H_34H_PSYS,   REG_PAGE_0 },
	{ MP2964_MFR_STANDBY_HOT_SET,     REG_PAGE_0 },
	{ MP2964_MFR_MIN_ON_TIME,         REG_PAGE_0 },
	{ MP2964_MFR_MIN_OFF_TIME,        REG_PAGE_0 },
	{ MP2964_MFR_MIN_HIZ_TIME,        REG_PAGE_0 },
	{ MP2964_MFR_BLANK_TIME,          REG_PAGE_0_1 },
	{ MP2964_MFR_PSI_TRIM4,           REG_PAGE_0_1 },
	{ MP2964_MFR_PSI_TRIM1,           REG_PAGE_0 },
	{ MP2964_MFR_PSI_TRIM2,           REG_PAGE_0 },
	{ MP2964_MFR_PSI_TRIM3,           REG_PAGE_0 },
	{ MP2964_MFR_VIN_HYS_OFFS_SET,    REG_PAGE_0 },
	{ MP2964_MFR_PIN_MAX,             REG_PAGE_0 },
	{ MP2964_MFR_AUXIMON_MAX_SET,     REG_PAGE_0 },
	{ MP2964_MFR_ADDR_PMBUS,          REG_PAGE_0 },
	{ MP2964_MFR_VIN_OV_UV_LIMIT,     REG_PAGE_0 },
	{ MP2964_MFR_OTP_SET,             REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_CNT_1P,        REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_SR_1P,         REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_CNT_2P,        REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_SR_2P,         REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_CNT_3P,        REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_SR_3P,         REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_CNT_4P,        REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_SR_4P,         REG_PAGE_0 },
	{ MP2964_RESERVED1,               REG_PAGE_0 },
	{ MP2964_RESERVED2,               REG_PAGE_0 },
	{ MP2964_RESERVED3,               REG_PAGE_0 },
	{ MP2964_RESERVED4,               REG_PAGE_0 },
	{ MP2964_RESERVED5,               REG_PAGE_0 },
	{ MP2964_RESERVED6,               REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_CNT_5P,        REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_SR_5P,         REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_CNT_6P,        REG_PAGE_0 },
	{ MP2964_MFR_SLOPE_SR_6P,         REG_PAGE_0 },
	{ MP2964_MFR_CS_OFFSET2_3,        REG_PAGE_0 },
	{ MP2964_MFR_CS_OFFSET4,          REG_PAGE_0 },
	{ MP2964_RESERVED7,               REG_PAGE_0 },
	{ MP2964_MFR_CS_OFFSET5_6,        REG_PAGE_0 },
	{ MP2964_MFR_IMON_SVID1,          REG_PAGE_0 },
	{ MP2964_MFR_IMON_SVID2,          REG_PAGE_0 },
	{ MP2964_MFR_IMON_SVID3,          REG_PAGE_0 },
	{ MP2964_MFR_IMON_SVID4,          REG_PAGE_0 },
	{ MP2964_RESERVED8,               REG_PAGE_0 },
	{ MP2964_RESERVED9,               REG_PAGE_0 },
	{ MP2964_RESERVED10,              REG_PAGE_0 },
	{ MP2964_MFR_IMON_SVID5,          REG_PAGE_0 },
	{ MP2964_MFR_IMON_SVID6,          REG_PAGE_0 },
	{ MP2964_MFR_CB_PI_SET,           REG_PAGE_0 },
	{ MP2964_MFR_VIN_GAIN_SET,        REG_PAGE_0 },
	{ MP2964_MFR_AUXIMON_SVID,        REG_PAGE_0 },
	{ MP2964_MFR_PSYS_SVID,           REG_PAGE_0 },
	{ MP2964_MFR_TEMPERATURE_GAIN_SET,  REG_PAGE_0 },
	{ MP2964_MFR_PSYS_GAIN_SEL,       REG_PAGE_0 },
};

static int mp2964_write8(uint8_t reg, uint8_t value)
{
	const uint8_t tx[2] = { reg, value };

	return i2c_xfer_unlocked(I2C_PORT_MP2964, I2C_ADDR_MP2964_FLAGS,
				 tx, sizeof(tx), NULL, 0, I2C_XFER_SINGLE);
}

static void mp2964_read16(uint8_t reg, uint16_t *value)
{
	const uint8_t tx[1] = { reg };
	uint8_t rx[2];

	i2c_xfer_unlocked(I2C_PORT_MP2964, I2C_ADDR_MP2964_FLAGS,
			  tx, sizeof(tx), rx, sizeof(rx), I2C_XFER_SINGLE);
	*value = (rx[1] << 8) | rx[0];
}

static void mp2964_write16(uint8_t reg, uint16_t value)
{
	const uint8_t tx[3] = { reg, value & 0xff, value >> 8 };

	i2c_xfer_unlocked(I2C_PORT_MP2964, I2C_ADDR_MP2964_FLAGS,
			  tx, sizeof(tx), NULL, 0, I2C_XFER_SINGLE);
}

static int mp2964_select_page(enum reg_page page)
{
	int status;

	if (page >= REG_PAGE_COUNT)
		return EC_ERROR_INVAL;

	status = mp2964_write8(MP2964_PAGE, page);
	if (status != EC_SUCCESS) {
		ccprintf("%s: could not select page 0x%02x, error %d\n",
			 __func__, page, status);
	}
	return status;
}

static void mp2964_write_vec16(const struct mp2964_reg_val *init_list,
			       int count, int *delta)
{
	const struct mp2964_reg_val *reg_val;
	uint16_t outval;
	int i;

	reg_val = init_list;
	for (i = 0; i < count; ++i, ++reg_val) {
		mp2964_read16(reg_val->reg, &outval);
		if (outval == reg_val->val) {
			ccprintf("mp2964: reg 0x%02x already 0x%04x\n",
				 reg_val->reg, outval);
			continue;
		}
		ccprintf("mp2964: tuning reg 0x%02x from 0x%04x to 0x%04x\n",
			 reg_val->reg, outval, reg_val->val);
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

	status = i2c_xfer_unlocked(I2C_PORT_MP2964, I2C_ADDR_MP2964_FLAGS,
				   &wr, sizeof(wr), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;

	usleep(MP2964_STORE_WAIT_US);

	status = i2c_xfer_unlocked(I2C_PORT_MP2964, I2C_ADDR_MP2964_FLAGS,
				   &rd, sizeof(rd), NULL, 0, I2C_XFER_SINGLE);
	if (status != EC_SUCCESS)
		return status;

	usleep(MP2964_RESTORE_WAIT_US);

	return EC_SUCCESS;
}

static void mp2964_patch_rail(enum reg_page page,
			      const struct mp2964_reg_val *page_vals,
			      int count,
			      int *delta)
{
	if (mp2964_select_page(page) != EC_SUCCESS)
		return;
	mp2964_write_vec16(page_vals, count, delta);
}

int mp2964_tune(const struct mp2964_reg_val *rail_a, int count_a,
		const struct mp2964_reg_val *rail_b, int count_b)
{
	int tries = 2;
	int delta;

	udelay(MP2964_STARTUP_WAIT_US);

	i2c_lock(I2C_PORT_MP2964, 1);

	do {
		int status;

		delta = 0;
		mp2964_patch_rail(REG_PAGE_0, rail_a, count_a, &delta);
		mp2964_patch_rail(REG_PAGE_1, rail_b, count_b, &delta);
		if (delta == 0)
			break;

		status = mp2964_store_user_all();
		if (status != EC_SUCCESS)
			ccprintf("%s: STORE_USER_ALL failed\n", __func__);
	} while (--tries > 0);

	i2c_lock(I2C_PORT_MP2964, 0);

	if (delta)
		return EC_ERROR_UNKNOWN;
	else
		return EC_SUCCESS;
}

static int command_mp2964_dump(int argc, char **argv)
{
	uint16_t outval;
	int i;

	i2c_lock(I2C_PORT_MP2964, 1);
	if (mp2964_select_page(0) != EC_SUCCESS) {
		i2c_lock(I2C_PORT_MP2964, 0);
		return EC_ERROR_UNKNOWN;
	}

	for (i = 0; i < ARRAY_SIZE(mp2964_dump_reg); i++) {
		mp2964_read16(mp2964_dump_reg[i].reg, &outval);
		mp2964_dump_reg[i].val = outval;
	}
	i2c_lock(I2C_PORT_MP2964, 0);

	ccprintf("mp2964: Page 0 reg values\n");
	for (i = 0; i < ARRAY_SIZE(mp2964_dump_reg); i++) {
		ccprintf("mp2964: reg 0x%02x  val: 0x%04x\n",
		mp2964_dump_reg[i].reg, mp2964_dump_reg[i].val);
		usleep(2 * MSEC);
	}

	i2c_lock(I2C_PORT_MP2964, 1);
	if (mp2964_select_page(1) != EC_SUCCESS) {
		i2c_lock(I2C_PORT_MP2964, 0);
		return EC_ERROR_UNKNOWN;
	}

	for (i = 0; i < ARRAY_SIZE(mp2964_dump_reg); i++) {
		if (mp2964_dump_reg[i].page == REG_PAGE_0_1) {
			mp2964_read16(mp2964_dump_reg[i].reg, &outval);
			mp2964_dump_reg[i].val = outval;
		}
	}
	i2c_lock(I2C_PORT_MP2964, 0);

	ccprintf("mp2964: Page 1 reg values\n");
	for (i = 0; i < ARRAY_SIZE(mp2964_dump_reg); i++) {
		if (mp2964_dump_reg[i].page == REG_PAGE_0_1) {
			ccprintf("mp2964: reg 0x%02x  val: 0x%04x\n",
			mp2964_dump_reg[i].reg, mp2964_dump_reg[i].val);
			usleep(2 * MSEC);
		}
	}
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(mp2964_dump, command_mp2964_dump,
			NULL,
			"dump the MP2964 registers");
