/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cros Board Info
 */

#include "common.h"
#include "console.h"
#include "cros_board_info.h"
#include "datablob.h"
#include "gpio.h"
#include "host_command.h"
#include "i2c.h"
#include "timer.h"

#ifdef HOST_TOOLS_BUILD
#include <string.h>
#else
#include "util.h"
#endif

/*
 * Functions and variables defined here shared with host tools (e.g. cbi-util).
 * TODO: Move these to common/cbi/cbi.c and common/cbi/utils.c if they grow.
 */

uint8_t cbi_crc8(const struct datablob_header *h)
{
	return datablob_crc8(h);
}

uint8_t *cbi_set_data(uint8_t *p, enum cbi_data_tag tag,
		      const void *buf, int size)
{
	return datablob_add_data(p, tag, buf, size);
}

uint8_t *cbi_set_string(uint8_t *p, enum cbi_data_tag tag, const char *str)
{
	if (str == NULL)
		return p;

	return datablob_add_data(p, tag, str, strlen(str) + 1);
}

struct datablob_item *cbi_find_tag(const void *record, enum cbi_data_tag tag)
{
	return datablob_find_tag(record, tag);
}

/*
 * Functions and variables specific to EC firmware
 */
#ifndef HOST_TOOLS_BUILD

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "CBI " format, ## args)

/*
 * We allow EEPROMs with page size of 8 or 16. Use 8 to be the most compatible.
 * This causes a little more overhead for writes, but we are not writing to the
 * EEPROM outside of the factory process.
 */
#define EEPROM_PAGE_WRITE_SIZE	8

#define EEPROM_PAGE_WRITE_MS	5

static int do_eeprom_read(uint8_t offset, uint8_t *in, int in_size)
{
	return i2c_read_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
			      offset, in, in_size);
}

/*
 * Get board information from EEPROM
 */
