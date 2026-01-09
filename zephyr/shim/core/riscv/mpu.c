// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hooks.h"
#include "system.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <zephyr/arch/riscv/pmp.h>
#include <zephyr/mem_mgmt/mem_attr.h>

#define ROLLBACK_NODE DT_NODELABEL(rollback)

static void prepare_for_sysjump_to_ec(void)
{
	/*
	 * Clear all Physical Memory Protection (PMP) entries before jumping to
	 * the new image. When CONFIG_HW_STACK_PROTECTION is enabled, the
	 * current image's PMP configuration for stack boundaries can interfere
	 * with the stack setup of the new image, leading to crashes. Clearing
	 * ensures the new image starts with a clean PMP state.
	 */
	z_riscv_pmp_clear_all();
}
DECLARE_HOOK(HOOK_SYSJUMP, prepare_for_sysjump_to_ec, HOOK_PRIO_LAST);

#if defined(CONFIG_PLATFORM_EC_ROLLBACK_MPU_PROTECT) && defined(CONFIG_MEM_ATTR)
/**
 * @brief Find the index of a memory region by its DeviceTree node name.
 *
 * @param target_name The DT_NODE_FULL_NAME to search for.
 *
 * @return The index in the regions array, or -1 if not found.
 */
static int get_mem_region_index_by_name(const char *target_name)
{
	const struct mem_attr_region_t *regions;
	size_t num_regions;

	num_regions = mem_attr_get_regions(&regions);

	for (int i = 0; i < num_regions; ++i) {
		if (regions[i].dt_name != NULL &&
		    strcmp(regions[i].dt_name, target_name) == 0) {
			return i;
		}
	}

	return -ENOENT;
}

int mpu_lock_rollback(int lock)
{
	const char *rollback_node_name = DT_NODE_FULL_NAME(ROLLBACK_NODE);
	int rollback_region_idx =
		get_mem_region_index_by_name(rollback_node_name);

	assert(rollback_region_idx != -ENOENT);

	if (lock) {
		z_riscv_pmp_change_permissions(rollback_region_idx, 0);
	} else {
		z_riscv_pmp_change_permissions(rollback_region_idx,
					       PMP_R | PMP_W);
	}
	return 0;
}
#endif
