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

/* Constant configuration of the emulator */
struct amd_sb_rmi_emul_cfg {
	const struct i2c_common_emul_cfg common;
};

struct amd_sb_rmi_register {
	uint8_t reg;
	uint8_t value;
	uint8_t def;
	uint8_t reserved;
};

struct amd_sb_rmi_emul_data {
	struct i2c_common_emul_data common;

	struct amd_sb_rmi_register regs[SB_RMI_REG_MAX];
};

/* Directly gets the value of the given register */
int amd_sb_rmi_emul_get_reg(const struct emul *emul, int r, uint8_t *val);

/* 
 * Directly sets the value of the register, does not invoke any other 
 * functionality
 */
int amd_sb_rmi_emul_set_reg(const struct emul *emul, int r, uint8_t val);

void amd_sb_rmi_emul_reset(const struct emul *emul);

#endif /* __EMUL_AMD_SB_RMI_H */