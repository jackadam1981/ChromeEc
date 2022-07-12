/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Tune the MP2964 IMVP9.1 parameters for skolas */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "hooks.h"
#include "mp2964.h"

const static struct mp2964_reg_val rail_a[] = {
	{ MP2964_MFR_GATECLK_DIS,     0x9c00 },	/* Disable POWER_SAVE */
};
const static struct mp2964_reg_val rail_b[] = {
	{ MP2964_MFR_GATECLK_DIS,     0x9c00 },	/* Disable POWER_SAVE */
};


static int command_tune_mp2964(int argc, char **argv)
{
	static int chip_updated;
	int status;
	int board_id;

	board_id = get_board_id();

	/* Skolas */
	if (board_id != 4) {
		ccprintf("Board ID %d is not supported\n", board_id);
		return EC_ERROR_INVAL;
	}

	if (chip_updated) {
		ccprintf("PMIC already updated\n");
		return EC_SUCCESS;
	}

	ccprintf("%s: attempting to tune PMIC\n", __func__);

	status = mp2964_tune(rail_a, ARRAY_SIZE(rail_a),
			     rail_b, ARRAY_SIZE(rail_b));
	if (status != EC_SUCCESS)
		ccprintf("%s: could not update all settings\n", __func__);

	chip_updated = 1;

	return EC_SUCCESS;
};

DECLARE_CONSOLE_COMMAND(tune_mp2964, command_tune_mp2964, "",
			"Tune MP2964 for Skolas");
