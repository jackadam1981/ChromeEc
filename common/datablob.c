/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Datablob implementation
 */

#include "common.h"
#include "crc8.h"
#include "datablob.h"

#ifdef HOST_TOOLS_BUILD
#include <string.h>
#else
#include "util.h"
#endif

uint8_t datablob_crc8(const struct datablob_header *h)
{
	return crc8((uint8_t *)&h->crc + 1,
		    h->total_size - sizeof(h->magic) - sizeof(h->crc));
}

uint8_t *datablob_add_data(uint8_t *p, int tag, const void *buf, int size)
{
	struct datablob_item *d = (struct datablob_item *)p;

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

struct datablob_item *datablob_find_tag(const void *record, int tag)
{
	struct datablob_item *d;
	const struct datablob_header *h = record;
	const uint8_t *p;

	for (p = h->data; p + sizeof(*d) < (uint8_t *)record + h->total_size;) {
		d = (struct datablob_item *)p;
		if (d->tag == tag)
			return d;
		p += sizeof(*d) + d->size;
	}
	return NULL;
}

void datablob_create(void *blob)
{
	struct datablob *b = blob;
	struct datablob_header * const h = (struct datablob_header *)b->cache;

	memset(b->cache, 0, b->record_size);
	memcpy(h->magic, datablob_magic, sizeof(datablob_magic));
	h->total_size = sizeof(*h);
	h->major_version = DATABLOB_VERSION_MAJOR;
	h->minor_version = DATABLOB_VERSION_MINOR;
	h->crc = datablob_crc8(h);
	b->cache_status = DATABLOB_CACHE_DIRTY;
}

static int datablob_read(struct datablob *blob)
{
	struct datablob_driver *drv = blob->driver;
	int i;
	int rv;

	if (blob->cache_status != DATABLOB_CACHE_INVALID)
		return EC_SUCCESS;

	for (i = 0; i < 2; i++) {
		rv = drv->load(blob->cache, blob->record_size);
		if (rv == EC_SUCCESS) {
			blob->cache_status = DATABLOB_CACHE_SYNCD;
			return EC_SUCCESS;
		}
		/* On error (I2C or bad contents), retry a read */
	}

	return rv;
}

int datablob_get_data(void *blob, int tag, uint8_t *buf, uint8_t *size)
{
	struct datablob *b = blob;
	const struct datablob_item *d;
	int rv;

	rv = datablob_read(b);
	if (rv)
		return rv;

	d = datablob_find_tag(b->cache, tag);
	if (!d)
		/* Not found */
		return EC_ERROR_NOT_FOUND;
	if (*size < d->size)
		/* Insufficient buffer size */
		return EC_ERROR_INVAL;

	/* Clear the buffer in case len < *size */
	memset(buf, 0, *size);
	/* Copy the value */
	memcpy(buf, d->value, d->size);
	*size = d->size;

	return EC_SUCCESS;
}

static void datablob_remove_tag(void *const record, struct datablob_item *d)
{
	struct datablob_header *const h = record;
	const size_t size = sizeof(*d) + d->size;
	const uint8_t *next = (uint8_t *)d + size;
	const size_t bytes_after = ((uint8_t *)record + h->total_size) - next;

	memmove(d, next, bytes_after);
	h->total_size -= size;
}

int datablob_set_data(void *blob, int tag, const uint8_t *buf, uint8_t size)
{
	struct datablob *b = blob;
	struct datablob_header * const h = (struct datablob_header *)b->cache;
	struct datablob_item *d;
	uint8_t crc;
	int rv;

	rv = datablob_read(b);
	if (rv == EC_ERROR_INVAL) {
		datablob_create(blob);
		if (b->driver->erase && b->driver->erase())
			return EC_ERROR_UNKNOWN;
	} else if (rv != EC_SUCCESS) {
		return rv;
	}

	crc = datablob_crc8(h);

	d = datablob_find_tag(b->cache, tag);

	/* If we found the entry, but the size doesn't match, delete it */
	if (d && d->size != size) {
		datablob_remove_tag(b->cache, d);
		d = NULL;
		h->major_version = DATABLOB_VERSION_MAJOR;
		h->minor_version = DATABLOB_VERSION_MINOR;
		h->crc = datablob_crc8(h);
		b->cache_status = DATABLOB_CACHE_DIRTY;
	}

	if (!d) {
		uint8_t *p;
		/* Not found. Check if new item would fit */
		if (b->record_size < h->total_size + sizeof(*d) + size)
			return EC_ERROR_OVERFLOW;
		/* Append new item */
		p = datablob_add_data(&b->cache[h->total_size], tag, buf, size);
		h->total_size = p - b->cache;
	} else {
		/* Overwrite existing item */
		memcpy(d->value, buf, d->size);
	}

	h->major_version = DATABLOB_VERSION_MAJOR;
	h->minor_version = DATABLOB_VERSION_MINOR;

	h->crc = datablob_crc8(h);
	if (h->crc != crc)
		b->cache_status = DATABLOB_CACHE_DIRTY;

	return EC_SUCCESS;
}

int datablob_write(void *blob)
{
	struct datablob *b = blob;
	int rv;

	if (b->driver->is_protected())
		return EC_ERROR_ACCESS_DENIED;

	if (b->cache_status == DATABLOB_CACHE_SYNCD)
		return EC_SUCCESS;

	rv = b->driver->save(b->cache, b->record_size);
	if (rv == EC_SUCCESS)
		b->cache_status = DATABLOB_CACHE_SYNCD;

	return rv;
}
