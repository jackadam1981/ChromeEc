/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Chrome EC - common functions */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "host_command.h"
#include "shared_mem.h"
#include "system.h"
#include "util.h"
#include "vboot_hash.h"

/*
 * Contents of erased flash, as a 32-bit value.  Most platforms erase flash
 * bits to 1.
 */
#ifndef CONFIG_FLASH_ERASED_VALUE32
#define CONFIG_FLASH_ERASED_VALUE32 (-1U)
#endif

#if 0

/**
 * Return the base pointer for the image copy, or 0xffffffff if error.
 */
uintptr_t flash_image_get_base(enum system_image_copy_t copy)
{
	switch (copy) {
	case SYSTEM_IMAGE_RO:
		return CONFIG_FLASH_BASE + CONFIG_FW_RO_OFF;
	case SYSTEM_IMAGE_RW:
		return CONFIG_FLASH_BASE + CONFIG_FW_RW_OFF;
	default:
		return 0xffffffff;
	}
}

/**
 * Return the size of the image copy, or 0 if error.
 */
uint32_t flash_image_get_size(enum system_image_copy_t copy)
{
	switch (copy) {
	case SYSTEM_IMAGE_RO:
		return CONFIG_FW_RO_SIZE;
	case SYSTEM_IMAGE_RW:
		return CONFIG_FW_RW_SIZE;
	default:
		return 0;
	}
}
#endif
int flash_dataptr(int offset, int size_req, int align, const char **ptrp)
{
	if (offset < 0 || size_req < 0 ||
			offset + size_req > CONFIG_FLASH_SIZE ||
			(offset | size_req) & (align - 1))
		return -1;  /* Invalid range */
	if (ptrp)
		*ptrp = flash_physical_dataptr(offset);

	return CONFIG_FLASH_SIZE - offset;
}

int flash_is_erased(uint32_t offset, int size)
{
	const uint32_t *ptr;

	if (flash_dataptr(offset, size, sizeof(uint32_t),
			  (const char **)&ptr) < 0)
		return 0;

	for (size /= sizeof(uint32_t); size > 0; size--, ptr++)
		if (*ptr != CONFIG_FLASH_ERASED_VALUE32)
			return 0;

	return 1;
}

int flash_write(int offset, int size, const char *data)
{
	if (flash_dataptr(offset, size, CONFIG_FLASH_WRITE_SIZE, NULL) < 0)
		return EC_ERROR_INVAL;  /* Invalid range */

#ifdef CONFIG_VBOOT_HASH
	vboot_hash_invalidate(offset, size);
#endif

	return flash_physical_write(offset, size, data);
}



int flash_erase(int offset, int size)
{
	if (flash_dataptr(offset, size, CONFIG_FLASH_ERASE_SIZE, NULL) < 0)
		return EC_ERROR_INVAL;  /* Invalid range */

#ifdef CONFIG_VBOOT_HASH
	vboot_hash_invalidate(offset, size);
#endif

	return flash_physical_erase(offset, size);
}


/*****************************************************************************/
/* Console commands */

static int command_flash_info(int argc, char **argv)
{
	int i;

	ccprintf("Physical:%4d KB\n", CONFIG_FLASH_PHYSICAL_SIZE / 1024);
	ccprintf("Usable:  %4d KB\n", CONFIG_FLASH_SIZE / 1024);
	ccprintf("Write:   %4d B (ideal %d B)\n", CONFIG_FLASH_WRITE_SIZE,
		 CONFIG_FLASH_WRITE_IDEAL_SIZE);
	ccprintf("Erase:   %4d B (to %d-bits)\n", CONFIG_FLASH_ERASE_SIZE,
		 CONFIG_FLASH_ERASED_VALUE32 ? 1 : 0);
	ccprintf("Protect: %4d B\n", CONFIG_FLASH_BANK_SIZE);

	i = flash_get_protect();
	ccprintf("Flags:  ");
	if (i & EC_FLASH_PROTECT_GPIO_ASSERTED)
		ccputs(" wp_gpio_asserted");
	if (i & EC_FLASH_PROTECT_RO_AT_BOOT)
		ccputs(" ro_at_boot");
	if (i & EC_FLASH_PROTECT_ALL_AT_BOOT)
		ccputs(" all_at_boot");
	if (i & EC_FLASH_PROTECT_RO_NOW)
		ccputs(" ro_now");
	if (i & EC_FLASH_PROTECT_ALL_NOW)
		ccputs(" all_now");
	if (i & EC_FLASH_PROTECT_ERROR_STUCK)
		ccputs(" STUCK");
	if (i & EC_FLASH_PROTECT_ERROR_INCONSISTENT)
		ccputs(" INCONSISTENT");
	ccputs("\n");

	ccputs("Protected now:");
	for (i = 0; i < CONFIG_FLASH_PHYSICAL_SIZE / CONFIG_FLASH_BANK_SIZE;
	     i++) {
		if (!(i & 31))
			ccputs("\n    ");
		else if (!(i & 7))
			ccputs(" ");
		ccputs(flash_physical_get_protect(i) ? "Y" : ".");
	}
	ccputs("\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flashinfo, command_flash_info,
			NULL,
			"Print flash info",
			NULL);

#ifdef CONFIG_CMD_FLASH
static int command_flash_erase(int argc, char **argv)
{
	int offset = -1;
	int size = CONFIG_FLASH_ERASE_SIZE;
	int rv;

	if (flash_get_protect() & EC_FLASH_PROTECT_ALL_NOW)
		return EC_ERROR_ACCESS_DENIED;

	rv = parse_offset_size(argc, argv, 1, &offset, &size);
	if (rv)
		return rv;

	ccprintf("Erasing %d bytes at 0x%x...\n", size, offset, offset);
	return flash_erase(offset, size);
}
DECLARE_CONSOLE_COMMAND(flasherase, command_flash_erase,
			"offset [size]",
			"Erase flash",
			NULL);

static int command_flash_write(int argc, char **argv)
{
	int offset = -1;
	int size = CONFIG_FLASH_ERASE_SIZE;
	int rv;
	char *data;
	int i;

	if (flash_get_protect() & EC_FLASH_PROTECT_ALL_NOW)
		return EC_ERROR_ACCESS_DENIED;

	rv = parse_offset_size(argc, argv, 1, &offset, &size);
	if (rv)
		return rv;

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

	ccprintf("Writing %d bytes to 0x%x...\n",
		 size, offset, offset);
	rv = flash_write(offset, size, data);

	/* Free the buffer */
	shared_mem_release(data);

	return rv;
}
DECLARE_CONSOLE_COMMAND(flashwrite, command_flash_write,
			"offset [size]",
			"Write pattern to flash",
			NULL);

#endif

static int command_flash_wp(int argc, char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "enable"))
		return flash_set_protect(EC_FLASH_PROTECT_RO_AT_BOOT, -1);
	else if (!strcasecmp(argv[1], "disable"))
		return flash_set_protect(EC_FLASH_PROTECT_RO_AT_BOOT, 0);
	else if (!strcasecmp(argv[1], "now"))
		return flash_set_protect(EC_FLASH_PROTECT_ALL_NOW, -1);
	else if (!strcasecmp(argv[1], "rw"))
		return flash_set_protect(EC_FLASH_PROTECT_ALL_AT_BOOT, -1);
	else if (!strcasecmp(argv[1], "norw"))
		return flash_set_protect(EC_FLASH_PROTECT_ALL_AT_BOOT, 0);
	else
		return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND(flashwp, command_flash_wp,
			"<enable | disable | now | rw | norw>",
			"Modify flash write protect",
			NULL);

