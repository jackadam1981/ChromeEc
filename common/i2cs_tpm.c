/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2cs.h"
#include "registers.h"
#include "tpm_registers.h"

/*
 * This implements adaptaition layer between i2cs (i2c slave) port and TPM.
 *
 * The adaptation layer is stateless, it processes the i2cs "write complete"
 * interrupts on the interrupt context.
 *
 * Each "write complete" interrupt is associated with some data receved from
 * the master. If the package received from the master contains just one byte
 * payload, the value of this byte is considered the address of the TPM2
 * register to reach, read or write.
 *
 * Real TPM register addresses can be two bytes in size (even within locality
 * zero), to keep the i2c protocol simple and efficient, the real TPM register
 * addresses are re-mapped into i2c specific TPM register addresses.
 *
 * If the payload includes bytes following the address byte - those are the
 * data to be written to the addressed register. The number of bytes of data
 * could be anything between 1 and 63. Most tpm registers accept 4 bytes or
 * less, only the FIFO register (0x24) accepts variable amounts of data.
 *
 * The master knows how many bytes to write into FIFO or to read from it by
 * consulting the "burst size" field of the TPM status register. This happens
 * transparently for this layer.
 *
 * Data destined to and coming from the FIFO register is treated as a byte
 * stream.
 *
 * Data for and from all other registers is considered variable size (1 to 4
 * bytes) in network byte order. This layer converts between the network and
 * machine byte orders in both directions.
 *
 * Master write accesses followed by data result in the regiser address
 * mapped, data converted, if necessary, and passed to the tpm register task.
 *
 * Master write accesses requesting register reads result in the register
 * address mappend and accessing the tpm task to retrieve the proper register
 * data, converting it, if necessary, and passing it to the 12cs controller to
 * make available for master read accesses.
 *
 * Again, both read and write accesses complete on the same interrupt context
 * they were invoked on.
 */

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

struct i2c_tpm_reg_map {
	uint8_t   i2c_address;
	uint8_t   reg_size;
	uint16_t  tpm_address;
};
static const struct i2c_tpm_reg_map i2c_to_tpm[] = {
	{0, 1, 0},	/* TPM Access */
	{1, 4, 0x18},	/* TPM Status */
	{5, 0, 0x24},	/* TPM Fifo, variable size. */
	{6, 4, 0xf00},  /* TPM DID VID */
};

static void wr_complete_handler(void *i2cs_data, size_t i2cs_data_size)
{
	size_t i;
	uint16_t tpm_reg;
	uint8_t *data = i2cs_data;
	uint8_t reg_value[4];
	const struct i2c_tpm_reg_map *i2c_reg_entry = NULL;
	uint16_t reg_size;

	if (i2cs_data_size < 1) {
		/*
		 * This is a misformatted request, should never happen, just
		 * ignore it.
		 */
		CPRINTF("%s: empty receive payload, i2cs_data_size = %d\n",
			__func__, i2cs_data_size);
		return;
	}

	/* Let's find real TPM register address. */
	for (i = 0; i < ARRAY_SIZE(i2c_to_tpm); i++)
		if (i2c_to_tpm[i].i2c_address == *data) {
			i2c_reg_entry = i2c_to_tpm + i;
			break;
		}

	if (!i2c_reg_entry) {
		CPRINTF("%s: unsupported i2c tpm address 0x%x\n",
			__func__, *data);
		return;
	}

	/*
	 * OK, we know the tpm register address. Note that only full register
	 * accesses are supported for multybyte registers,
	 * TODO (scollyer crosbug.com/p/56539): Look at modifying this so we
	 * can handle 1 - 4 byte accesses at any any I2C register address we
	 * support.
	 */
	tpm_reg = i2c_reg_entry->tpm_address;
	reg_size = i2c_reg_entry->reg_size;

	i2cs_data_size--;
	data++;

	if (!i2cs_data_size) {
		/*
		 * The master wants to read the register, read the value and
		 * pass it to the controller.
		 */
		if (reg_size == 1) {
			uint8_t byte_reg;

			/* Always read 4 bytes. */
			tpm_register_get(tpm_reg, &byte_reg, sizeof(byte_reg));
			i2cs_post_read_data(byte_reg);
			return;
		}

		if (reg_size == 4) {
			tpm_register_get(tpm_reg, reg_value, sizeof(reg_value));

			/* Convert it to network byte order. */
			for (i = 0; i < sizeof(reg_value); i++)
				i2cs_post_read_data(reg_value[sizeof(reg_value)
							      - 1 - i]);
			return;
		}

		/*
		 * FIFO accesses do not require endianness conversion, but to
		 * find out how many bytes to read we need to consult the
		 * burst size field of the tpm status register.
		 */
		reg_size = tpm_get_burst_size();
		/*
		 * Now, this is a hack, but we are short on SRAM, so let's
		 * reuse the receive buffer for the FIFO data sotrage. We know
		 * that the ISR has a 64 byte buffer were it moves received
		 * data, and that buffer is no idling.
		 */
		data -= 1; /* Restore to original point. */
		tpm_register_get(tpm_reg, data, reg_size);
		for (i = 0; i < reg_size; i++)
			i2cs_post_read_data(data[i]);
		return;
	}

	/* This is an actual write request. */
	if (reg_size == 0) { /* Fifo write, send the stream down directly. */
		tpm_register_put(tpm_reg, data, i2cs_data_size);
		return;
	}

	if (i2cs_data_size != reg_size) {
		CPRINTF("%s: data size mismatch for reg 0x%x "
			"(rx %d, need %d)\n", __func__, tpm_reg,
			i2cs_data_size, reg_size);
		return;
	}

	if (reg_size == 1) {
		tpm_register_put(tpm_reg, data, 1);
		return;
	}

	/* 4 byte access, convert from network byte order. */
	for (i = 0; i < 4; i++)
		reg_value[i] = data[3 - i];

	tpm_register_put(tpm_reg, reg_value, 4);
}

static void i2cs_tpm_init(void)
{
	i2cs_register_write_complete_handler(wr_complete_handler);
}
DECLARE_HOOK(HOOK_INIT, i2cs_tpm_init, HOOK_PRIO_LAST);
