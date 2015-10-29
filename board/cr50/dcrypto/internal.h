/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Crypto wrapper library for CR50.
 */
#ifndef __CROS_EC_DCRYPTO_INTERNAL_H
#define __CROS_EC_DCRYPTO_INTERNAL_H

#include "registers.h"
#include <inttypes.h>

#define BASE_ADDR           GC_KEYMGR_BASE_ADDR
#define CTRL_CTR_BIG_ENDIAN    0
#define CTRL_ENABLE   1
#define CTRL_ENCRYPT  1
#define CTRL_NO_SOFT_RESET  0


inline uint32_t read_volatile_reg(uint32_t addr)
{
	return(*(volatile uint32_t *) (addr));
}

inline void write_volatile_reg(uint32_t addr, uint32_t value)
{
	*(volatile uint32_t *) (addr) = value;
}

inline int wait_read_data(uint32_t offset)
{
	int rfifo_empty = 1;
	int timeout = 0;

	while (rfifo_empty && timeout++ < 20) {
		rfifo_empty = read_volatile_reg(BASE_ADDR + offset);
	}
	return rfifo_empty ? 0 : 1;
}

inline void set_control_register(
	unsigned enable, unsigned mode, unsigned keysize, unsigned encrypt,
	unsigned ctr_endian, unsigned soft_reset)
{
	write_volatile_reg(
		BASE_ADDR + GC_KEYMGR_AES_CTRL_OFFSET,
		((soft_reset << GC_KEYMGR_AES_CTRL_RESET_LSB)
			& GC_KEYMGR_AES_CTRL_RESET_MASK) |
		((keysize    << GC_KEYMGR_AES_CTRL_KEYSIZE_LSB)
			& GC_KEYMGR_AES_CTRL_KEYSIZE_MASK) |
		((mode       << GC_KEYMGR_AES_CTRL_CIPHER_MODE_LSB)
			& GC_KEYMGR_AES_CTRL_CIPHER_MODE_MASK) |
		((encrypt    << GC_KEYMGR_AES_CTRL_ENC_MODE_LSB)
			& GC_KEYMGR_AES_CTRL_ENC_MODE_MASK) |
		((ctr_endian << GC_KEYMGR_AES_CTRL_CTR_ENDIAN_LSB)
			& GC_KEYMGR_AES_CTRL_CTR_ENDIAN_MASK) |
		((enable     << GC_KEYMGR_AES_CTRL_ENABLE_LSB)
			& GC_KEYMGR_AES_CTRL_ENABLE_MASK));
}

#endif /* ! __CROS_EC_DCRYPTO_H */
