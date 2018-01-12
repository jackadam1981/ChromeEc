/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cros Board Info
 */

#include "common.h"
#include "console.h"
#include "cbi.h"
#include "crc8.h"
#include "host_command.h"
#include "i2c.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "CBI " format, ## args)

static struct board_info bi;
static int initialized;

/*
 * Get board information from EEPROM
 */
static int init_board_info(void)
{
	uint8_t buf[256];
	uint8_t offset;

	if (initialized)
		return EC_SUCCESS;

	/* Read header */
	offset = 0;
	if (i2c_xfer(I2C_PORT_EEPROM, I2C_ADDR_EEPROM,
		     &offset, 1, buf, sizeof(bi.head), I2C_XFER_SINGLE)) {
		CPRINTS("Failed to read header");
		return EC_ERROR_INVAL;
	}
	memcpy(&bi.head, buf, sizeof(bi.head));

	/* Check magic */
	if (memcmp(bi.head.magic, cbi_magic, sizeof(bi.head.magic))) {
		CPRINTS("Invalid magic");
		return EC_ERROR_INVAL;
	}

	/* check version */
	if (bi.head.major > CBI_VERSION_MAJOR) {
		CPRINTS("Major version too high");
		return EC_ERROR_INVAL;
	}

	/* Check the data size. It's expected to support up to 64k but our
	 * buffer has practical limitation. */
	if (bi.head.total_size > sizeof(buf)) {
		CPRINTS("Data too large");
		return EC_ERROR_OVERFLOW;
	}

	/* Read the rest */
	offset = sizeof(bi.head);
	if (i2c_xfer(NPCX_I2C_PORT0_1, I2C_ADDR_EEPROM, &offset, 1,
		     buf + sizeof(bi.head),
		     bi.head.total_size - sizeof(bi.head),
		     I2C_XFER_SINGLE)) {
		CPRINTS("Failed to read body");
		return EC_ERROR_INVAL;
	}

	/* Check CRC */
	if (crc8(buf + 4, bi.head.total_size - 4) != bi.head.crc) {
		CPRINTS("Bad CRC");
		return EC_ERROR_INVAL;
	}

	/* Every thing looks good. Save only the data we understand. */
	memcpy(&bi.major, &buf[sizeof(bi.head)], sizeof(bi) - sizeof(bi.head));
	initialized = 1;

	return EC_SUCCESS;
}

int cbi_get_board_version(void)
{
	if (init_board_info())
		return EC_ERROR_UNKNOWN;
	return bi.version;
}

/*
 * It can be named as system_get_sku_id() to plumb it to the existing host
 * command.
 */
int cbi_get_sku_id(void)
{
	if (init_board_info())
		return EC_ERROR_UNKNOWN;
	return bi.sku_id;
}

int cbi_get_oem_id(void)
{
	if (init_board_info())
		return EC_ERROR_UNKNOWN;
	return bi.oem_id;
}

static int hc_get_cbi(struct host_cmd_handler_args *args)
{
	if (init_board_info())
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CBI,
		     hc_get_cbi,
		     EC_VER_MASK(0));
