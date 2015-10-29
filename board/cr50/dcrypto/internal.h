/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Crypto wrapper library for CR50.
 */
#ifndef EC_BOARD_CR50_DCRYPTO_INTERNAL_H_
#define EC_BOARD_CR50_DCRYPTO_INTERNAL_H_

#include "registers.h"
#include <inttypes.h>

#define CTRL_CTR_BIG_ENDIAN    0
#define CTRL_ENABLE   1
#define CTRL_ENCRYPT  1
#define CTRL_NO_SOFT_RESET  0


inline int wait_read_data(volatile uint32_t *addr)
{
	int rfifo_empty = 1;
	int timeout = 0;

	while (rfifo_empty && timeout++ < 20) {
		rfifo_empty = REG32(addr);
	}
	return rfifo_empty ? 0 : 1;
}

#endif /* ! EC_BOARD_CR50_DCRYPTO_INTERNAL_H_ */
