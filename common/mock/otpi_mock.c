/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Mock OTPI library
 */
#include "mock/otpi_mock.h"
#include "openssl/mem.h"
#include "otp_key.h"
#include "rom_chip.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "util.h"

#ifndef TEST_BUILD
#error "Mocks should only be in the test build."
#endif

uint8_t otp_key_buffer[OTP_KEY_SIZE_BYTES] = { 0 };

/* true: OTP hardware on, false: OTP hardware off */
enum API_RETURN_STATUS_T otpi_power(bool on)
{
	return API_RET_OTP_STATUS_OK;
}

/*
 * address - OTP address to read from
 * data - pointer to 8-bit variable to store read data
 */
enum API_RETURN_STATUS_T otpi_read(uint32_t address, uint8_t *data)
{
	*data = otp_key_buffer[address - OTP_KEY_ADDR];
	return API_RET_OTP_STATUS_OK;
}

/*
 * address - OTP address to write to
 * data -  8-bit data value
 */
enum API_RETURN_STATUS_T otpi_write(uint32_t address, uint8_t data)
{
	otp_key_buffer[address - OTP_KEY_ADDR] = data;
	return API_RET_OTP_STATUS_OK;
}

/*
 * address - OTP address to protect, 16B aligned
 * size - Number of bytes to be protected, 16B aligned
 */
enum API_RETURN_STATUS_T otpi_write_protect(uint32_t address, uint32_t size)
{
	return API_RET_OTP_STATUS_OK;
}
