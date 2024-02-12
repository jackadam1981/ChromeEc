/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* One-Time Programmable (OTP) Key */
#include "common.h"
#include "console.h"
#include "otp_key.h"
#include "panic.h"
#include "printf.h"
#include "registers.h"
#include "rom_chip.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "util.h"

#define OTP_KEY_ADDR 0x300

void otp_key_init(void)
{
	enum API_RETURN_STATUS_T status = API_RET_OTP_STATUS_FAIL;

	status = otpi_power(true);
	if (status != API_RET_OTP_STATUS_OK) {
		ccprintf("ERROR! %s failed %x\n", __func__, status);
		software_panic(PANIC_SW_ASSERT, task_get_current());
	}
}

void otp_key_exit(void)
{
	enum API_RETURN_STATUS_T status = API_RET_OTP_STATUS_FAIL;

	status = otpi_power(false);
	if (status != API_RET_OTP_STATUS_OK)
		ccprintf("ERROR! %s failed %x\n", __func__, status);
}

uint32_t otp_key_read(uint8_t *key_buffer)
{
	enum API_RETURN_STATUS_T status = API_RET_OTP_STATUS_FAIL;
	uint8_t i;

	for (i = 0; i < FP_POSITIVE_MATCH_SECRET_BYTES; i++) {
		status = otpi_read(OTP_KEY_ADDR + i, &key_buffer[i]);
		if (status != API_RET_OTP_STATUS_OK)
			return status;
	}

	return status;
}

uint32_t otp_key_write(uint8_t *key_buffer)
{
	enum API_RETURN_STATUS_T status = API_RET_OTP_STATUS_FAIL;
	uint8_t i;

	for (i = 0; i < FP_POSITIVE_MATCH_SECRET_BYTES; i++) {
		status = otpi_write(OTP_KEY_ADDR + i, key_buffer[i]);
		if (status != API_RET_OTP_STATUS_OK)
			return status;
	}

	return status;
}

uint32_t otp_key_provision(void)
{
	enum API_RETURN_STATUS_T status = API_RET_OTP_STATUS_FAIL;
	uint8_t otp_key_buffer[FP_POSITIVE_MATCH_SECRET_BYTES] = { 0 };

	status = otp_key_read(otp_key_buffer);
	if (status != API_RET_OTP_STATUS_OK)
		ccprints("Failed to read OTP key with status=%d", status);

	/* If the stored bytes are trivial (all 0's), generate and write key*/
	if (bytes_are_trivial(otp_key_buffer, FP_POSITIVE_MATCH_SECRET_BYTES)) {
		ccprints("stored OTP bytes trivial, generating key");
		trng_init();
		trng_rand_bytes(otp_key_buffer, FP_POSITIVE_MATCH_SECRET_BYTES);
		trng_exit();

		status = otp_key_write(otp_key_buffer);
		if (status != API_RET_OTP_STATUS_OK)
			ccprints("failed to write OTP key, status=%d", status);

		status = otpi_write_protect(OTP_KEY_ADDR,
					    FP_POSITIVE_MATCH_SECRET_BYTES);
		if (status != API_RET_OTP_STATUS_OK)
			ccprints("failed to write protect OTP key, status=%d",
				 status);
	}

	return status;
}
