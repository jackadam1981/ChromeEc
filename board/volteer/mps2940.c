/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MPX2940 register dump routine */

#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"
#include "util.h"

#define I2C_ADDR_MP2940	0x20
#define MPS2940_PORT	I2C_PORT_EEPROM

#define MP2940_STORE_USER_ALL	0x15
#define MP2940_MFR_PHASE_NUM	0xCA

/*
 * Note - original vendor ID and product code:
 *
 * VENDOR_ID_PRODUCT_ID   (bfh) = 2540
 * PRODUCT_DATA_CODE_VR   (c0h) = 8064
 * LOT_CODE_VR            (c1h) = 3401
 */

/*
 * Volteer updates:
 *
 * 0xFA = 0x0b0a
 * 0xFD = 0xc088
 * 0xF1 = 0x7F5D
 * 0xCE = 0xe64c
 * 0xF6 = 0xBECD
 * 0xD6 = 0x002f
 * 0xE5 = 0x1865
 */

struct reg_entry {
	uint8_t	reg;
	const char *name;
	unsigned int bytes;
};

static const struct reg_entry reg_list[] =
{
	{ 0x07, "LAST_FAULT_BLOCK       ", 2, },
	{ 0x08, "CLEAR_LAST_FAULT       ", 0, },
	{ 0x15, "STORE_USER_ALL         ", 0, },
	{ 0x16, "RESTORE_USER_ALL       ", 0, },
	{ 0x1B, "IDROOP_CTRL            ", 2, },
	{ 0x1D, "MFR_MTP CTRL           ", 2, },
	{ 0x1E, "PSYS_WARN_FILT_CNT     ", 1, },
	{ 0x21, "VOUT_COMMAND           ", 2, },
	{ 0x22, "MFR_VOUT_TRIM          ", 2, },
	{ 0x23, "VOUT_CAL_OFFSET        ", 2, },
	{ 0x24, "MFR_VOUT_MAX           ", 2, },
	{ 0x25, "VOUT_MARGIN_HIGH       ", 2, },
	{ 0x26, "VOUT_MARGIN_LOW        ", 2, },
	{ 0x2B, "MFR_IMMEDIATE_SET      ", 2, },
	{ 0x2E, "MFR_PLATFORM_TIME      ", 2, },
	{ 0x30, "MFR_APSI_CTRL          ", 2, },
	{ 0x35, "VIN_ON                 ", 2, },
	{ 0x36, "VIN_OFF                ", 2, },
	{ 0x38, "IOUT_CAL_GAIN          ", 2, },
	{ 0x39, "IOUT_CAL_OFFSET        ", 2, },
	{ 0x3A, "MFR_IMON_SVID1         ", 2, },
	{ 0x3B, "MFR_IMON_SVID2         ", 2, },
	{ 0x3C, "MFR_IMON_SVID3         ", 2, },
	{ 0x50, "MFR_SYS_PASSWORD       ", 1, },
	{ 0x51, "MFR_INPUT_PASSWORD     ", 1, },
	{ 0x55, "VIN_OV_FAULT_LIMIT     ", 2, },
	{ 0x58, "VIN_UV_WARNING_LIMIT   ", 2, },
	{ 0x72, "SVID_REG_80H_81H       ", 2, },
	{ 0x73, "SVID_REG_82H           ", 1, },
	{ 0x74, "SLAVE_ADDR             ", 1, },
	{ 0x75, "MFR_CS_1_2             ", 2, },
	{ 0x76, "MFR_CS_3               ", 2, },
	{ 0x7A, "STATUS_VOUT            ", 1, },
	{ 0x7B, "STATUS_IOUT            ", 1, },
	{ 0x7C, "STATUS_INPUT           ", 1, },
	{ 0x7D, "STATUS_TEMPERATURE     ", 1, },
	{ 0x7E, "STATUS_CML             ", 1, },
	{ 0x88, "READ_VIN               ", 2, },
	{ 0x8B, "READ_VOUT              ", 2, },
	{ 0x8C, "READ_IOUT              ", 2, },
	{ 0x8D, "READ_TEMPERATURE       ", 2, },
	{ 0x96, "READ_POUT              ", 2, },
	{ 0x97, "READ_PIN               ", 2, },
	{ 0xB8, "MFR_VIN_HYS            ", 1, },
	{ 0xBB, "MFR_1PHL_HYS           ", 1 },
	{ 0xBD, "PROTOCOL_ID_SVID_RDY_VR", 2 },
	{ 0xBE, "PS3_PS4_EXIT_DELAY     ", 2 },
	{ 0xBF, "VENDOR_ID_PRODUCT_ID   ", 2 },
	{ 0xC0, "PRODUCT_DATA_CODE_VR   ", 2 },
	{ 0xC1, "LOT_CODE_VR            ", 2 },
	{ 0xC2, "DECAY_CFG_34H_06H      ", 2 },
	{ 0xC3, "TOLERANCE_SR_FAST      ", 2 },
	{ 0xC4, "MFR_VOUT_MAX_9BIT      ", 2 },
	{ 0xC5, "IVID2_1_I_DEF          ", 2 },
	{ 0xC6, "PIN_MAX_IVID3_I_DEF    ", 2 },
	{ 0xCA, "MFR_PHASE_NUM          ", 1 },
	{ 0xCC, "DC_CTRL_DYNAMIC_FLT    ", 2 },
	{ 0xCD, "MFR_SW_HF_SET          ", 2 },
	{ 0xCE, "MIN_ON_OFF_BLANK_TIME  ", 2 },
	{ 0xCF, "MFR_SW_LF_SET          ", 2 },
	{ 0xD6, "MFR_SLOPE_SR_3P        ", 2 },
	{ 0xD7, "MFR_SLOPE_CNT_3P       ", 2 },
	{ 0xD8, "MFR_SLOPE_SR_2P        ", 2 },
	{ 0xD9, "MFR_SLOPE_CNT_2P       ", 2 },
	{ 0xDA, "MFR_SLOPE_SR_1P        ", 2 },
	{ 0xDB, "MFR_SLOPE_CNT_1P       ", 2 },
	{ 0xDC, "MFR_SLOPE_SR_DCM       ", 2 },
	{ 0xDD, "MFR_SLOPE_CNT_DCM      ", 2 },
	{ 0xDE, "MFR_TRIM_2_1_DCM       ", 2 },
	{ 0xDF, "MFR_TRIM_3             ", 2 },
	{ 0xE1, "SHUTLEVEL_ADDRPMBUS    ", 2 },
	{ 0xE2, "MFR_CB_SATU_PI         ", 2 },
	{ 0xE3, "MFR_VCAL_PI            ", 2 },
	{ 0xE4, "MFR_VR_CONFIG          ", 2 },
	{ 0xE5, "MFR_FS _VBOOT          ", 2 },
	{ 0xE6, "MFR_ADDR_SVID          ", 2 },
	{ 0xE8, "TEMPERATURE_GAIN_OFFSET", 2 },
	{ 0xE9, "MFR_CUR_GAIN           ", 2 },
	{ 0xEA, "MFR_CUR_OFFSET         ", 1 },
	{ 0xEB, "MFR_CS_OFFSET1_2       ", 2 },
	{ 0xEC, "MFR_CS_OFFSET3         ", 2 },
	{ 0xEE, "MFR_OCP_SET            ", 2 },
	{ 0xEF, "MFR_ICC_MAX            ", 2 },
	{ 0xF0, "MFR_VOUT_CMPS_MAX      ", 1 },
	{ 0xF1, "MFR_PROTECT_CFG        ", 2 },
	{ 0xF2, "MFR_OTP_SET            ", 2 },
	{ 0xF3, "MFR_TEMP_MAX           ", 2 },
	{ 0xF5, "MFR_AUDIBLE_REDUCE     ", 2 },
	{ 0xF6, "OCP_OVP_DA_LIMIT       ", 2 },
	{ 0xF7, "MFR_OVP_UVP_SET        ", 2 },
	{ 0xF8, "MFR_VID_DOWN_DELAY     ", 2 },
	{ 0xF9, "MFR_FILTER_SET         ", 2 },
	{ 0xFA, "MFR_TRANS_FAST         ", 2 },
	{ 0xFB, "MFR_EN_SEQUENCE_CFG    ", 2 },
	{ 0xFC, "MFR_PSYS_SVID          ", 2 },
	{ 0xFD, "MFR_ALT_SET            ", 2 },
};
static const unsigned int reg_size = ARRAY_SIZE(reg_list);


