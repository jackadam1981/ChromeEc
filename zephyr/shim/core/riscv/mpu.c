/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "mpu.h" // Zephyr's generic MPU header

#include <zephyr/arch/cpu.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <kernel_arch_data.h> // Keep for Zephyr-related definitions if needed

LOG_MODULE_REGISTER(shim_mpu, LOG_LEVEL_ERR);

// mpu_config is a global struct typically defined and populated by Zephyr's
// MPU Kconfig and architecture-specific MPU driver. It contains information
// about the pre-configured MPU regions.
// extern struct mpu_config mpu_config; // Assumed to be available via Zephyr
// headers

/**
 * @brief Enables all MPU regions defined in mpu_config.
 *
 * On RISC-V with PMP, there isn't a global "MPU enable" register like ARM.
 * Instead, each PMP entry's "A" (access) field determines if it's active.
 * This function iterates through the Zephyr-managed MPU regions and enables
 * each one using the generic `mpu_region_enable` API.
 */
void mpu_enable(void)
{
	for (int index = 0; index < mpu_config.num_regions; index++) {
		// Use Zephyr's generic API to enable the region
		mpu_region_enable(index);

		// Retrieve and log using Zephyr's generic MPU API.
		uint32_t base;
		size_t size;
		mpu_attr_t attr; // Use generic mpu_attr_t

		if (mpu_region_get_parameters(index, &base, &size, &attr) ==
		    0) {
			LOG_DBG("[%d] Base: 0x%08x Size: %zu Attr: 0x%x (Enabled)",
				index, base, size, attr);
		} else {
			LOG_DBG("[%d] Error getting region parameters.", index);
		}
	}
}

/**
 * @brief Disables all fixed MPU regions defined in mpu_config.
 *
 * This function is typically called early in the kernel initialization
 * to disable default Zephyr-configured MPU regions if they are not
 * desired to be active by default.
 */
static int mpu_disable_fixed_regions(void)
{
	for (int index = 0; index < mpu_config.num_regions; index++) {
		// Use Zephyr's generic API to disable the region
		mpu_region_disable(index);

		// Log region parameters.
		uint32_t base;
		size_t size;
		mpu_attr_t attr;

		if (mpu_region_get_parameters(index, &base, &size, &attr) ==
		    0) {
			LOG_DBG("[%d] Base: 0x%08x Size: %zu Attr: 0x%x (Disabled)",
				index, base, size, attr);
		} else {
			LOG_DBG("[%d] Error getting region parameters.", index);
		}
	}

	return 0;
}

// Register this function to be called during early kernel initialization.
SYS_INIT(mpu_disable_fixed_regions, PRE_KERNEL_1, 50);

#ifdef CONFIG_PLATFORM_EC_ROLLBACK_MPU_PROTECT

// This variable will store the starting ID for the dynamically configured
// rollback MPU regions.
static int mpu_static_rollback_region_id = -1;

// Define the MPU attribute for rollback protection.
// K_MEM_PARTITION_P_NA_U_NA typically means "no access" for both privileged
// and unprivileged modes. For Zephyr's generic MPU API (and RISC-V PMP),
// setting the attribute to 0 effectively means no read, write, or execute
// permissions, making the region inaccessible when enabled.
#define MPU_ATTR_ROLLBACK_PROTECT 0

/**
 * @brief Configures static MPU regions for rollback protection.
 *
 * This function dynamically configures two additional MPU regions to protect
 * rollback areas. It assumes that available MPU region IDs can be used
 * immediately following the regions managed by `mpu_config.num_regions`.
 * It uses Zephyr's generic `mpu_region_configure` API.
 */
static int mpu_add_static_rollback_regions(void)
{
	// Get the starting ID for these new regions.
	// We assume they start immediately after the existing configured
	// regions managed by Zephyr's mpu_config.
	int start_region_id = mpu_config.num_regions;

	// Configure the first rollback region.
	int result0 = mpu_region_configure(
		start_region_id,
		CONFIG_MAPPED_STORAGE_BASE +
			DT_REG_ADDR(DT_NODELABEL(rollback0)),
		DT_REG_SIZE(DT_NODELABEL(rollback0)),
		MPU_ATTR_ROLLBACK_PROTECT);

	if (result0 != 0) {
		LOG_ERR("Failed to configure rollback0 MPU region (ID %d): %d",
			start_region_id, result0);
		return result0;
	}

	// Configure the second rollback region.
	int result1 = mpu_region_configure(
		start_region_id + 1,
		CONFIG_MAPPED_STORAGE_BASE +
			DT_REG_ADDR(DT_NODELABEL(rollback1)),
		DT_REG_SIZE(DT_NODELABEL(rollback1)),
		MPU_ATTR_ROLLBACK_PROTECT);

	if (result1 != 0) {
		LOG_ERR("Failed to configure rollback1 MPU region (ID %d): %d",
			start_region_id + 1, result1);
		// Note: If the first config succeeded but second failed, we
		// might want to disable the first here if strict atomicity is
		// required.
		return result1;
	}

	// Store the ID of the first newly configured rollback region.
	mpu_static_rollback_region_id = start_region_id;
	LOG_DBG("Rollback MPU regions configured starting at ID %d",
		mpu_static_rollback_region_id);

	return 0;
}

// Register this function to be called during early kernel initialization,
// after static MPU regions from Zephyr are typically installed.
SYS_INIT(mpu_add_static_rollback_regions, PRE_KERNEL_1, 50);

/**
 * @brief Locks or unlocks the rollback MPU regions.
 *
 * Enables or disables the previously configured rollback MPU regions.
 *
 * @param lock If non-zero, the regions are locked (enabled). If zero,
 * the regions are unlocked (disabled).
 * @return 0 on success, -EINVAL if rollback regions were not initialized.
 */
int mpu_lock_rollback(int lock)
{
	if (mpu_static_rollback_region_id < 0) {
		LOG_ERR("Rollback MPU regions not initialized.");
		return -EINVAL;
	}

	// Iterate over the two rollback regions.
	for (int region_offset = 0; region_offset < 2; region_offset++) {
		int region_id = mpu_static_rollback_region_id + region_offset;
		if (lock) {
			mpu_region_enable(region_id);
			LOG_DBG("Enabled rollback region ID: %d", region_id);
		} else {
			mpu_region_disable(region_id);
			LOG_DBG("Disabled rollback region ID: %d", region_id);
		}
	}

	return 0;
}
#endif