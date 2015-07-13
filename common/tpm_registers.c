/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This implements the register interface for the TPM SPI Hardware Protocol.
 * The master puts or gets between 1 and 64 bytes to a register designated by a
 * 24-bit address. There is no provision for error reporting at this level.
 */

#include "common.h"
#include "console.h"
#include "tpm_registers.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

/*
 * NOTE: The put/get functions are called in interrupt context! Don't waste a
 * lot of time here - just copy the data and wake up a task to deal with it
 * later. Although if the implementation mandates a "busy" bit somewhere, you
 * might want to set it now to avoid race conditions with back-to-back
 * interrupts.
 */

void register_put(uint32_t regaddr, uint8_t *data, uint32_t data_size)
{
	uint32_t i;

	CPRINTF("%s(0x%06x, %d", __func__, regaddr, data_size);
	for (i = 0; i < data_size; i++)
		CPRINTF(", %02x", data[i]);
	CPRINTF("\n");
}

void register_get(uint32_t regaddr, uint8_t *dest, uint32_t data_size)
{
	CPRINTS("%s(0x%06x, %d) => ??", __func__, regaddr, data_size);
}
