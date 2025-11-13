/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* One-Time Programmable (OTP) Key */

#include "common.h"
#include "console.h"
#include "openssl/mem.h"
#include "otp_key.h"
#include "panic.h"
#include "printf.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "util.h"

#include <zephyr/hal_egis/et171/inc/et171_hal/et171_hal_otp.h>

BUILD_ASSERT(OTP_KEY_SIZE_BYTES % sizeof(uint32_t) == 0);

#define ET171_OTP_BASE_ADDR 0x64
#define ET171_OTP_SECRET_KEY_INDEX 2

static const uint32_t otp_key_len = OTP_KEY_SIZE_BYTES / sizeof(uint32_t);
static const uint32_t otp_key_addr =
	ET171_OTP_BASE_ADDR + ET171_OTP_SECRET_KEY_INDEX * otp_key_len;

void otp_key_init(void)
{
}

void otp_key_exit(void)
{
}

enum ec_error_list otp_key_read(uint8_t *key_buffer)
{
	if (key_buffer == NULL) {
		return EC_ERROR_INVAL;
	}

	if (HAL_OTP_Read32(otp_key_addr, otp_key_len, (uint32_t *)key_buffer) !=
	    HAL_OK) {
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

enum ec_error_list otp_key_provision(void)
{
	enum ec_error_list ec_status = EC_ERROR_UNKNOWN;
	uint8_t otp_key_buffer[OTP_KEY_SIZE_BYTES] = { 0 };

	ec_status = otp_key_read(otp_key_buffer);
	if (ec_status != EC_SUCCESS) {
		ccprints("Failed to read OTP key with status=%d", ec_status);
		return ec_status;
	}

	/*
	 * If the stored bytes are trivial (all 0's or all 1's), panic.
	 */
	if (bytes_are_trivial(otp_key_buffer, OTP_KEY_SIZE_BYTES)) {
		uint8_t first_byte = otp_key_buffer[0];
		ccprintf("ERROR! %s OTP key is trivial (all 0x%02x)!\n",
			 __func__, first_byte);
		k_oops();
	}

	OPENSSL_cleanse(otp_key_buffer, OTP_KEY_SIZE_BYTES);

	return EC_SUCCESS;
}
