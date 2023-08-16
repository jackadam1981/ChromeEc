/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_AMD_SB_RMI_H
#define __EMUL_AMD_SB_RMI_H

#include "driver/sb_rmi.h"
#include "emul/emul_common_i2c.h"

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

#define SB_RMI_STATUS_REG_RESERVED GENMASK(7, 2)
#define SB_RMI_SW_INTR_REG_RESERVED GENMASK(7, 1)

/* These registers are fully reserved. */
#define SB_RMI_IN_BND_MSG5_REG_RESERVED 0xFF
#define SB_RMI_IN_BND_MSG6_REG_RESERVED 0xFF

#define SB_RMI_OUT_BND_MSG5_REG_RESERVED 0xFF
#define SB_RMI_OUT_BND_MSG6_REG_RESERVED 0xFF

#define SB_RMI_REG_MAX (SB_RMI_SW_INTR_REG + 1)

#define SB_RMI_MAILBOX_REG_COUNT 8

#define AMD_SB_RMI_MAILBOX_EMUL_DT_INST_DEFINE(inst, init_fn, data_ptr, cfg_ptr, api) \
	EMUL_DT_INST_DEFINE(inst, init_fn, data_ptr, cfg_ptr, NULL, api)

/* Constant configuration of the emulator */
struct amd_sb_rmi_emul_cfg {
	const struct i2c_common_emul_cfg common;

	const struct device **mailboxes;
	size_t num_mailboxes;
};

struct amd_sb_rmi_register {
	uint8_t reg;
	uint8_t value;
	uint8_t def;
	uint8_t reserved;
};

struct amd_sb_rmi_emul_data {
	struct i2c_common_emul_data common;
	sys_slist_t mailboxes;

	struct amd_sb_rmi_register regs[SB_RMI_REG_MAX];
};

struct amd_sb_rmi_mailbox_emul_api;

struct amd_sb_rmi_mailbox_emul_cfg {
	const uint8_t *commands;
	size_t num_commands;
};

struct amd_sb_rmi_mailbox_emul {
	sys_snode_t node;
	const struct emul *target;
	const struct amd_sb_rmi_mailbox_emul_api *api;
};

struct amd_sb_rmi_mailbox_emul_api {
	/* Perform the mailbox command. */
	int (*handle_command)(const struct emul *emul, uint8_t cmd, uint32_t in_data, uint32_t *out_data);

	const struct amd_sb_rmi_mailbox_emul_cfg *(*get_config)(const struct emul *emul);

	/*
	 * Normally, sub-emulators are stored within the emul.bus member. However that can only
	 * store standard bus emulators (I2C, SPI, etc). This function provides a way for the
	 * emulator to provide its own emulator struct.
	 */
	struct amd_sb_rmi_mailbox_emul *(*get_mailbox_emul)(const struct emul *emul);
	
};

/* Directly gets the value of the given register */
int amd_sb_rmi_emul_get_reg(const struct emul *emul, int r, uint8_t *val);

/* 
 * Directly sets the value of the register, does not invoke any other 
 * functionality
 */
int amd_sb_rmi_emul_set_reg(const struct emul *emul, int r, uint8_t val);

void amd_sb_rmi_emul_mailbox_finish(const struct emul *sb_rmi, uint8_t cmd, uint8_t err);

int amd_sb_rmi_emul_mailbox_register(const struct device *dev, struct amd_sb_rmi_mailbox_emul *emul);

void amd_sb_rmi_emul_reset(const struct emul *emul);

#endif /* __EMUL_AMD_SB_RMI_H */