/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* One-Time Programmable (OTP) Key */

#include "console.h"
#include "openssl/mem.h"
#include "otp_key.h"
#include "util.h"

#include <et171_hal/et171_hal_otp.h>

BUILD_ASSERT(OTP_KEY_SIZE_BYTES % sizeof(uint32_t) == 0);

/* ET171 OTP memory map details from the Flash Memory Address Map */
#define ET171_OTP_BASE_ADDR 0x64

/* Index of the secret key (skey3) within the OTP key region */
#define ET171_OTP_SECRET_KEY_INDEX 2

static const uint32_t otp_key_len = OTP_KEY_SIZE_BYTES / sizeof(uint32_t);

/* Calculated address of the OTP key @index 3 in the ET171 OTP memory */
static const uint32_t otp_key_addr =
	ET171_OTP_BASE_ADDR + ET171_OTP_SECRET_KEY_INDEX * otp_key_len;

void otp_key_init(void)
{
	/* Nothing to do */
}

void otp_key_exit(void)
{
	/* Nothing to do */
}

enum ec_error_list otp_key_read(uint8_t *key_buffer)
{
	if (key_buffer == NULL) {
		return EC_ERROR_INVAL;
	}

	if (HAL_OTP_Read32(otp_key_addr, otp_key_len, (uint32_t *)key_buffer) !=
	    HAL_OK) {
		/* TODO: Map more specific HAL errors if available */
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
	 * If the stored bytes are trivial (all 0's or all 1's), this indicates
	 * a potential provisioning failure during factory setup. Panicking here
	 * to halt the fingerprint sensor boot up process.
	 */
	if (bytes_are_trivial(otp_key_buffer, OTP_KEY_SIZE_BYTES)) {
		uint8_t first_byte = otp_key_buffer[0];
		ccprintf("ERROR! %s OTP key is trivial (all 0x%02x)!\n",
			 __func__, first_byte);
		/* This function is expected to be called in a context
		 * where panicking on a bad OTP key is the desired behavior
		 * (e.g., initial device setup).
		 */
		k_oops();
	}

	OPENSSL_cleanse(otp_key_buffer, OTP_KEY_SIZE_BYTES);

	return EC_SUCCESS;
}
