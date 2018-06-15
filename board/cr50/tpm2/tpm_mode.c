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
#include "tpm_registers.h"
#include "tpm_vendor_cmds.h"
#include "i2cs.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)


static enum tpm_modes s_tpm_mode __attribute__((section(".bss.Tpm2_common")));

static int tpm_is_enabled(void)
{
	return (s_tpm_mode == tpm_mode_enabled_tentative)
		 || (s_tpm_mode == tpm_mode_enabled);
}

static void tpm_mode_query_if_register(void)
{
	CPRINTS("%s\n", __func__);
	tpm_register_interface_query(tpm_is_enabled);
}
DECLARE_HOOK(HOOK_INIT, tpm_mode_query_if_register, HOOK_PRIO_LAST);

static enum vendor_cmd_rc enable_tpm_mode(struct vendor_cmd_params *p)
{
	p->out_size = 0;

	if (s_tpm_mode != tpm_mode_enabled_tentative)
		return VENDOR_RC_NOT_ALLOWED;

	s_tpm_mode = tpm_mode_enabled;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_ENABLE_TPM_MODE, enable_tpm_mode);

static enum vendor_cmd_rc disable_tpm_mode(struct vendor_cmd_params *p)
{
	p->out_size = 0;

	if (s_tpm_mode != tpm_mode_enabled_tentative)
		return VENDOR_RC_NOT_ALLOWED;

	s_tpm_mode = tpm_mode_disabled;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_DISABLE_TPM_MODE, disable_tpm_mode);

#ifdef CR50_DEV
static enum vendor_cmd_rc get_tpm_mode(struct vendor_cmd_params *p)
{
	uint32_t mode_val;

	mode_val = be32toh(s_tpm_mode);

	memcpy(p->buffer, &mode_val, sizeof(uint32_t));
	p->out_size = sizeof(uint32_t);

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_GET_TPM_MODE, get_tpm_mode);
#endif
