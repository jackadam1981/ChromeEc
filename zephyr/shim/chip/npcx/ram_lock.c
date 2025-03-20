/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ram_lock.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/syscon.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shim_cros_ram_lock, LOG_LEVEL_ERR);

#define NPCX_RAM_LK_CTL 0x001
#define NPCX_RAM_LK_FETCH_BF 1
#define NPCX_RAM_WRITE_LOCK(n) (0x022 + (n))
#define NPCX_RAM_FETCH_LOCK(n) (0x042 + (n))

#define NPCX_RAM_BASE 0x10058000
#define NPCX_RAMLOCK_MAXSIZE 0x80000
#define NPCX_RAM_SECTOR 0x1000
#define NPCX_RAM_BLOCK 0x8000
#define NPCX_RAM_ALIAS_SHIFT 0x10000000

static const struct device *ramlock_dev = DEVICE_DT_GET(DT_NODELABEL(ramlock0));

static int set_lock_bit(uint32_t offset, uint8_t lock_bit)
{
	int ret;

	ret = syscon_write_reg(ramlock_dev, offset, lock_bit);

	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int fetch_bustrap_enable(void)
{
	uint32_t npcx_lk_ctl_value = 0x0;
	int ret;

	ret = syscon_read_reg(ramlock_dev, NPCX_RAM_LK_CTL, &npcx_lk_ctl_value);

	if (ret != 0) {
		return ret;
	}

	if ((npcx_lk_ctl_value & BIT(NPCX_RAM_LK_FETCH_BF)) == 0) {
		npcx_lk_ctl_value |= BIT(NPCX_RAM_LK_FETCH_BF);
		ret = syscon_write_reg(ramlock_dev, NPCX_RAM_LK_CTL,
				       npcx_lk_ctl_value);

		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static int ram_lock_update_lock_region(uint8_t region, uint32_t addr,
				       uint8_t lock_bit)
{
	uint32_t offset;
	int ret;

	addr = (region == REGION_DATA_RAM) ? addr - NPCX_RAM_ALIAS_SHIFT : addr;
	offset = addr - NPCX_RAM_BASE;

	if (offset > NPCX_RAMLOCK_MAXSIZE || offset < 0) {
		return -EINVAL;
	}

	if (region == REGION_DATA_RAM) {
		ret = set_lock_bit(NPCX_RAM_FETCH_LOCK(offset / NPCX_RAM_BLOCK),
				   lock_bit);
		if (ret != 0) {
			LOG_ERR("Set Fetch Lock FAIL %x", ret);
			return ret;
		}

		ret = fetch_bustrap_enable();
		if (ret != 0) {
			LOG_ERR("Enable Fetch Bustrap FAIL %x", ret);
			return ret;
		}
	} else if (region == REGION_STORAGE) {
		ret = set_lock_bit(NPCX_RAM_WRITE_LOCK(offset / NPCX_RAM_BLOCK),
				   lock_bit);
		if (ret != 0) {
			LOG_ERR("Set Write Lock FAIL %x", ret);
			return ret;
		}
	}

	return 0;
}

int ram_lock_config_lock_region(uint8_t region, uint32_t addr, uint32_t size)
{
	int sr_idx;
	uint32_t subregion_base, subregion_size;
	uint8_t natural_alignment = LOG2(NPCX_RAM_SECTOR);
	uint8_t lock_region = 0;

	/* Check address is aligned */
	if (addr & (NPCX_RAM_SECTOR - 1)) {
		return -EINVAL;
	}

	/* Check size is aligned */
	if (size & (NPCX_RAM_SECTOR - 1)) {
		return -EINVAL;
	}

	/*
	 * Depending on the block alignment this can allow us to cover a larger
	 * area. Generate the subregion mask by walking through each, locking if
	 * it is contained in the requested range.
	 */
	natural_alignment += 3;
	subregion_base = addr & ~BIT_MASK(natural_alignment);
	subregion_size = 1 << (natural_alignment - 3);

	do {
		for (sr_idx = 0; sr_idx < 8; sr_idx++) {
			if (subregion_base >= addr &&
			    (subregion_base + subregion_size) <= (addr + size))
				lock_region |= 1 << sr_idx;

			subregion_base += subregion_size;
		}

		ram_lock_update_lock_region(
			region, subregion_base - (8 * subregion_size),
			lock_region);
		lock_region = 0;
	} while (subregion_base < (addr + size));

	return 0;
}