/*****************************************************************************/
/* Host commands */

static int flash_command_get_info(struct host_cmd_handler_args *args)
{
	struct ec_response_flash_info_1 *r = args->response;

	r->flash_size = CONFIG_FLASH_SIZE;
	r->write_block_size = CONFIG_FLASH_WRITE_SIZE;
	r->erase_block_size = CONFIG_FLASH_ERASE_SIZE;
	r->protect_block_size = CONFIG_FLASH_BANK_SIZE;

	if (args->version == 0) {
		/* Only version 0 fields returned */
		args->response_size = sizeof(struct ec_response_flash_info);
	} else {
		/* Fill in full version 1 struct */

		/*
		 * Compute the ideal amount of data for the host to send us,
		 * based on the maximum response size and the ideal write size.
		 */
		r->write_ideal_size =
			(args->response_max -
			 sizeof(struct ec_params_flash_write)) &
			~(CONFIG_FLASH_WRITE_IDEAL_SIZE - 1);
		/*
		 * If we can't get at least one ideal block, then just want
		 * as high a multiple of the minimum write size as possible.
		 */
		if (!r->write_ideal_size)
			r->write_ideal_size =
				(args->response_max -
				 sizeof(struct ec_params_flash_write)) &
				~(CONFIG_FLASH_WRITE_SIZE - 1);

		r->flags = 0;

#if (CONFIG_FLASH_ERASED_VALUE32 == 0)
		r->flags |= EC_FLASH_INFO_ERASE_TO_0;
#endif

		args->response_size = sizeof(*r);
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_INFO,
		     flash_command_get_info,
		     EC_VER_MASK(0) | EC_VER_MASK(1));

static int flash_command_read(struct host_cmd_handler_args *args)
{
	const struct ec_params_flash_read *p = args->params;
	const char *src;

	if (flash_dataptr(p->offset, p->size, 1, &src) < 0)
		return EC_RES_ERROR;

	if (p->size > args->response_max)
		return EC_RES_OVERFLOW;

	memcpy(args->response, src, p->size);
	args->response_size = p->size;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_READ,
		     flash_command_read,
		     EC_VER_MASK(0));

/**
 * Flash write command
 *
 * Version 0 and 1 are equivalent from the EC-side; the only difference is
 * that the host can only send 64 bytes of data at a time in version 0.
 */
static int flash_command_write(struct host_cmd_handler_args *args)
{
	const struct ec_params_flash_write *p = args->params;

	if (flash_get_protect() & EC_FLASH_PROTECT_ALL_NOW)
		return EC_RES_ACCESS_DENIED;

	if (p->size + sizeof(*p) > args->params_size)
		return EC_RES_INVALID_PARAM;

	if (system_unsafe_to_overwrite(p->offset, p->size))
		return EC_RES_ACCESS_DENIED;

	if (flash_write(p->offset, p->size, (const uint8_t *)(p + 1)))
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_WRITE,
		     flash_command_write,
		     EC_VER_MASK(0) | EC_VER_MASK(EC_VER_FLASH_WRITE));

static int flash_command_erase(struct host_cmd_handler_args *args)
{
	const struct ec_params_flash_erase *p = args->params;

	if (flash_get_protect() & EC_FLASH_PROTECT_ALL_NOW)
		return EC_RES_ACCESS_DENIED;

	if (system_unsafe_to_overwrite(p->offset, p->size))
		return EC_RES_ACCESS_DENIED;

	/* Indicate that we might be a while */
#if defined(HAS_TASK_HOSTCMD) && defined(CONFIG_HOST_COMMAND_STATUS)
	args->result = EC_RES_IN_PROGRESS;
	host_send_response(args);
#endif
	if (flash_erase(p->offset, p->size))
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_ERASE,
		     flash_command_erase,
		     EC_VER_MASK(0));



