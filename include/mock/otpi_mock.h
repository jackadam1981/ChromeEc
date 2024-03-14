/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Controls for the mock OTP key library
 */

#ifndef __MOCK_OTPI_MOCK_H
#define __MOCK_OTPI_MOCK_H

#include "otp_key.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum API_RETURN_STATUS_T {
	/* OTP API succeeded */
	API_RET_OTP_STATUS_OK = 0xA5A5,
	/* OTP API failed */
	API_RET_OTP_STATUS_FAIL = 0x5A5A
};

enum ec_error_list otp_key_write(const uint8_t *key_buffer);

struct mock_otp {
	bool powered_on;
	uint8_t otp_key_buffer[OTP_KEY_SIZE_BYTES];
};

#define MOCK_OTP_DEFAULT \
	((struct mock_otp){ .powered_on = false, .otp_key_buffer = { 0 } })

extern struct mock_otp mock_otp;

extern enum API_RETURN_STATUS_T otpi_power(bool on);
extern enum API_RETURN_STATUS_T otpi_read(uint32_t address, uint8_t *data);
extern enum API_RETURN_STATUS_T otpi_write(uint32_t address, uint8_t data);
extern enum API_RETURN_STATUS_T otpi_write_protect(uint32_t address,
						   uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __MOCK_OTP_KEY_MOCK_H */
