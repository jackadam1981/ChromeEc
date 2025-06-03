/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "rom_chip.h"

static const volatile uint32_t *ADDR_DOWNLOAD_FROM_FLASH = (uint32_t *)0x40;

typedef void (*download_from_flash_ptr)(uint32_t src_offset, uint32_t dest_addr,
					uint32_t size,
					enum API_SIGN_OPTIONS_T sign,
					uint32_t exe_addr,
					enum API_RETURN_STATUS_T *ec_status);

void download_from_flash(uint32_t src_offset, uint32_t dest_addr, uint32_t size,
			 enum API_SIGN_OPTIONS_T sign, uint32_t exe_addr,
			 enum API_RETURN_STATUS_T *status)
{
	((download_from_flash_ptr)*ADDR_DOWNLOAD_FROM_FLASH)(
		src_offset, dest_addr, size, sign, exe_addr, status);
}
