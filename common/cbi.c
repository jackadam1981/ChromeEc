/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cros Board Info
 */

#include "cbi_eeprom.h"
#include "common.h"
#include "console.h"
#include "crc8.h"
#include "cros_board_info.h"
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
uint8_t cbi_crc8(const struct cbi_header *h)
{
	return cros_crc8((uint8_t *)&h->crc + 1,
			 h->total_size - sizeof(h->magic) - sizeof(h->crc));
}

uint8_t *cbi_set_data(uint8_t *p, enum cbi_data_tag tag,
		      const void *buf, int size)
{
	struct cbi_data *d = (struct cbi_data *)p;

	/*
	 * If size of the data to be added is zero, then no need to add the tag
	 * as well.
	 */
	if (size == 0)
		return p;

	d->tag = tag;
	d->size = size;
	memcpy(d->value, buf, size);
	p += sizeof(*d) + size;
	return p;
}

uint8_t *cbi_set_string(uint8_t *p, enum cbi_data_tag tag, const char *str)
{
	if (str == NULL)
		return p;

	return cbi_set_data(p, tag, str, strlen(str) + 1);
}

struct cbi_data *cbi_find_tag(const void *buf, enum cbi_data_tag tag)
{
	struct cbi_data *d;
	const struct cbi_header *h = buf;
	const uint8_t *p;
	for (p = h->data; p + sizeof(*d) < (uint8_t *)buf + h->total_size;) {
		d = (struct cbi_data *)p;
		if (d->tag == tag)
			return d;
		p += sizeof(*d) + d->size;
	}
	return NULL;
}

/*
 * Functions and variables specific to EC firmware
 */
#ifndef HOST_TOOLS_BUILD

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "CBI " format, ## args)

static int cached_read_result = EC_ERROR_CBI_CACHE_INVALID;
static uint8_t cbi[CBI_EEPROM_SIZE];
static struct cbi_header * const head = (struct cbi_header *)cbi;

int cbi_get_cache_state(void)
{
	return cached_read_result;
}

void cbi_set_cache_state(int state)
{
	cached_read_result = state ? EC_ERROR_CBI_CACHE_INVALID : EC_SUCCESS;
}

uint8_t *cbi_get_cache(void)
{
	return cbi;
}

void cbi_create(void)
{
	memset(cbi, 0, sizeof(cbi));
	memcpy(head->magic, cbi_magic, sizeof(cbi_magic));
	head->total_size = sizeof(*head);
	head->major_version = CBI_VERSION_MAJOR;
	head->minor_version = CBI_VERSION_MINOR;
	head->crc = cbi_crc8(head);
	cached_read_result = EC_SUCCESS;
}

__attribute__((weak))
int cbi_board_override(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	return EC_SUCCESS;
}

int cbi_get_board_info(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	const struct cbi_data *d;

	if (cbi_read())
		return EC_ERROR_UNKNOWN;

	d = cbi_find_tag(cbi, tag);
	if (!d)
		/* Not found */
		return EC_ERROR_UNKNOWN;
	if (*size < d->size)
		/* Insufficient buffer size */
		return EC_ERROR_INVAL;

	/* Clear the buffer in case len < *size */
	memset(buf, 0, *size);
	/* Copy the value */
	memcpy(buf, d->value, d->size);
	*size = d->size;

	return cbi_board_override(tag, buf, size);
}

static void cbi_remove_tag(void *const cbi, struct cbi_data *const d)
{
	struct cbi_header *const h = cbi;
	const size_t size = sizeof(*d) + d->size;
	const uint8_t *next = (uint8_t *)d + size;
	const size_t bytes_after = ((uint8_t *)cbi + h->total_size) - next;

	memmove(d, next, bytes_after);
	h->total_size -= size;
}

int cbi_set_board_info(enum cbi_data_tag tag, const uint8_t *buf, uint8_t size)
{
	struct cbi_data *d;

	d = cbi_find_tag(cbi, tag);

	/* If we found the entry, but the size doesn't match, delete it */
	if (d && d->size != size) {
		cbi_remove_tag(cbi, d);
		d = NULL;
	}

	if (!d) {
		uint8_t *p;
		/* Not found. Check if new item would fit */
		if (sizeof(cbi) < head->total_size + sizeof(*d) + size)
			return EC_ERROR_OVERFLOW;
		/* Append new item */
		p = cbi_set_data(&cbi[head->total_size], tag, buf, size);
		head->total_size = p - cbi;
	} else {
		/* Overwrite existing item */
		memcpy(d->value, buf, d->size);
	}

	return EC_SUCCESS;
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

int cbi_get_ssfc(uint32_t *ssfc)
{
	uint8_t size = sizeof(*ssfc);

	return cbi_get_board_info(CBI_TAG_SSFC, (uint8_t *)ssfc,
				  &size);
}

int cbi_get_pcb_supplier(uint32_t *pcb_supplier)
{
	uint8_t size = sizeof(*pcb_supplier);

	return cbi_get_board_info(CBI_TAG_PCB_SUPPLIER, (uint8_t *)pcb_supplier,
			&size);
}

int cbi_get_rework_id(uint64_t *id)
{
	uint8_t size = sizeof(*id);
	return cbi_get_board_info(CBI_TAG_REWORK_ID, (uint8_t *)id, &size);
}

static enum ec_status hc_cbi_get(struct host_cmd_handler_args *args)
{
	const struct __ec_align4 ec_params_get_cbi *p = args->params;
	uint8_t size = MIN(args->response_max, UINT8_MAX);

	if (p->flag & CBI_GET_RELOAD)
		cached_read_result = EC_ERROR_CBI_CACHE_INVALID;

	if (cbi_get_board_info(p->tag, args->response, &size))
		return EC_RES_INVALID_PARAM;

	args->response_size = size;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CROS_BOARD_INFO,
		     hc_cbi_get,
		     EC_VER_MASK(0));

#endif /* !HOST_TOOLS_BUILD */
