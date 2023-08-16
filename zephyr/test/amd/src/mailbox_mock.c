/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/emul_amd_sb_rmi.h"
#include "emul/emul_stub_device.h"
#include "driver/sb_rmi.h"

#define DT_DRV_COMPAT cros_sb_rmi_mailbox_mock

static int amd_sb_rmi_mailbox_mock_handle_command(const struct emul *emul, uint8_t cmd, uint32_t in_data, uint32_t *out_data)
{
    *out_data = in_data;
    return SB_RMI_MAILBOX_SUCCESS;
}

static struct amd_sb_rmi_mailbox_emul *amd_sb_rmi_mailbox_mock_get_emul(const struct emul *emul)
{
    return emul->data;
}

const struct amd_sb_rmi_mailbox_emul_cfg *amd_sb_rmi_mailbox_mock_get_config(const struct emul *emul)
{
	return emul->cfg;
}

const static struct amd_sb_rmi_mailbox_emul_api amd_sb_rmi_mailbox_mock_api = {
    .handle_command = amd_sb_rmi_mailbox_mock_handle_command,
    .get_mailbox_emul = amd_sb_rmi_mailbox_mock_get_emul,
    .get_config = amd_sb_rmi_mailbox_mock_get_config,
};

static int amd_sb_rmi_mailbox_mock_init(const struct emul *emul,
			     const struct device *parent)
{
	struct amd_sb_rmi_mailbox_emul *mailbox = emul->data;

	mailbox->target = emul;
	mailbox->api = &amd_sb_rmi_mailbox_mock_api;
	return 0;
}

#define AMD_SB_RMI_MAILBOX_MOCK(n) \
	static const struct amd_sb_rmi_mailbox_emul_cfg \
		amd_sb_rmi_mailbox_emul_mock_cfg_##n = { \
		.commands = NULL, \
		.num_commands = 0, \
	}; \
	static struct amd_sb_rmi_mailbox_emul amd_sb_rmi_mailbox_emul_mock_##n; \
	AMD_SB_RMI_MAILBOX_EMUL_DT_INST_DEFINE(n, amd_sb_rmi_mailbox_mock_init, \
			&amd_sb_rmi_mailbox_emul_mock_##n, \
			&amd_sb_rmi_mailbox_emul_mock_cfg_##n, \
			&amd_sb_rmi_mailbox_mock_api) 

DT_INST_FOREACH_STATUS_OKAY(AMD_SB_RMI_MAILBOX_MOCK)
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE)