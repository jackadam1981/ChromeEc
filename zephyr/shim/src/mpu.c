/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "mpu.h"

uint32_t mpu_get_type(void)
{
	return 0;
}

int mpu_protect_data_ram(void)
{
}

int mpu_protect_code_ram(void)
{
	/* Prevent write access to code RAM */
	return mpu_config_region(REGION_STORAGE,
				 CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RO_MEM_OFF,
				 CONFIG_CODE_RAM_SIZE,
				 MPU_ATTR_RO_NO | MPU_ATTR_INTERNAL_SRAM,
				 1);
}
