/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "Global.h"
#include "board.h"
#include "console.h"
#include "endian.h"
#include "extension.h"
#include "hooks.h"
#include "util.h"
#include "timer.h"
#include "tpm_registers.h"
#include "tpm_vendor_cmds.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)

DECLARE_DEFERRED(tpm_stop);

static enum tpm_modes s_tpm_mode __attribute__((section(".bss.Tpm2_common")));

static enum vendor_cmd_rc set_tpm_mode(struct vendor_cmd_params *p)
{
	uint32_t mode_val;

	p->out_size = 0;

	if (s_tpm_mode != tpm_mode_enabled_tentative)
		return VENDOR_RC_NOT_ALLOWED;

	if (p->in_size != sizeof(uint32_t))
		return VENDOR_RC_NOT_ALLOWED;

	mode_val = be32toh(((uint32_t *)p->buffer)[0]);
	if (mode_val == tpm_mode_disabled)
		hook_call_deferred(&tpm_stop_data, 10 * MSEC);
	else if (mode_val != tpm_mode_enabled)
		return VENDOR_RC_NOT_ALLOWED;

	s_tpm_mode = mode_val;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_SET_TPM_MODE, set_tpm_mode);

enum tpm_modes get_tpm_mode(void)
{
	return s_tpm_mode;
}

