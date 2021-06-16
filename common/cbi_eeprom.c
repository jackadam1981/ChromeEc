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

static int read_eeprom(uint8_t offset, uint8_t *in, int in_size)
{
	return i2c_read_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
			      offset, in, in_size);
}

/*
 * Get board information from EEPROM
 */
static int read_board_info_eeprom(void)
{
	uint8_t *cbi = cbi_get_cache();
	struct cbi_header * const head = (struct cbi_header *)cbi;

	CPRINTS("Reading board info");

	/* Read header */
	if (read_eeprom(0, cbi, sizeof(*head))) {
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
			head->total_size > CBI_EEPROM_SIZE) {
		CPRINTS("Bad size: %d", head->total_size);
		return EC_ERROR_OVERFLOW;
	}

	/* Read the data */
	if (read_eeprom(sizeof(*head), head->data,
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

int cbi_read(void)
{
	if (cbi_get_cache_state() == EC_ERROR_CBI_CACHE_INVALID) {
		cbi_set_cache_state(read_board_info_eeprom());
		if (cbi_get_cache_state() == EC_ERROR_CBI_CACHE_INVALID)
			/* On error (I2C or bad contents), retry a read */
			cbi_set_cache_state(read_board_info_eeprom());
	}
	/*
	 * Else, we already tried and know the result. Return the cached
	 * error code immediately to avoid wasteful reads.
	 */
	return cbi_get_cache_state();
}

int cbi_write(void)
{
	uint8_t *p = cbi_get_cache();
	int rest = ((struct cbi_header *)p)->total_size;

	if (eeprom_is_write_protected()) {
		CPRINTS("Failed to write for WP");
		return EC_ERROR_ACCESS_DENIED;
	}

	while (rest > 0) {
		int size = MIN(EEPROM_PAGE_WRITE_SIZE, rest);
		int rv;

		rv = i2c_write_block(I2C_PORT_EEPROM, I2C_ADDR_EEPROM_FLAGS,
				     p - cbi_get_cache(), p, size);
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

static enum ec_status common_cbi_set(const struct __ec_align4
							ec_params_set_cbi * p)
{
	struct cbi_header * const head =
			(struct cbi_header *)cbi_get_cache();

	/*
	 * If we ultimately cannot write to the flash, then fail early unless
	 * we are explicitly trying to write to the in-memory CBI only
	 */
	if (eeprom_is_write_protected() && !(p->flag & CBI_SET_NO_SYNC)) {
		CPRINTS("Failed to write for WP");
		return EC_RES_ACCESS_DENIED;
	}

#ifndef CONFIG_SYSTEM_UNLOCKED
	/*
	 * These fields are not allowed to be reprogrammed regardless the
	 * hardware WP state. They're considered as a part of the hardware.
	 */
	if (p->tag == CBI_TAG_BOARD_VERSION || p->tag == CBI_TAG_OEM_ID)
		return EC_RES_ACCESS_DENIED;
#endif

	if (p->flag & CBI_SET_INIT) {
		memset(head, 0, CBI_EEPROM_SIZE);
		memcpy(head->magic, cbi_magic, sizeof(cbi_magic));
		head->total_size = sizeof(*head);
		cbi_set_cache_state(EC_SUCCESS);
	} else {
		if (cbi_read())
			return EC_RES_ERROR;
	}

	if (cbi_set_board_info(p->tag, p->data, p->size))
		return EC_RES_INVALID_PARAM;

	/*
	 * Whether we're modifying existing data or creating new one,
	 * we take over the format.
	 */
	head->major_version = CBI_VERSION_MAJOR;
	head->minor_version = CBI_VERSION_MINOR;
	head->crc = cbi_crc8(head);

	/* Skip write if client asks so. */
	if (p->flag & CBI_SET_NO_SYNC)
		return EC_RES_SUCCESS;

	/* We already checked write protect failure case. */
	if (cbi_write())
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static enum ec_status hc_cbi_set(struct host_cmd_handler_args *args)
{
	const struct __ec_align4 ec_params_set_cbi * p = args->params;

	/* Given data size exceeds the packet size. */
	if (args->params_size < sizeof(*p) + p->size)
		return EC_RES_INVALID_PARAM;

	return common_cbi_set(p);
}
DECLARE_HOST_COMMAND(EC_CMD_SET_CROS_BOARD_INFO,
		     hc_cbi_set,
		     EC_VER_MASK(0));

#ifdef CONFIG_CMD_CBI
static void dump_flash(void)
{
	uint8_t buf[16];
	int i;

	for (i = 0; i < CBI_EEPROM_SIZE; i += sizeof(buf)) {
		if (read_eeprom(i, buf, sizeof(buf))) {
			ccprintf("\nFailed to read EEPROM\n");
			return;
		}
		hexdump(buf, sizeof(buf));
	}
}

static void print_tag(const char * const tag, int rv, const uint32_t *val)
{
	ccprintf("%s", tag);
	if (rv == EC_SUCCESS && val)
		ccprintf(": %u (0x%x)\n", *val, *val);
	else
		ccprintf(": (Error %d)\n", rv);
}

static void print_uint64_tag(const char * const tag, int rv,
			     const uint64_t *lval)
{
	ccprintf("%s", tag);
	if (rv == EC_SUCCESS && lval)
		ccprintf(": %llu (0x%llx)\n", *(unsigned long long *)lval,
			 *(unsigned long long *)lval);
	else
		ccprintf(": (Error %d)\n", rv);
}

static void dump_cbi(void)
{
	uint32_t val;
	uint64_t lval;
	int cache_state;
	struct cbi_header * const head =
			(struct cbi_header *)cbi_get_cache();

	/* Ensure we read the latest data from flash. */
	cbi_set_cache_state(EC_ERROR_CBI_CACHE_INVALID);
	cache_state = cbi_read();

	if (cache_state) {
		ccprintf("Cannot Read CBI (Error %d)\n", cache_state);
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
	print_tag("SSFC", cbi_get_ssfc(&val), &val);
	print_uint64_tag("REWORK_ID", cbi_get_rework_id(&lval), &lval);
}

/*
 * Space for the set command (does not include data space) plus maximum
 * possible console input
 */
static uint8_t buf[sizeof(struct ec_params_set_cbi) + \
		       CONFIG_CONSOLE_INPUT_LINE_SIZE];

static int cc_cbi(int argc, char **argv)
{
	struct __ec_align4 ec_params_set_cbi * setter =
		(struct __ec_align4 ec_params_set_cbi *)buf;
	int last_arg;
	char *e;

	if (argc == 1) {
		dump_cbi();
		dump_flash();
		return EC_SUCCESS;
	}

	if (strcasecmp(argv[1], "set") == 0) {
		if (argc < 5) {
			ccprintf("Set requires: <tag> <value> <size>\n");
			return EC_ERROR_PARAM_COUNT;
		}

		setter->tag = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		if (setter->tag == CBI_TAG_DRAM_PART_NUM ||
		    setter->tag == CBI_TAG_OEM_NAME) {
			setter->size = strlen(argv[3]) + 1;
			memcpy(setter->data, argv[3], setter->size);
		} else {
			uint64_t val = strtoull(argv[3], &e, 0);

			if (*e)
				return EC_ERROR_PARAM3;

			setter->size = strtoi(argv[4], &e, 0);
			if (*e)
				return EC_ERROR_PARAM4;

			if (setter->size < 1) {
				ccprintf("Set size too small\n");
				return EC_ERROR_PARAM4;
			} else if (setter->tag == CBI_TAG_REWORK_ID &&
				   setter->size > 8) {
				ccprintf("Set size too large\n");
				return EC_ERROR_PARAM4;
			} else if (setter->size > 4) {
				ccprintf("Set size too large\n");
				return EC_ERROR_PARAM4;
			}

			memcpy(setter->data, &val, setter->size);
		}

		last_arg = 5;
	} else if (strcasecmp(argv[1], "remove") == 0) {
		if (argc < 3) {
			ccprintf("Remove requires: <tag>\n");
			return EC_ERROR_PARAM_COUNT;
		}

		setter->tag = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		setter->size = 0;
		last_arg = 3;
	} else {
		return EC_ERROR_PARAM1;
	}

	setter->flag = 0;

	if (argc > last_arg) {
		int i;

		for (i = last_arg; i < argc; i++) {
			if (strcasecmp(argv[i], "init") == 0) {
				setter->flag |= CBI_SET_INIT;
			} else if (strcasecmp(argv[i], "skip_write") == 0) {
				setter->flag |= CBI_SET_NO_SYNC;
			} else {
				ccprintf("Invalid additional option\n");
				return EC_ERROR_PARAM1 + i - 1;
			}
		}
	}

	if (common_cbi_set(setter) == EC_RES_SUCCESS)
		return EC_SUCCESS;

	return EC_ERROR_UNKNOWN;
}
DECLARE_CONSOLE_COMMAND(cbi, cc_cbi, "[set <tag> <value> <size> | "
			"remove <tag>] [init | skip_write]",
			"Print or change Cros Board Info from flash");
#endif /* CONFIG_CMD_CBI */

#ifndef HAS_TASK_CHIPSET
int cbi_set_fw_config(uint32_t fw_config)
{
	struct cbi_header * const head =
			(struct cbi_header *)cbi_get_cache();

	/* Check write protect status */
	if (eeprom_is_write_protected())
		return EC_ERROR_ACCESS_DENIED;

	/* Ensure that CBI has been configured */
	if (read_board_info_eeprom())
		cbi_create();

	/* Update the FW_CONFIG field */
	cbi_set_board_info(CBI_TAG_FW_CONFIG, (uint8_t *)&fw_config,
			   sizeof(int));

	/* Update CRC calculation and write to serial EEPROM */
	head->crc = cbi_crc8(head);
	if (cbi_write())
		return EC_ERROR_UNKNOWN;

	dump_cbi();

	return EC_SUCCESS;
}
#endif
