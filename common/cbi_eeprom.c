/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Support Cros Board Info EEPROM */

#include "cbi_eeprom.h"
#include "common.h"
#include "console.h"
#include "crc8.h"
#include "cros_board_info.h"
#include "gpio.h"
#include "host_command.h"
#include "i2c.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "CBI " format, ## args)

/*
 * We allow EEPROMs with page size of 8 or 16. Use 8 to be the most compatible.
 * This causes a little more overhead for writes, but we are not writing to the
 * EEPROM outside of the factory process.
 */
#define EEPROM_PAGE_WRITE_SIZE	8
#define EEPROM_PAGE_WRITE_MS	5
#define EC_ERROR_CBI_CACHE_INVALID	EC_ERROR_INTERNAL_FIRST

static int do_eeprom_read(uint8_t offset, uint8_t *in, int in_size)
{
	return i2c_read_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
			      offset, in, in_size);
}

/*
 * Get board information from EEPROM
 */
static int eeprom_read(uint8_t *cbi)
{
	struct cbi_header * const head = (struct cbi_header *)cbi;

	CPRINTS("Reading board info");

	/* Read header */
	if (do_eeprom_read(0, cbi, sizeof(head))) {
		CPRINTS("Failed to read header");
		return EC_ERROR_INVAL;
	}

	/* Check magic */
	if (memcmp(head->magic, cbi_magic, sizeof(head->magic))) {
		CPRINTS("Bad magic");
		return EC_ERROR_INVAL;
	}

	/* check version */
	if (head->major_version > CBI_VERSION_MAJOR) {
		CPRINTS("Version mismatch");
		return EC_ERROR_INVAL;
	}

	/*
	 * Check the data size. It's expected to support up to 64k but our
	 * buffer has practical limitation.
	 */
	if (head->total_size < sizeof(*head) ||
			head->total_size > CBI_IMAGE_SIZE) {
		CPRINTS("Bad size: %d", head->total_size);
		return EC_ERROR_OVERFLOW;
	}

	/* Read the data */
	if (do_eeprom_read(sizeof(*head), head->data,
			head->total_size - sizeof(*head))) {
		CPRINTS("Failed to read body");
		return EC_ERROR_INVAL;
	}

	/* Check CRC. This supports new fields unknown to this parser. */
	if (cbi_crc8(head) != head->crc) {
		CPRINTS("Bad CRC");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int eeprom_is_write_protected(void)
{
#ifdef CONFIG_BYPASS_CBI_EEPROM_WP_CHECK
	return 0;
#elif defined(CONFIG_WP_ACTIVE_HIGH)
	return gpio_get_level(GPIO_WP);
#else
	return !gpio_get_level(GPIO_WP_L);
#endif /* CONFIG_BYPASS_CBI_EEPROM_WP_CHECK */
}

static int eeprom_write(uint8_t *cbi)
{
	uint8_t *p = cbi;
	int rest = ((struct cbi_header *)p)->total_size;

	if (eeprom_is_write_protected()) {
		CPRINTS("Failed to write for WP");
		return EC_ERROR_ACCESS_DENIED;
	}

	while (rest > 0) {
		int size = MIN(EEPROM_PAGE_WRITE_SIZE, rest);
		int rv;

		rv = i2c_write_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
				     p - cbi, p, size);
		if (rv) {
			CPRINTS("Failed to write for %d", rv);
			return rv;
		}
		/* Wait for internal write cycle completion */
		msleep(EEPROM_PAGE_WRITE_MS);
		p += size;
		rest -= size;
	}

	return EC_SUCCESS;
}

const struct cbi_storage_driver eeprom_drv = {
	.store = eeprom_write,
	.load = eeprom_read,
	.is_protected = eeprom_is_write_protected,
};

const struct cbi_storage_config_t cbi_storage_config = {
	.storage_type = CBI_EEPROM,
	.drv = &eeprom_drv,
};
