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
#include "rom_chip.h"

#include <stdbool.h>

struct mock_otp {
	bool powered_on;
	uint8_t otp_key_buffer[OTP_KEY_SIZE_BYTES];
};

#define MOCK_OTP_DEFAULT \
	((struct mock_otp){ .powered_on = false, .otp_key_buffer = { 0 } })

extern struct mock_otp mock_otp;
#endif /* __MOCK_OTP_KEY_MOCK_H */
