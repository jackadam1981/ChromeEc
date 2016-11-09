/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_INCLUDE_TPM_VENDOR_H
#define __EC_INCLUDE_TPM_VENDOR_H

#include "common.h"

/*
 * TPMv2 Spec mandates that vendor-specific command codes have bit 29 set,
 * while bits 15-0 indicate the command. All other bits should be zero.
 */
#define TPM_CC_VENDOR_BIT_MASK 0x20000000
#define VENDOR_CC_MASK         0x0000ffff

/*
 * The TPM response code is all zero for success.
 * Errors are a little complicated:
 *
 *   Bits 31:12 must be zero.
 *
 *   Bit 11     S=0   Error
 *   Bit 10     T=1   Vendor defined response code
 *   Bit  9     r=0   reserved
 *   Bit  8     V=1   Conforms to TPMv2 spec
 *   Bit  7     F=0   Confirms to Table 14, Format-Zero Response Codes
 *   Bits 6:0   num   128 possible failure reasons
 */
#define VENDOR_RC_ERR 0x00000500


/* Our vendor-specific command codes. 16 bits available. */
enum vendor_cmd_cc {
	VENDOR_CC_HEY = 0,
	VENDOR_CC_WHAT = 1,
};

/* Our vendor-specific response codes. 7 bits available. */
enum vendor_cmd_rc {
	VENDOR_RC_SUCCESS = 0,
	VENDOR_RC_BOGUS_ARGS = 1,
	/* Only 7 bits available; max is 127 */
	VENDOR_RC_NO_SUCH_COMMAND = 127,
};

/*
 * Function type for vendor commands.
 *
 * @param code          Vendor-specific command code
 * @param buffer        Input parameters, to be replaced with response data
 * @param input_size    Number of bytes of input data
 * @param response_size On input == max size of the buffer,
 *                      On output == number of data bytes returned
 * @returns             Response code
 */
typedef enum vendor_cmd_rc (*vendor_cmd_handler)(enum vendor_cmd_cc code,
						 uint8_t *buffer,
						 uint32_t input_size,
						 uint32_t *response_size);

/*
 * Handle a vendor command.
 *
 * @param inout         Buffer of TPM input, to be replaced with TPM output
 * @param inout_size    On input == max size of the buffer,
 *                      On output == number of data bytes to return
 */
void call_vendor_cmd(void *inout, uint32_t *inout_size);


struct vendor_cmd_s {
	enum vendor_cmd_cc command_code;
	vendor_cmd_handler handler;
};

/* Declare a vendor command handler */
#define DECLARE_VENDOR_COMMAND(command_code, handler)			\
	const struct vendor_cmd_s __keep __vendor_cmd_##command_code	\
	__attribute__((section(".rodata.vendorcmds")))			\
		= {command_code, handler}

#endif  /* __EC_INCLUDE_TPM_VENDOR_H */
