/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Chrome EC - common functions */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "host_command.h"
#include "spi.h"
#include "spi_flash.h"
#include "system.h"
#include "util.h"
#include "watchdog.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/*
  * Buffer allocated to read data from spi Flash
  *
  */
#define FLASH_EXT_DATACHUNK_SIZE 256
static uint8_t flash_data[FLASH_EXT_DATACHUNK_SIZE];



/*****************************************************************************/
/* Physical layer APIs */

/**
 * Get the physical memory address of a flash offset
 *
 * This is used for direct flash access. We assume that the flash is
 * contiguous from this start address through to the end of the usable
 * flash.
 *
 * @param offset	Flash offset to get address of
 * @param dataptrp	Returns pointer to memory address of flash offset
 * @return pointer to flash memory offset, if ok, else NULL
 */
const char *flash_physical_dataptr(int offset)
{
	return (char *)((uintptr_t)CONFIG_FLASH_BASE_SPI + offset);
}

int flash_physical_write(int offset, int size, const char *data)
{
	int i;

	offset += CONFIG_FLASH_BASE_SPI;

	/* Fail if offset, size, and data aren't at least word-aligned */
	if ((offset | size | (uint32_t)(uintptr_t)data) & 3)
		return EC_ERROR_INVAL;

	spi_enable(1);

	for (i = 0; i < size; i += 16)
		spi_flash_write(offset+i, 16, &data[i]);

	spi_enable(0);

	return EC_SUCCESS;
}

int flash_physical_read(int offset, int size, char *data)
{

	offset += CONFIG_FLASH_BASE_SPI;

	/* Fail if offset, size, and data aren't at least word-aligned */
	if ((offset | size | (uint32_t)(uintptr_t)data) & 3)
		return EC_ERROR_INVAL;

	spi_enable(1);

	spi_flash_read((uint8_t *)data, offset, size);

	spi_enable(0);

	return EC_SUCCESS;
}


int flash_physical_erase(int offset, int size)
{


	offset += CONFIG_FLASH_BASE_SPI;

	spi_enable(1);

	for (; size > 0; size -= CONFIG_FLASH_ERASE_SIZE,
		     offset += CONFIG_FLASH_ERASE_SIZE) {

		/* Do nothing if already erased */
		if (spi_flash_erase(offset, CONFIG_FLASH_ERASE_SIZE))
			return EC_ERROR_UNKNOWN;

		/*
		 * Reload the watchdog timer, so that erasing many flash pages
		 * doesn't cause a watchdog reset.  May not need this now that
		 * we're using msleep() below.
		 */
		watchdog_reload();
	}
	spi_enable(0);

	return EC_SUCCESS;
}

int flash_read(int offset, int size, char *data)
{
	if (flash_dataptr(offset, size, 1, NULL) < 0)
		return EC_ERROR_INVAL;  /* Invalid range */

#ifdef CONFIG_VBOOT_HASH
	vboot_hash_invalidate(offset, size);
#endif
	return flash_physical_read(offset, size, data);
}

int flash_physical_get_protect(int bank)
{
	/* TODO(crosbug.com/p/36076): IMPLEMENT ME ! */
	return 1;
}

int flash_physical_protect_now(int bank)
{
	/* TODO(crosbug.com/p/36076): IMPLEMENT ME ! */
	return 1;
}
int flash_protect_at_boot(enum flash_wp_range range)
{

	/* TODO(crosbug.com/p/36076): IMPLEMENT ME ! */
	return EC_SUCCESS;
}

uint32_t flash_get_protect(void)
{
	/* TODO(crosbug.com/p/36076): IMPLEMENT ME ! */
	return 0;
}

int flash_set_protect(uint32_t mask, uint32_t flags)
{
	/* TODO(crosbug.com/p/36076): IMPLEMENT ME ! */
	return 0;
}

#ifdef CONFIG_CMD_FLASH
static int command_flash_read(int argc, char **argv)
{
	int offset = -1;
	int size = 256;
	int rv;
	char *data;
	int i;

	rv = parse_offset_size(argc, argv, 1, &offset, &size);
	if (rv)
		return rv;

	if (size > 256)
		size = 256;

	if (size > shared_mem_size())
		size = shared_mem_size();

	/* Acquire the shared memory buffer */
	rv = shared_mem_acquire(size, &data);
	if (rv) {
		ccputs("Can't get shared mem\n");
		return rv;
	}

	/* Fill the data buffer with a pattern */
	for (i = 0; i < size; i++)
		data[i] = i;

	ccprintf("Reading %d bytes\n", size);
	rv = flash_read(offset, size, data);

	/* Printing 256 bytes of the Flash data*/
	for (i = 0; i < size; i++) {
		ccprintf("0x%x ", data[i]);
		if ((i != 0) && ((i % 0x10) == 0))
			ccprintf("\n");
	}

	/* Free the buffer */
	shared_mem_release(data);

	return rv;
}
DECLARE_CONSOLE_COMMAND(flashread, command_flash_read,
			"offset [size]",
			"Read from Flash",
			NULL);
#endif
/*****************************************************************************/
/* Host commands */



static int flash_command_protect(struct host_cmd_handler_args *args)
{
	/* TODO(crosbug.com/p/36076): IMPLEMENT ME ! */
	return EC_RES_SUCCESS;
}

/*
 * TODO(crbug.com/239197) : Adding both versions to the version mask is a
 * temporary workaround for a problem in the cros_ec driver. Drop
 * EC_VER_MASK(0) once cros_ec driver can send the correct version.
 */
DECLARE_HOST_COMMAND(EC_CMD_FLASH_PROTECT,
		     flash_command_protect,
		     EC_VER_MASK(0) | EC_VER_MASK(1));

static int flash_command_region_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_flash_region_info *p = args->params;
	struct ec_response_flash_region_info *r = args->response;

	switch (p->region) {
	case EC_FLASH_REGION_RO:
		r->offset = CONFIG_RO_SPI_OFF;
		r->size = CONFIG_FW_RO_SIZE;
		break;
	case EC_FLASH_REGION_RW:
		r->offset = CONFIG_RW_SPI_OFF;
		r->size = CONFIG_FW_RW_SIZE;
		break;
	case EC_FLASH_REGION_WP_RO:
		r->offset = CONFIG_RO_WP_SPI_OFF;
		r->size = CONFIG_FW_WP_RO_SIZE;
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_REGION_INFO,
		     flash_command_region_info,
		     EC_VER_MASK(EC_VER_FLASH_REGION_INFO));