static int eeprom_read(uint8_t *record, int record_size)
{
	struct datablob_header * const h = (struct datablob_header *)record;
	CPRINTS("Reading board info");

	/* Read header */
	if (do_eeprom_read(0, record, sizeof(*h))) {
		CPRINTS("Failed to read header");
		return EC_ERROR_INVAL;
	}

	/* Check magic */
	if (memcmp(h->magic, datablob_magic, sizeof(h->magic))) {
		CPRINTS("Bad magic");
		return EC_ERROR_INVAL;
	}

	/* check version */
	if (h->major_version > DATABLOB_VERSION_MAJOR) {
		CPRINTS("Version mismatch");
		return EC_ERROR_INVAL;
	}

	/* Check the data size. It's expected to support up to 64k but our
	 * buffer has practical limitation. */
	if (h->total_size < sizeof(*h) || record_size < h->total_size) {
		CPRINTS("Bad size: %d", h->total_size);
		return EC_ERROR_OVERFLOW;
	}

	/* Read the data */
	if (do_eeprom_read(sizeof(*h), h->data, h->total_size - sizeof(*h))) {
		CPRINTS("Failed to read body");
		return EC_ERROR_INVAL;
	}

	/* Check CRC. This supports new fields unknown to this parser. */
	if (datablob_crc8(h) != h->crc) {
		CPRINTS("Bad CRC");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int eeprom_is_write_protected(void)
{
#ifdef CONFIG_WP_ACTIVE_HIGH
	return gpio_get_level(GPIO_WP);
#else
	return !gpio_get_level(GPIO_WP_L);
#endif /* CONFIG_WP_ACTIVE_HIGH */
}

static int eeprom_write(const uint8_t *record, int record_size)
{
	struct datablob_header * const h = (struct datablob_header *)record;
	const uint8_t *p = record;
	int rest = h->total_size;

	while (rest > 0) {
		int size = MIN(EEPROM_PAGE_WRITE_SIZE, rest);
		int rv;
		rv = i2c_write_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
				     p - record, p, size);
		if (rv) {
			CPRINTS("Failed to write for %d", rv);
			return rv;
		}
		/* Wait for internal write cycle completion */
		msleep(EEPROM_PAGE_WRITE_MS);
		p += size;
		rest -= size;
	}
	CPRINTS("Written %d bytes", h->total_size);

	return EC_SUCCESS;
}

const struct datablob_driver eeprom_drv = {
	.save = eeprom_write,
	.load = eeprom_read,
	.is_protected = eeprom_is_write_protected,
};

static struct datablob_cbi {
	const struct datablob_driver *driver;
	int record_size;
	int cache_status;
	uint8_t cache[CBI_EEPROM_SIZE];
} cbi = {
	.driver = &eeprom_drv,
	.record_size = CBI_EEPROM_SIZE,
};

void cbi_invalidate_cache(void)
{
	cbi.cache_status = DATABLOB_CACHE_INVALID;
}

/*
 * Cros Board Info APIs
 */
int cbi_create(void)
{
	datablob_create(&cbi);
	return EC_SUCCESS;
}

__attribute__((weak))
int cbi_board_override(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	return EC_SUCCESS;
}

int cbi_get_board_info(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	int rv = datablob_get_data(&cbi, tag, buf, size);

	if (rv)
		return rv;

	return cbi_board_override(tag, buf, size);
}

int cbi_set_board_info(enum cbi_data_tag tag, const uint8_t *buf, uint8_t size)
{
	return datablob_set_data(&cbi, tag, buf, size);
}

int cbi_write(void)
{
	return datablob_write(&cbi);
}

int cbi_get_board_version(uint32_t *ver)
{
	uint8_t size = sizeof(*ver);

	return cbi_get_board_info(CBI_TAG_BOARD_VERSION, (uint8_t *)ver, &size);
}

int cbi_get_sku_id(uint32_t *id)
{
	uint8_t size = sizeof(*id);

	return cbi_get_board_info(CBI_TAG_SKU_ID, (uint8_t *)id, &size);
}

int cbi_get_oem_id(uint32_t *id)
{
	uint8_t size = sizeof(*id);

	return cbi_get_board_info(CBI_TAG_OEM_ID, (uint8_t *)id, &size);
}

int cbi_get_model_id(uint32_t *id)
{
	uint8_t size = sizeof(*id);

	return cbi_get_board_info(CBI_TAG_MODEL_ID, (uint8_t *)id, &size);
}

int cbi_get_fw_config(uint32_t *fw_config)
{
	uint8_t size = sizeof(*fw_config);

	return cbi_get_board_info(CBI_TAG_FW_CONFIG, (uint8_t *)fw_config,
				  &size);
}

int cbi_get_pcb_supplier(uint32_t *pcb_supplier)
{
	uint8_t size = sizeof(*pcb_supplier);

	return cbi_get_board_info(CBI_TAG_PCB_SUPPLIER, (uint8_t *)pcb_supplier,
			&size);
}

static enum ec_status hc_cbi_get(struct host_cmd_handler_args *args)
{
	const struct __ec_align4 ec_params_get_cbi *p = args->params;
	uint8_t size = MIN(args->response_max, UINT8_MAX);

	if (p->flag & CBI_GET_RELOAD)
		cbi.cache_status = DATABLOB_CACHE_INVALID;

	if (cbi_get_board_info(p->tag, args->response, &size))
		return EC_RES_INVALID_PARAM;

	args->response_size = size;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CROS_BOARD_INFO,
		     hc_cbi_get,
		     EC_VER_MASK(0));

static enum ec_status hc_cbi_set(struct host_cmd_handler_args *args)
{
	const struct __ec_align4 ec_params_set_cbi *p = args->params;

	/*
	 * If we ultimately cannot write to the flash, then fail early unless
	 * we are explicitly trying to write to the in-memory CBI only
	 */
	if (cbi.driver->is_protected() && !(p->flag & CBI_SET_NO_SYNC)) {
		CPRINTS("Failed to write for WP");
		return EC_RES_ACCESS_DENIED;
	}

#ifndef CONFIG_SYSTEM_UNLOCKED
	/* These fields are not allowed to be reprogrammed regardless the
	 * hardware WP state. They're considered as a part of the hardware. */
	if (p->tag == CBI_TAG_BOARD_VERSION || p->tag == CBI_TAG_OEM_ID)
		return EC_RES_ACCESS_DENIED;
#endif

	if (p->flag & CBI_SET_INIT) {
		cbi_create();
	} else {
		if (cbi.driver->load(cbi.cache, cbi.record_size))
			return EC_RES_ERROR;
	}

	if (cbi_set_board_info(p->tag, p->data, p->size))
		return EC_RES_INVALID_PARAM;

	/* Skip write if client asks so. */
	if (p->flag & CBI_SET_NO_SYNC)
		return EC_RES_SUCCESS;

	/* We already checked write protect failure case. */
	if (datablob_write(&cbi))
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_SET_CROS_BOARD_INFO,
		     hc_cbi_set,
		     EC_VER_MASK(0));

#ifdef CONFIG_CMD_CBI

static void print_tag(const char * const tag, int rv, const uint32_t *val)
{
	ccprintf(tag);
	if (rv == EC_SUCCESS && val)
		ccprintf(": %u (0x%x)\n", *val, *val);
	else
		ccprintf(": (Error %d)\n", rv);
}

static void dump_cbi(void)
{
	struct datablob_header * const head =
			(struct datablob_header *)cbi.cache;
	uint32_t val;

	/* Ensure we read the latest data from EEPROM. */
	cbi.cache_status = DATABLOB_CACHE_INVALID;
	cbi.driver->load(cbi.cache, cbi.record_size);

	if (cbi.cache_status != DATABLOB_CACHE_SYNCD) {
		ccprintf("Cannot Read CBI (Error %d)\n", cbi.cache_status);
		return;
	}

	ccprintf("CBI_VERSION: 0x%04x\n", head->version);
	ccprintf("TOTAL_SIZE: %u\n", head->total_size);

	print_tag("BOARD_VERSION", cbi_get_board_version(&val), &val);
	print_tag("OEM_ID", cbi_get_oem_id(&val), &val);
	print_tag("MODEL_ID", cbi_get_model_id(&val), &val);
	print_tag("SKU_ID", cbi_get_sku_id(&val), &val);
	print_tag("FW_CONFIG", cbi_get_fw_config(&val), &val);
	print_tag("PCB_SUPPLIER", cbi_get_pcb_supplier(&val), &val);
}

static int cc_cbi(int argc, char **argv)
{
	dump_cbi();
	if (cbi.cache_status == DATABLOB_CACHE_SYNCD)
		hexdump(cbi.cache, cbi.record_size);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(cbi, cc_cbi, NULL, "Print Cros Board Info from flash");
#endif /* CONFIG_CMD_CBI */

#endif /* !HOST_TOOLS_BUILD */
