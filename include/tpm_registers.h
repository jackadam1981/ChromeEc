/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This defines the interface functions for TPM SPI Hardware Protocol. The SPI
 * master reads or writes between 1 and 64 bytes to a register designated by a
 * 24-bit address. There is no provision for error reporting at this level.
 */

#ifndef __CROS_EC_TPM_REGISTERS_H
#define __CROS_EC_TPM_REGISTERS_H

#include <stdint.h>
#include <stddef.h>

#include "common.h"

/* The SPI master is writing data into a TPM register. */
void tpm_register_put(uint32_t regaddr,
		      const uint8_t *data, uint32_t data_size);

/* The SPI master is reading data from a TPM register. */
void tpm_register_get(uint32_t regaddr, uint8_t *dest, uint32_t data_size);

/* Get the current value of the burst size field of the status register. */
size_t tpm_get_burst_size(void);

/* Register a function to restart TPM communications layer. */
typedef void (*interface_restart_func)(void);
void tpm_register_interface(interface_restart_func interface_restart);

/*
 * Reset the TPM. This sends a request to the TPM task, so that the reset can
 * happen when the TPM task finishes whatever it's doing at the moment.
 *
 * Returns 0 if the request was made, but we can't wait for it to complete
 * because we're in interrupt context or something similar. Otherwise, it
 * blocks and returns 1 after the TPM has been cleared, or returns -1 if the
 * request timed out.
 */
int tpm_reset(void);

/*
 * Return true if tpm is being reset. Usually this helps to avoid unnecessary
 * extra reset early at startup time, when TPM could be busy installing
 * endorsement certificates.
 */
int tpm_is_resetting(void);

/*
 * All TPM commands use this struct for input and output. Any other data
 * follows immediately after. All values are sent using network (big endian)
 * byte order.
 */
struct tpm_common_header {
	uint16_t tag;
	uint32_t size;
	uint32_t code;		     /* Command (in) or Response (out) */
} __packed;

/*
 * The only TPM2 command we care about on the driver level, see
 * crosbug.com/p/55667 for detals.
 */
#define TPM2_PCR_Read		0x0000017e

#endif	/* __CROS_EC_TPM_REGISTERS_H */
