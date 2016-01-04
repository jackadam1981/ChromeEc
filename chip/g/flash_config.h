/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_CHIP_G_FLASH_CONFIG_H
#define __EC_CHIP_G_FLASH_CONFIG_H

struct cr50_flash_region {
	uint32_t reg_base;
	uint32_t reg_size;
	uint32_t reg_perms;
};

#define FLASH_REGION_EN_ALL ((1 << GC_GLOBALSEC_FLASH_REGION0_CTRL_EN_LSB) |\
			     (1 << GC_GLOBALSEC_FLASH_REGION0_CTRL_RD_EN_LSB) |\
			     (1 << GC_GLOBALSEC_FLASH_REGION0_CTRL_WR_EN_LSB))

int flash_regions_to_enable(struct cr50_flash_region *regions,
			    int max_regions);

#endif  /* ! __EC_CHIP_G_FLASH_CONFIG_H */

