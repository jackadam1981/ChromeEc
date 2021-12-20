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
#include "util.h"

const static struct mp2964_reg_val rail_a[] = {
	{ MP2964_MFR_ALT_SET,  0xe081 },	/* ALERT_DELAY = 200ns */
};
const static struct mp2964_reg_val rail_b[] = {
	{ MP2964_MFR_ALT_SET,  0xe081 },	/* ALERT_DELAY = 200ns */
};

const static struct mp2964_reg_val rail_a_28wES_to_15wQS[] = {
	{ MP2964_MFR_VOUT_TRIM,		0x0ccd },
	{ MP2964_MFR_PHASE_NUM,		0x0002 },
	{ MP2964_MFR_IMON_SNS_OFFS,	0x0384 },
	{ MP2964_IOUT_CAL_GAIN_SET,	0x0060 },
	{ MP2964_MFR_TRANS_FAST,	0x1ac3 },
	{ MP2964_MFR_ICC_MAX_SET,	0x0050 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT,	0x68b0 },
	{ MP2964_MFR_OCP_SET,		0x0cb4 },
	{ MP2964_PRODUCT_DATA_CODE,	0x2b00 },
	{ MP2964_LOT_CODE_VR,		0x0003 },
	{ MP2964_MFR_PSI_TRIM4,		0x1926 },
	{ MP2964_MFR_PSI_TRIM1,		0x0000 },
	{ MP2964_MFR_PSI_TRIM3,		0x24a0 },
	{ MP2964_MFR_SLOPE_CNT_2P,	0x0000 },
	{ MP2964_MFR_SLOPE_CNT_5P,	0x0012 },
	{ MP2964_MFR_IMON_SVID1,	0x009a },
	{ MP2964_MFR_IMON_SVID2,	0x009a },
	{ MP2964_MFR_IMON_SVID3,	0x009a },
	{ MP2964_MFR_IMON_SVID4,	0x009a },
	{ MP2964_MFR_IMON_SVID5,	0x00b3 },
	{ MP2964_MFR_IMON_SVID6,	0x00b3 },
};

const static struct mp2964_reg_val rail_b_28wES_to_15wQS[] = {
	{ MP2964_MFR_VOUT_TRIM,		0x00dd },
	{ MP2964_MFR_PHASE_NUM,		0x0001 },
	{ MP2964_MFR_IMON_SNS_OFFS,	0x032b },
	{ MP2964_IOUT_CAL_GAIN_SET,	0x0038 },
	{ MP2964_MFR_TRANS_FAST,	0x1ac3 },
	{ MP2964_MFR_CONFIG2,		0x0140 },
	{ MP2964_MFR_SLOPE_SR_DCM,	0x0002 },
	{ MP2964_MFR_ICC_MAX_SET,	0x0028 },
	{ MP2964_MFR_OCP_OVP_DAC_LIMIT,	0x34b0 },
	{ MP2964_MFR_OCP_SET,		0x0cb4 },
	{ MP2964_MFR_PSI_TRIM4,		0x1926 },
};

const static struct mp2964_reg_val rail_a_15wES_to_15wQS[] = {
	{ MP2964_MFR_VOUT_TRIM,		0x0ccd },
	{ MP2964_MFR_TRANS_FAST,	0x1ac3 },
	{ MP2964_LOT_CODE_VR,		0x0003 },
	{ MP2964_MFR_PSI_TRIM4,		0x1926 },
	{ MP2964_MFR_PSI_TRIM1,		0x0000 },
	{ MP2964_MFR_PSI_TRIM3,		0x24a0 },
	{ MP2964_MFR_SLOPE_CNT_2P,	0x0000 },
};

const static struct mp2964_reg_val rail_b_15wES_to_15wQS[] = {
	{ MP2964_MFR_VOUT_TRIM,		0x00dd },
	{ MP2964_MFR_TRANS_FAST,	0x1ac3 },
	{ MP2964_MFR_CONFIG2,		0x0140 },
	{ MP2964_MFR_SLOPE_SR_DCM,	0x0002 },
	{ MP2964_MFR_PSI_TRIM4,		0x1926 },
};

static int mp2964_patch(int argc, char **argv)
{
	int status = EC_SUCCESS;

	ccprintf("MP2964 PATCH:\n 1. Power OFF the device\n 2. press power");
	ccprintf("button to power MP2964 ON\n 3. Use EC command `imvp9`\n");

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	ccprintf("%s: attempting to tune PMIC\n", __func__);

	if (!strcasecmp(argv[1], "P1")) {
		status = mp2964_tune(rail_a, ARRAY_SIZE(rail_a),
				     rail_b, ARRAY_SIZE(rail_b));
	} else if (!strcasecmp(argv[1], "28to15")) {
		status = mp2964_tune(rail_a_28wES_to_15wQS,
				     ARRAY_SIZE(rail_a_28wES_to_15wQS),
				     rail_b_28wES_to_15wQS,
				     ARRAY_SIZE(rail_b_28wES_to_15wQS));

	} else if (!strcasecmp(argv[1], "15to15")) {
		status = mp2964_tune(rail_a_15wES_to_15wQS,
				     ARRAY_SIZE(rail_a_15wES_to_15wQS),
				     rail_b_15wES_to_15wQS,
				     ARRAY_SIZE(rail_b_15wES_to_15wQS));
	} else {
		ccprintf("ERROR: param1 has to be one of P1|15to15|28to15\n");
		return EC_ERROR_PARAM1;
	}

	if (status != EC_SUCCESS) {
		ccprintf("%s: could not update all settings\n", __func__);
		return status;
	}
	ccprintf("%s: IMVP9 update done\n", __func__);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(imvp9, mp2964_patch,
			"P1|15to15|28to15",
			"Tune imvp9.1 for differtent scenarios");