static void mp2949_read8(uint8_t reg, uint8_t *value)
{
	uint8_t buf[2] = { reg };
	int status;
	status = i2c_xfer(MPS2940_PORT, I2C_ADDR_MP2940,
			  buf, 1, buf + 1, 1);
	if (status != EC_SUCCESS)
		return;
	*value = buf[1];
}

static int mp2949_write8(uint8_t reg, uint8_t value)
{
	uint8_t buf[2] = { reg, value };
	return i2c_xfer(MPS2940_PORT, I2C_ADDR_MP2940,
			buf, sizeof(buf), NULL, 0);
}

static void mp2949_read16(uint8_t reg, uint16_t *value)
{
	uint8_t buf[3] = { reg };
	i2c_xfer(MPS2940_PORT, I2C_ADDR_MP2940,
		 buf, 1, buf + 1, 2);
	*value = (buf[2] << 8) | buf[1];
}

static int mp2949_store_user_all(void)
{
	const uint8_t wr = MP2940_STORE_USER_ALL;
	int status;
	status = i2c_xfer(MPS2940_PORT, I2C_ADDR_MP2940,
			  &wr, sizeof(wr), NULL, 0);
	if (status != EC_SUCCESS) {
		ccprintf("MPS I2C write error\n");
		return status;
	}

	return EC_SUCCESS;
}

