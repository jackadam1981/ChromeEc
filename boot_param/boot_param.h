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
#define BOOT_PARAM_VERSION 1

/* Size of BootParam structure in bytes */
#if BOOT_PARAM_VERSION == 0
#define BOOT_PARAM_SIZE_STAGE1 820
#elif BOOT_PARAM_VERSION == 1
#define BOOT_PARAM_SIZE_STAGE1 1045
#else /* BOOT_PARAM_VERSION */
#error "Unsupported BOOT_PARAM_VERSION"
#endif /* BOOT_PARAM_VERSION */

#define BOOT_PARAM_SIZE \
	(BOOT_PARAM_SIZE_STAGE1 + BOOT_PARAM_CFG_DESCR_EXTRA_STAGE_SIZE)

/*
 * 8-bit ID of the DICE chain:
 * 0 - GSC
 * 1 - AP
 * 2 - HWDRM (not used)
 */
#define BOOT_PARAM_DICE_CHAIN_GSC   0
#define BOOT_PARAM_DICE_CHAIN_AP    1
#define BOOT_PARAM_DICE_CHAIN_HWDRM 2

/* Get (part of) BootParam structure for the specific chain:
 * [offset .. offset + size).
 */
size_t get_boot_param_bytes_for_chain(
	/* [OUT] destination buffer to fill */
	uint8_t *dest,
	/* [IN] starting offset in the BootParam struct */
	size_t offset,
	/* [IN] size of the BootParam struct to copy */
	size_t size,
	/* [IN] chain ID */
	uint8_t chain_id);

/* Get (part of) BootParam structure for the main chain:
 * [offset .. offset + size).
 */
#define get_boot_param_bytes(d, o, s) \
	get_boot_param_bytes_for_chain(d, o, s, BOOT_PARAM_DICE_CHAIN_AP)

/* Size of DiceChain structure in bytes */
#define DICE_CHAIN_SIZE_STAGE1 605
#define DICE_CHAIN_SIZE \
	(DICE_CHAIN_SIZE_STAGE1 + BOOT_PARAM_CFG_DESCR_EXTRA_STAGE_SIZE)

/* Get (part of) DiceChain structure for the specific chain:
 * [offset .. offset + size).
 */
size_t get_dice_chain_bytes_for_chain(
	/* [OUT] destination buffer to fill */
	uint8_t *dest,
	/* [IN] starting offset in the DiceChain struct */
	size_t offset,
	/* [IN] size of the data to copy */
	size_t size,
	/* [IN] chain ID */
	uint8_t chain_id);

/* Get (part of) DiceChain structure for the main chain:
 * [offset .. offset + size).
 */
#define get_dice_chain_bytes(d, o, s) \
	get_dice_chain_bytes_for_chain(d, o, s, BOOT_PARAM_DICE_CHAIN_AP)

/**
 * @brief Get the boot mode
 *
 * @return 0 for error (BOOT_MODE_ERROR), or
 * BOOT_MODE_NORMAL / BOOT_MODE_RECOVERY / BOOT_MODE_DEBUG
 */
uint8_t get_boot_mode(void);

/* Sign data with attestation CDI key for the specific chain.
 */
bool sign_with_cdi_key(
	/* [IN] chain ID */
	uint8_t chain_id,
	/* [IN] data to sign */
	const struct slice_ref_s data_to_sign,
	/* [OUT] resulting signature */
	uint8_t signature[ECDSA_SIG_BYTES]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __GSC_UTILS_BOOT_PARAM_BOOT_PARAM_H */
