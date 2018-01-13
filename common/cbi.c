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

static uint8_t cbi_crc8(const struct board_info *bi)
{
	return crc8((uint8_t *)&bi->head.crc + 1, bi->head.total_size - 4);
}

/*
 * Get board information from EEPROM
 */
static int read_board_info(void)
{
	uint8_t buf[256];
	uint8_t offset;

	if (initialized)
		return EC_SUCCESS;

	CPRINTS("Reading board info");

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
	if (i2c_xfer(I2C_PORT_EEPROM, I2C_ADDR_EEPROM, &offset, 1,
		     buf + sizeof(bi.head),
		     bi.head.total_size - sizeof(bi.head),
		     I2C_XFER_SINGLE)) {
		CPRINTS("Failed to read body");
		return EC_ERROR_INVAL;
	}

	/* Save only the data we understand. */
	memcpy(&bi.head + 1, &buf[sizeof(bi.head)],
	       sizeof(bi) - sizeof(bi.head));

	/* Check CRC */
	if (cbi_crc8(&bi) != bi.head.crc) {
		CPRINTS("Bad CRC");
		return EC_ERROR_INVAL;
	}

	initialized = 1;

	return EC_SUCCESS;
}

static int write_board_info(void)
{
	uint8_t buf[17];	/* Address byte + Page write size (16) */
	_Static_assert(sizeof(buf) > sizeof(struct board_info),
		       "Buffer is smaller than struct board_info");

	buf[0] = 0;	/* Offset 0 */
	memcpy(&buf[1], &bi, sizeof(bi));
	if (i2c_xfer(I2C_PORT_EEPROM, I2C_ADDR_EEPROM, buf,
		     sizeof(bi) + 1, NULL, 0, I2C_XFER_SINGLE)) {
		CPRINTS("Failed to write");
		return EC_ERROR_ACCESS_DENIED;
	}

	return EC_SUCCESS;
}

int cbi_get_board_version(void)
{
	if (read_board_info())
		return EC_ERROR_UNKNOWN;
	return bi.version;
}

/*
 * It can be named as system_get_sku_id() to plumb it to the existing host
 * command.
 */
int cbi_get_sku_id(void)
{
	if (read_board_info())
		return EC_ERROR_UNKNOWN;
	return bi.sku_id;
}

int cbi_get_oem_id(void)
{
	if (read_board_info())
		return EC_ERROR_UNKNOWN;
	return bi.oem_id;
}

static int hc_cbi_get(struct host_cmd_handler_args *args)
{
	const struct __ec_align4 ec_params_cbi_get *p = args->params;

	if (read_board_info())
		return EC_RES_ERROR;

	switch (p->type) {
	case CBI_DATA_BOARD_VERSION:
		*(uint32_t *)args->response = bi.version;
		break;
	case CBI_DATA_OEM_ID:
		*(uint32_t *)args->response = bi.oem_id;
		break;
	case CBI_DATA_SKU_ID:
		*(uint32_t *)args->response = bi.sku_id;
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}
	args->response_size = sizeof(uint32_t);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CBI_GET,
		     hc_cbi_get,
		     EC_VER_MASK(0));

static int hc_cbi_set(struct host_cmd_handler_args *args)
{
	const struct __ec_align4 ec_params_cbi_set *p = args->params;

	if (p->flag & CBI_SET_INIT) {
		memset(&bi, 0, sizeof(bi));
		memcpy(&bi.head.magic, cbi_magic, sizeof(cbi_magic));
		bi.head.major = CBI_VERSION_MAJOR;
		bi.head.minor = CBI_VERSION_MINOR;
		bi.head.total_size = sizeof(bi);
		initialized = 1;
	} else {
		if (read_board_info())
			return EC_RES_ERROR;
	}

	switch (p->type) {
	case CBI_DATA_BOARD_VERSION:
		if (p->data > UINT16_MAX)
			return EC_RES_INVALID_PARAM;
		bi.version = p->data;
		break;
	case CBI_DATA_OEM_ID:
		if (p->data > UINT8_MAX)
			return EC_RES_INVALID_PARAM;
		bi.oem_id = p->data;
		break;
	case CBI_DATA_SKU_ID:
		if (p->data > UINT8_MAX)
			return EC_RES_INVALID_PARAM;
		bi.sku_id = p->data;
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	bi.head.crc = cbi_crc8(&bi);

	/* Skip write if client asks so. */
	if (p->flag & CBI_SET_NO_SYNC)
		return EC_RES_SUCCESS;

	if (write_board_info())
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CBI_SET,
		     hc_cbi_set,
		     EC_VER_MASK(0));