static int command_mps(int argc, char **argv)
{
	int i;
	int ret;
	uint8_t reg8;
	uint16_t reg16;
	const struct reg_entry *entry = reg_list;

	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	if (argc == 2) {
		if (!strcasecmp(argv[1], "store")) {
			ccprintf("%s: updating persistent settings\n", __func__);
			ret = mp2949_store_user_all();
			usleep(1000 * MSEC);

			return ret;
		}
		else
			return EC_ERROR_PARAM1;
	}

	ccprintf("MPC2940 register dump\n");

	for (i = 0; i < reg_size; i++, entry++) {
		reg8 = 0;
		reg16 = 0;
		switch (entry->bytes) {
		default:
		case 0:
			break;
		case 1:
			mp2949_read8(entry->reg, &reg8);
			ccprintf("  %s(%02xh) = 0x  %02x\n",
				entry->name, entry->reg, reg8);
			break;
		case 2:
			mp2949_read16(entry->reg, &reg16);
			ccprintf("  %s(%02xh) = 0x%04x\n",
				entry->name, entry->reg, reg16);
			break;
		}

	}

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(mps, command_mps,
			"[store]",
			"Dmp MPC2940 registers, [store] saves registers");

void mps2940_chipset_startup(void)
{
	uint8_t reg8 = 0;


	/* Restore 3-phase operation */
	mp2949_read8(MP2940_MFR_PHASE_NUM, &reg8);
	ccprintf("MPS phase = %d\n", reg8);
	if (reg8 != 3) {
		reg8 = 3;
		mp2949_write8(MP2940_MFR_PHASE_NUM, reg8);
		mp2949_store_user_all();
		ccprintf("3-phase restored\n");
	}
}


