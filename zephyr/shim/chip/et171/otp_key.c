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

void otp_key_init(void)
{
}

void otp_key_exit(void)
{
}

enum ec_error_list otp_key_read(uint8_t *key_buffer)
{
	if (key_buffer == NULL) {
		return API_RET_STATUS_INVALID_SIZE;
	}

	HAL_OTP_Read32(0x64 + 2 * 8, 8, (uint32_t *)key_buffer);

	return API_RET_STATUS_OK;
}

enum ec_error_list otp_key_provision(void)
{
	enum API_RETURN_STATUS_T otpi_status = API_RET_OTP_STATUS_FAIL;
	enum ec_error_list ec_status = EC_ERROR_UNKNOWN;
	uint8_t otp_key_buffer[OTP_KEY_SIZE_BYTES] = { 0 };

	ec_status = otp_key_read(otp_key_buffer);
	if (ec_status != API_RET_STATUS_OK) {
		ccprints("Failed to read OTP key with status=%d", ec_status);
		return ec_status;
	}

	/*
	 * If the stored bytes are trivial (all 0's or all 1's), panic.
	 */
	if (bytes_are_trivial(otp_key_buffer, OTP_KEY_SIZE_BYTES)) {
		ccprintf("ERROR! %s OTP key is trivial!\n", __func__);
		k_oops();
	}

	OPENSSL_cleanse(otp_key_buffer, OTP_KEY_SIZE_BYTES);

	return API_RET_STATUS_OK;
}
