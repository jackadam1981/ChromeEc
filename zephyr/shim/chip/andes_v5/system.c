/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/syscon.h>

#include "system.h"

uintptr_t system_get_fw_reset_vector(uintptr_t base)
{
	return base;
}

static const struct device *const syscon_dev =
DEVICE_DT_GET(DT_NODELABEL(syscon));

#define AOSMU_BOOT_STRAPPING 0x8 /* Secure key handling */
#define AOSMU_SECURE_CON 0xc /* Secure key handling */
#define AOSMU_SECURE_CON_BYPASS_BOOTSTRAP BIT(17) /* issue reset to MCU core */
#define AOSMU_SECURE_CON_DFU BIT(16) /* issue reset to MCU core */
#define AOSMU_SECURE_CON_WARM_RESET BIT(2) /* issue reset to whole SoC except for reset_vector(0x10) and dummy(0x1c) */
#define AOSMU_RESET_VECTOR 0x10

// TODO consider when we should accept such command. With only singed token? Always, because we use SDCP anyway?
void chip_enter_bootloader(void)
{
	uint32_t syscon;

	syscon_read_reg(syscon_dev, AOSMU_SECURE_CON, &syscon);
	syscon_write_reg(syscon_dev, AOSMU_RESET_VECTOR, 0x70000000);
	syscon_write_reg(syscon_dev, AOSMU_SECURE_CON, syscon | AOSMU_SECURE_CON_DFU);
	syscon_write_reg(syscon_dev, AOSMU_SECURE_CON, syscon | AOSMU_SECURE_CON_WARM_RESET | AOSMU_SECURE_CON_DFU);
}
