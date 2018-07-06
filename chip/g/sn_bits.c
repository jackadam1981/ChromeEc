/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board_id.h"
#include "console.h"
#include "flash_info.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

int read_sn_bits(struct sn_bits *sn)
{
	uint32_t *id_p;
	int i;

	/*
	 * SN Bits structure size is guaranteed to be divisible by 4, and it
	 * is guaranteed to be aligned at 4 bytes.
	 */

	id_p = (uint32_t *)sn;

	/* Make sure INFO1 board ID space is readable */
	if (flash_info_read_enable(INFO_BOARD_SPACE_SN_OFFSET,
				   INFO_BOARD_SPACE_SN_PROTECT_SIZE) !=
	    EC_SUCCESS) {
		CPRINTS("%s: failed to enable read access to info", __func__);
		return EC_ERROR_ACCESS_DENIED;
	}

	for (i = 0; i < sizeof(*sn); i += sizeof(uint32_t)) {
		int rv;

		rv = flash_physical_info_read_word
			(INFO_BOARD_SPACE_SN_OFFSET + i,
			 id_p);
		if (rv != EC_SUCCESS) {
			CPRINTF("%s: failed to read word %d, error %d\n",
				__func__, i, rv);
			return rv;
		}
		id_p++;
	}
	return EC_SUCCESS;
}

/**
 * Write serial number bits into the flash INFO1 space.
 *
 * @param id	Pointer to a SN Bits structure to copy into INFO1
 *
 * @return EC_SUCCESS or an error code in cases of various failures to read or
 *		if the space has been already initialized.
 */
static int write_sn_bits(const struct sn_bits *sn)
{
	int rv = EC_ERROR_PARAM_COUNT;

	/* Enable write access */
	if (flash_info_write_enable(INFO_BOARD_SPACE_SN_OFFSET,
				    INFO_BOARD_SPACE_SN_PROTECT_SIZE) !=
	    EC_SUCCESS) {
		CPRINTS("%s: failed to enable write access", __func__);
		return EC_ERROR_ACCESS_DENIED;
	}

	/* Write Board ID */
	rv = flash_info_physical_write(INFO_BOARD_SPACE_SN_OFFSET,
				       sizeof(*sn), (const char *)sn);
	if (rv != EC_SUCCESS)
		CPRINTS("%s: write failed", __func__);

	/* Disable write access */
	flash_info_write_disable();

	return rv;
}


static int command_sn_bits(int argc, char **argv)
{
	int rv = EC_ERROR_PARAM_COUNT;

	struct sn_bits sn;

	// Make buildall happy.
	if (0)
		write_sn_bits(&sn);

	switch (argc) {
#ifdef CR50_DEV
	case 3:
	{
		char *e;

		sn.sn[0] = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;

		sn.sn[1] = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;

		rv = write_sn_bits(&sn);
		if (rv == EC_SUCCESS)
			CPRINTF("Serial Number set successfully.\n");
	}
	/* fall through */
#endif
	case 1:
		rv = read_sn_bits(&sn);
		if (rv == EC_SUCCESS)
			CPRINTF("Serial Number: %08x %08x\n",
				sn.sn[0], sn.sn[1]);

		break;
	default:
		CPRINTF("Please specify no arguments to get SN, "
			"or 1 argument to set\n");
	}

	return rv;
}
DECLARE_SAFE_CONSOLE_COMMAND(snbits,
			     command_sn_bits, 0, "Get/Set Serial Number");
