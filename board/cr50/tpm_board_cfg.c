/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This implements TPM_BOARD_CFG register and its read|write functions.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "tpm_board_cfg.h"


static void deferred_process_board_cfg(void)
{
	uint32_t tpm_board_cfg = GREG32(PMU, PWRDN_SCRATCH21);

	if (tpm_board_cfg & BITMASK_LONG_INT_AP_PULSE)
		int_ap_extension_enable();
}
DECLARE_DEFERRED(deferred_process_board_cfg);

void board_cfg_reg_write(uint32_t value)
{
	/* Lock the program. */
	value |= BITMASK_PROGRAMMED_LOCKED;

	/* If PWRDN_SCRATCH21 is already written, then do nothing but return. */
	if (GREG32(PMU, PWRDN_SCRATCH21))
		return;

	/* Store the tpm_board_cfg in power-down scratch. */
	GREG32(PMU, PWRDN_SCRATCH21) = value;

	/* Process board_configuration change in a deferred function */
	hook_call_deferred(&deferred_process_board_cfg_data, 0);
}

uint32_t board_cfg_reg_read(void)
{
	return GREG32(PMU, PWRDN_SCRATCH21);
}

/**
 * Console command to display TPM_BOARD_CFG register value.
 */
static int command_brdcfg(int argc, char **argv)
{
#ifdef CR50_DEV
	if (argc > 1) {
		uint32_t val;
		char *e;

		val = strtoi(argv[1], &e, 16);
		if (*e)
			return EC_ERROR_PARAM1;

		board_cfg_reg_write(val);
	}
#endif
	ccprintf("TPM_BOARD_CFG = 0x%08x\n", board_cfg_reg_read());
	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(brdcfg, command_brdcfg, NULL,
			     "Display TPM_BOARD_CFG value");
