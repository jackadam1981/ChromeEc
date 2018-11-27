/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "Global.h"
#include "console.h"
#include "extension.h"
#include "hooks.h"
#include "timer.h"
#include "tpm_registers.h"
#include "tpm_vendor_cmds.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)

static void disable_tpm(void)
{
	tpm_stop();
}
DECLARE_DEFERRED(disable_tpm);

/*
 * On TPM reset event, tpm_reset_now() in tpm_registers.c clears TPM2 BSS memory
 * area. By placing s_tpm_mode in TPM2 BSS area, TPM mode value shall be
 * "TPM_MODE_ENABLED_TENTATIVE" on every TPM reset events.
 */
static enum tpm_modes s_tpm_mode __attribute__((section(".bss.Tpm2_common")));

static enum vendor_cmd_rc process_tpm_mode(struct vendor_cmd_params *p)
{
	size_t expected_size = 1;	// sizeof(uint8_t)
	uint8_t mode_val;
	uint8_t *buffer;

	p->out_size = 0;

	if (p->in_size > expected_size)
		return VENDOR_RC_NOT_ALLOWED;

	buffer = (uint8_t *)p->buffer;
	if (p->in_size == expected_size) {
		mode_val = buffer[0];
		if (mode_val != VENDOR_SC_GET_TPM_MODE &&
		    s_tpm_mode != TPM_MODE_ENABLED_TENTATIVE)
			return VENDOR_RC_NOT_ALLOWED;

		switch (mode_val) {
		case VENDOR_SC_ENABLE_TPM:
			s_tpm_mode = TPM_MODE_ENABLED;
			break;
		case VENDOR_SC_DISABLE_TPM:
			/*
			 * If it is to be disabled, call disable_tpm() deferred
			 * so that this vendor command can be responded to
			 * before TPM stops.
			 */
			s_tpm_mode = TPM_MODE_DISABLED;
			hook_call_deferred(&disable_tpm_data, 10 * MSEC);
			break;
		case VENDOR_SC_GET_TPM_MODE:
			break;
		default:
			return VENDOR_RC_NO_SUCH_SUBCOMMAND;
		}
	}

	p->out_size = expected_size;
	buffer[0] = (uint8_t)s_tpm_mode;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_TPM_MODE, process_tpm_mode);

enum tpm_modes get_tpm_mode(void)
{
	return s_tpm_mode;
}
