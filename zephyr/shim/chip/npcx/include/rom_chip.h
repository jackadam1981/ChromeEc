/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ROM_CHIP_H
#define __CROS_EC_ROM_CHIP_H

#include "common.h"
#include "util.h"

/* Enumerations of ROM api functions */
enum API_SIGN_OPTIONS_T {
	SIGN_NO_CHECK = 0,
	SIGN_CRC_CHECK = 1,
};

enum API_RETURN_STATUS_T {
	/* Successful download */
	API_RET_STATUS_OK = 0,
	/* Address is outside of flash or not 4 bytes aligned. */
	API_RET_STATUS_INVALID_SRC_ADDR = 1,
	/* Address is outside of RAM or not 4 bytes aligned. */
	API_RET_STATUS_INVALID_DST_ADDR = 2,
	/* Size is 0 or not 4 bytes aligned. */
	API_RET_STATUS_INVALID_SIZE = 3,
	/* Flash Address + Size is out of flash. */
	API_RET_STATUS_INVALID_SIZE_OUT_OF_FLASH = 4,
	/* RAM Address + Size is out of RAM. */
	API_RET_STATUS_INVALID_SIZE_OUT_OF_RAM = 5,
	/* Wrong sign option. */
	API_RET_STATUS_INVALID_SIGN = 6,
	/* Error during Code copy. */
	API_RET_STATUS_COPY_FAILED = 7,
	/* Execution Address is outside of RAM */
	API_RET_STATUS_INVALID_EXE_ADDR = 8,
	/* Bad CRC value */
	API_RET_STATUS_INVALID_SIGNATURE = 9,
};

/******************************************************************************/
/*
 * Declarations of ROM API functions
 */

/**
 * @param src_offset The offset of the data to be downloaded.
 * @param dest_addr The address of the downloaded data in the RAM.
 * @param size Number of bytes to download.
 * @param sign Need CRC check or not.
 * @param exe_addr jump to this address after download if not zero.
 * @param status Status of download.
 */
void download_from_flash(uint32_t src_offset, uint32_t dest_addr, uint32_t size,
			 enum API_SIGN_OPTIONS_T sign, uint32_t exe_addr,
			 enum API_RETURN_STATUS_T *status);

#endif /* __CROS_EC_ROM_CHIP_H */
