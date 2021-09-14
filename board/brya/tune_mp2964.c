/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Tune the MP2964 IMVP9.1 parameters for brya */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "hooks.h"
#include "mp2964.h"

#define MP2964_TUNE_15W

const static struct mp2964_reg_val rail_a[] = {
	{ MP2964_MFR_ALT_SET,     0xe081 },	/* ALERT_DELAY = 200ns */
};
const static struct mp2964_reg_val rail_b[] = {
	{ MP2964_MFR_ALT_SET,     0xe081 },	/* ALERT_DELAY = 200ns */
};

#if defined(MP2964_TUNE_15W)
const static struct mp2964_reg_val rail_a_15W[] = {
	{ MP2964_MFR_PHASE_NUM,      0x0002 },
	{ MP2964_IOUT_CAL_GAIN_SET,  0x0060 },
	{ MP2964_MFR_ICC_MAX_SET,    0x0050 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT, 0x68B0 },
	{ MP2964_MFR_OCP_SET,        0x0CB4 },
	{ MP2964_PRODUCT_DATA_CODE,  0x2B00 },
	{ MP2964_LOT_CODE_VR,        0x0002 },
	{ MP2964_MFR_SLOPE_CNT_5P,   0x0012 },
	{ MP2964_MFR_IMON_SVID1,     0x009A },
	{ MP2964_MFR_IMON_SVID2,     0x009A },
	{ MP2964_MFR_IMON_SVID3,     0x009A },
	{ MP2964_MFR_IMON_SVID4,     0x009A },
	{ MP2964_MFR_IMON_SVID5,     0x00B3 },
	{ MP2964_MFR_IMON_SVID6,     0x00B3 },
	{ MP2964_MFR_VOUT_TRIM,     0x0CCD },
	{ MP2964_MFR_IMON_SNS_OFFS, 0x039D },
	{ MP2964_MFR_TRANS_FAST,    0x1AC3 },
	{ MP2964_MFR_PSI_TRIM4,     0x1926 },
	{ MP2964_MFR_PSI_TRIM1,     0x0000 },
	{ MP2964_MFR_PSI_TRIM3,     0x24A0 },
	{ MP2964_MFR_SLOPE_CNT_2P,  0x0000 },
};

const static struct mp2964_reg_val rail_b_15W[] = {
	{ MP2964_MFR_PHASE_NUM,     0x0001 },
	{ MP2964_IOUT_CAL_GAIN_SET, 0x0038 },
	{ MP2964_MFR_ICC_MAX_SET,   0x0028 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT, 0x34B0 },
	{ MP2964_MFR_OCP_SET,       0x0CB4 },
	{ MP2964_MFR_VOUT_TRIM,     0x00DD },
	{ MP2964_MFR_IMON_SNS_OFFS, 0x0351 },
	{ MP2964_MFR_TRANS_FAST,    0x1AC3 },
	{ MP2964_MFR_CONFIG2,       0x0140 },
	{ MP2964_MFR_SLOPE_SR_DCM,  0x0002 },
	{ MP2964_MFR_PSI_TRIM4,     0x1926 },
};

#else

const static struct mp2964_reg_val rail_a_28W[] = {
	{ MP2964_MFR_PHASE_NUM,      0x0002 },
	{ MP2964_IOUT_CAL_GAIN_SET,  0x0060 },
	{ MP2964_MFR_ICC_MAX_SET,    0x0050 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT, 0x68B0 },
	{ MP2964_MFR_OCP_SET,        0x0CB4 },
	{ MP2964_PRODUCT_DATA_CODE,  0x2B00 },
	{ MP2964_LOT_CODE_VR,        0x0002 },
	{ MP2964_MFR_SLOPE_CNT_5P,   0x0012 },
	{ MP2964_MFR_IMON_SVID1,     0x009A },
	{ MP2964_MFR_IMON_SVID2,     0x009A },
	{ MP2964_MFR_IMON_SVID3,     0x009A },
	{ MP2964_MFR_IMON_SVID4,     0x009A },
	{ MP2964_MFR_IMON_SVID5,     0x00B3 },
	{ MP2964_MFR_IMON_SVID6,     0x00B3 },
	{ MP2964_MFR_VOUT_TRIM,     0x0000 },
	{ MP2964_MFR_IMON_SNS_OFFS, 0x0384 },
	{ MP2964_MFR_TRANS_FAST,    0x1BC3 },
	{ MP2964_MFR_PSI_TRIM4,     0x10A4 },
	{ MP2964_MFR_PSI_TRIM1,     0x14A5 },
	{ MP2964_MFR_PSI_TRIM3,     0x14A0 },
	{ MP2964_MFR_SLOPE_CNT_2P,  0x0063 },
};

const static struct mp2964_reg_val rail_b_28W[] = {
	{ MP2964_MFR_PHASE_NUM,     0x0001 },
	{ MP2964_IOUT_CAL_GAIN_SET, 0x0038 },
	{ MP2964_MFR_ICC_MAX_SET,   0x0028 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT, 0x34B0 },
	{ MP2964_MFR_OCP_SET,       0x0CB4 },
	{ MP2964_MFR_VOUT_TRIM,     0x0000 },
	{ MP2964_MFR_IMON_SNS_OFFS, 0x032B },
	{ MP2964_MFR_TRANS_FAST,    0x1BC3 },
	{ MP2964_MFR_CONFIG2,       0x00C0 },
	{ MP2964_MFR_SLOPE_SR_DCM,  0x0001 },
	{ MP2964_MFR_PSI_TRIM4,     0x10A4 },
};
#endif

static void mp2964_on_startup(void)
{
	static int chip_updated;
	int status = EC_SUCCESS;

	if (get_board_id() != 1 && get_board_id() != 2)
		return;

	if (chip_updated)
		return;

	chip_updated = 1;

	ccprintf("%s: attempting to tune PMIC\n", __func__);

	if (get_board_id() == 1) {
		status = mp2964_tune(rail_a, ARRAY_SIZE(rail_a),
			     rail_b, ARRAY_SIZE(rail_b));
	}

	if (get_board_id() == 2) {
#if defined(MP2964_TUNE_15W)
		status = mp2964_tune(rail_a_15W, ARRAY_SIZE(rail_a_15W),
			     rail_b_15W, ARRAY_SIZE(rail_b_15W));
#else
		status = mp2964_tune(rail_a_28W, ARRAY_SIZE(rail_a_28W),
			     rail_b_28W, ARRAY_SIZE(rail_b_28W));
#endif
	}

	if (status != EC_SUCCESS)
		ccprintf("%s: could not update all settings\n", __func__);
}

DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp2964_on_startup,
	     HOOK_PRIO_FIRST);
