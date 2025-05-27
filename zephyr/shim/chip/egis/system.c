/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"

#include <zephyr/drivers/syscon.h>

/* TODO(b/432659361): use Egis HAL once it is merged to chromiumos sources. */
#define AOSMU_BOOT_STRAPPING 0x8
#define AOSMU_SECURE_CON 0xc
#define AOSMU_SECURE_CON_BYPASS_BOOTSTRAP BIT(17)
#define AOSMU_SECURE_CON_DFU BIT(16)
#define AOSMU_SECURE_CON_WARM_RESET BIT(2)
#define AOSMU_RESET_VECTOR 0x10

static const struct device *const syscon_dev =
	DEVICE_DT_GET(DT_NODELABEL(syscon));

uintptr_t system_get_fw_reset_vector(uintptr_t base)
{
	return base;
}

void chip_enter_bootloader(uint8_t mode)
{
	uint32_t syscon;

	syscon_read_reg(syscon_dev, AOSMU_SECURE_CON, &syscon);
	syscon_write_reg(syscon_dev, AOSMU_RESET_VECTOR, 0x70000000);
	syscon_write_reg(syscon_dev, AOSMU_SECURE_CON,
			 syscon | AOSMU_SECURE_CON_DFU);
	syscon_write_reg(syscon_dev, AOSMU_SECURE_CON,
			 syscon | AOSMU_SECURE_CON_WARM_RESET |
				 AOSMU_SECURE_CON_DFU);
}
