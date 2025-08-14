/*
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __GSC_UTILS_BOOT_PARAM_BOOT_PARAM_H
#define __GSC_UTILS_BOOT_PARAM_BOOT_PARAM_H

#include "boot_param_types.h"
#include "boot_param_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BootParam structure version */
#define BOOT_PARAM_VERSION 0

/* Size of BootParam structure in bytes */
#if BOOT_PARAM_VERSION == 1
#define BOOT_PARAM_SIZE_STAGE1 1045
#else /* BOOT_PARAM_VERSION == 0 */
#define BOOT_PARAM_SIZE_STAGE1 820
#endif /* BOOT_PARAM_VERSION */

#define BOOT_PARAM_SIZE \
	(BOOT_PARAM_SIZE_STAGE1 + BOOT_PARAM_CFG_DESCR_EXTRA_STAGE_SIZE)

/* Get (part of) BootParam structure: [offset .. offset + size) */
size_t get_boot_param_bytes(
	/* [OUT] destination buffer to fill */
	uint8_t *dest,
	/* [IN] starting offset in the BootParam struct */
	size_t offset,
	/* [IN] size of the data to copy */
	size_t size
);

/* Size of DiceChain structure in bytes */
#define DICE_CHAIN_SIZE_STAGE1 605
#define DICE_CHAIN_SIZE \
	(DICE_CHAIN_SIZE_STAGE1 + BOOT_PARAM_CFG_DESCR_EXTRA_STAGE_SIZE)

/* Get (part of) DiceChain structure: [offset .. offset + size) */
size_t get_dice_chain_bytes(
	/* [OUT] destination buffer to fill */
	uint8_t *dest,
	/* [IN] starting offset in the DiceChain struct */
	size_t offset,
	/* [IN] size of the data to copy */
	size_t size
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __GSC_UTILS_BOOT_PARAM_BOOT_PARAM_H */
