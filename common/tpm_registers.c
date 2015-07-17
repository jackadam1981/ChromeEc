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

#define CPRINTS(format, args...) cprints(CC_TPM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_TPM, format, ## args)


/* Register addresses for FIFO mode. */
#define TPM_ACCESS_REG		0
#define TPM_INT_ENABLE_REG	8
#define TPM_INT_VECTOR	      0xC
#define TPM_INT_STATUS	     0x10
#define TPM_INTF_CAPABILITY  0x14
#define TPM_STS		     0x1C
#define TPM_DATA_FIFO	     0x28
#define TPM_INTERFACE_ID     0x30
#define TPM_DID_VID	    0xf00
#define TPM_RID		    0xf00

#define GOOGLE_VID 0x1ae0
#define GOOGLE_DID 0x0028

/* A preliminary interface capability register value, will be fine tuned. */
#define IF_CAPABILITY_REG ((3 << 28) | /* TPM2.0 (interface 1.3) */   \
			   (3 << 9) | /* up to 64 bytes transfers. */ \
			   0x15) /* Mandatory set to one. */
/*
 * NOTE: The put/get functions are called in interrupt context! Don't waste a
 * lot of time here - just copy the data and wake up a task to deal with it
 * later. Although if the implementation mandates a "busy" bit somewhere, you
 * might want to set it now to avoid race conditions with back-to-back
 * interrupts.
 */

static void copy_bytes(uint8_t *dest, uint32_t data_size, uint32_t value)
{
	unsigned real_size, i;

	if (data_size > 4)
		real_size = 4;
	else
		real_size = data_size;

	for (i = 0; i < real_size; i++)
		dest[i] = (value >> (i * 8)) & 0xff;
}

void tpm_register_put(uint32_t regaddr, uint8_t *data, uint32_t data_size)
{
	uint32_t i;

	CPRINTF("%s(0x%06x, %d", __func__, regaddr, data_size);
	for (i = 0; i < data_size; i++)
		CPRINTF(", %02x", data[i]);
	CPRINTF("\n");
}

void tpm_register_get(uint32_t regaddr, uint8_t *dest, uint32_t data_size)
{
	switch (regaddr) {
	case TPM_DID_VID:
		copy_bytes(dest, data_size, (GOOGLE_DID << 16) | GOOGLE_VID);
		return;
	case TPM_INTF_CAPABILITY:
		copy_bytes(dest, data_size, IF_CAPABILITY_REG);
		return;
	default:
		break;
	}
	CPRINTS("%s(0x%06x, %d) => ??", __func__, regaddr, data_size);
}
