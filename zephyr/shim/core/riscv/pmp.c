// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdint.h>

#include <zephyr/arch/riscv/csr.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shim_pmp, LOG_LEVEL_ERR);

#define PMP_CFG_W_BIT 1 // Write permission bit in the PMP config byte
#define PMPCFG_STRIDE sizeof(unsigned long)
#define PMP_ADDR(addr) ((addr) >> 2)
#define NAPOT_RANGE(size) (((size) - 1) >> 1)
#define PMP_ADDR_NAPOT(addr, size) PMP_ADDR(addr | NAPOT_RANGE(size))

/**
 * @brief Read the content of a specific PMP Address Register (pmpaddrX).
 *
 * This helper function provides a way to read the memory address configuration
 * from one of the first 8 PMP address registers (pmpaddr0 through pmpaddr7).
 *
 * @param index The PMP entry index (0-7) corresponding to the desired register.
 * @return The 64-bit value (address/length) stored in the pmpaddrX CSR.
 */
static inline unsigned long read_pmpaddr(uint8_t index)
{
	switch (index) {
	case 0:
		return csr_read(pmpaddr0);
	case 1:
		return csr_read(pmpaddr1);
	case 2:
		return csr_read(pmpaddr2);
	case 3:
		return csr_read(pmpaddr3);
	case 4:
		return csr_read(pmpaddr4);
	case 5:
		return csr_read(pmpaddr5);
	case 6:
		return csr_read(pmpaddr6);
	case 7:
		return csr_read(pmpaddr7);
	default:
		return 0;
	}
}

int riscv_pmp_set_write_permission(bool write_enable)
{
	if (CONFIG_PMP_SLOTS > 8) {
		LOG_ERR("This function only supports up to 8 PMP slots.");
		return -ENOTSUP;
	}

	int entry_index = -1;
	uintptr_t target_pmpaddr_start =
		PMP_ADDR(CONFIG_CUSTOM_PMP_ENTRY_START);
	uintptr_t target_pmpaddr_end = PMP_ADDR(CONFIG_CUSTOM_PMP_ENTRY_START +
						CONFIG_CUSTOM_PMP_ENTRY_SIZE);
	uintptr_t target_pmpaddr_napot = PMP_ADDR_NAPOT(
		CONFIG_CUSTOM_PMP_ENTRY_START, CONFIG_CUSTOM_PMP_ENTRY_SIZE);

	for (uint8_t i = 0; i < CONFIG_PMP_SLOTS; ++i) {
		if ((read_pmpaddr(i) == target_pmpaddr_start) &&
		    (read_pmpaddr(i + 1) == target_pmpaddr_end)) {
			entry_index = i + 1;
			break;
		}
		if (read_pmpaddr(i) == target_pmpaddr_napot) {
			entry_index = i;
			break;
		}
	}

	if (entry_index == -1) {
		LOG_ERR("PMP entry for address 0x%x not found",
			CONFIG_CUSTOM_PMP_ENTRY_START);
		return -ENOENT;
	}

	uint8_t cfg_reg_idx = entry_index / PMPCFG_STRIDE;
	uint8_t entry_in_reg = entry_index % PMPCFG_STRIDE;
	int bit_position = (entry_in_reg * 8) + PMP_CFG_W_BIT;
	unsigned long mask = 1UL << bit_position;

	unsigned long pmpcfg_val;

#if defined(CONFIG_64BIT)
	// RV64: pmpcfg0 holds configs for entries 0-7
	// pmpcfg registers are pmpcfg0, pmpcfg2, pmpcfg4, ...
	if (cfg_reg_idx == 0) { // Entries 0-7 are in pmpcfg0
		pmpcfg_val = csr_read(pmpcfg0);
		if (write_enable) {
			pmpcfg_val |= mask;
		} else {
			pmpcfg_val &= ~mask;
		}
		csr_write(pmpcfg0, pmpcfg_val);
	} else {
		LOG_ERR("cfg_reg_idx %d unexpected for <= 8 slots on RV64",
			cfg_reg_idx);
		return -EINVAL;
	}
#else
	// RV32: pmpcfg0 for entries 0-3, pmpcfg1 for entries 4-7
	// pmpcfg registers are pmpcfg0, pmpcfg1, pmpcfg2, ...
	if (cfg_reg_idx == 0) { // Entries 0-3
		pmpcfg_val = csr_read(pmpcfg0);
		if (write_enable) {
			pmpcfg_val |= mask;
		} else {
			pmpcfg_val &= ~mask;
		}
		csr_write(pmpcfg0, pmpcfg_val);
	} else if (cfg_reg_idx == 1) { // Entries 4-7
		pmpcfg_val = csr_read(pmpcfg1);
		if (write_enable) {
			pmpcfg_val |= mask;
		} else {
			pmpcfg_val &= ~mask;
		}
		csr_write(pmpcfg1, pmpcfg_val);
	} else {
		LOG_ERR("cfg_reg_idx %d unexpected for <= 8 slots on RV32",
			cfg_reg_idx);
		return -EINVAL;
	}
#endif

	return 0;
}
