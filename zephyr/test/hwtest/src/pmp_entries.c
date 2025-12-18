// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

extern "C" {
void pmp_decode_region(uint8_t cfg_byte, unsigned long *pmp_addr,
		       unsigned int index, unsigned long *start,
		       unsigned long *end);
void z_riscv_pmp_read_config(unsigned long *pmp_cfg, size_t pmp_cfg_size);
void z_riscv_pmp_read_addr(unsigned long *pmp_addr, size_t pmp_addr_size);
}

LOG_MODULE_REGISTER(pmp_entries, LOG_LEVEL_INF);

ZTEST_SUITE(pmp_entries, NULL, NULL, NULL, NULL, NULL);

ZTEST(pmp_entries, test_pmp_entries)
{
    	const size_t num_pmpcfg_regs = CONFIG_PMP_SLOTS / sizeof(unsigned long);
	const size_t num_pmpaddr_regs = CONFIG_PMP_SLOTS;

	unsigned long current_pmpcfg_regs[num_pmpcfg_regs];
	unsigned long current_pmpaddr_regs[num_pmpaddr_regs];

	/* Read the current PMP configuration from the control registers */
	z_riscv_pmp_read_config(current_pmpcfg_regs, num_pmpcfg_regs);
	z_riscv_pmp_read_addr(current_pmpaddr_regs, num_pmpaddr_regs);

	const uint8_t *const current_pmp_cfg_entries = (const uint8_t *)current_pmpcfg_regs;

    for (int i = 0; i < CONFIG_PMP_SLOTS; ++i)
    {
        LOG_INF("addr/cfg: 0x%08lx / 0x%02x", current_pmpaddr_regs[i], current_pmpcfg_regs[i]);
    }

}
