/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Datablob
 *
 * The datablob is a data structure which packs data items forming a binary
 * blob. Each data item is identified by the 'tags'. CBI uses the datablob.
 */

#ifndef __CROS_EC_DATABLOB_H
#define __CROS_EC_DATABLOB_H

#include "common.h"
#include "ec_commands.h"

#define DATABLOB_VERSION_MAJOR	0
#define DATABLOB_VERSION_MINOR	0

static const uint8_t datablob_magic[] = { 0x43, 0x42, 0x49 };  /* 'C' 'B' 'I' */

#define DATABLOB_CACHE_INVALID	0  /* RAM contents are old */
#define DATABLOB_CACHE_SYNCD	1  /* Contents in flash and RAM match */
#define DATABLOB_CACHE_DIRTY	2  /* Flash contents are old */

struct datablob_header {
	uint8_t magic[3];
	/* CRC of 'struct board_info' excluding magic and crc */
	uint8_t crc;
	/*
	 * Data format version. Parsers are expected to process data as long
	 * as major version is equal or younger.
	 */
	union {
		struct {
			uint8_t minor_version;
			uint8_t major_version;
		};
		uint16_t version;
	};
	/*
	 * Total size of data. It can be larger than sizeof(struct board_info)
	 * if future versions add additional fields.
	 */
	uint16_t total_size;
	/* List of data items (i.e. struct datablob_item[]) */
	uint8_t data[];
} __packed;

struct datablob_item {
	uint8_t tag;		/* e.g. enum cbi_data_tag */
	uint8_t size;		/* size of value[] */
	uint8_t value[];	/* data value */
} __packed;

/*
 * Driver for storage media access
 */
struct datablob_driver {
	/* Write out record from RAM to storage media (i.e. sync) */
	int (*save)(const uint8_t *record, int record_size);
	/* Read record from storage media to RAM */
	int (*load)(uint8_t *record, int record_size);
	/* Erase a page. Not implemented unless necessary. */
	int (*erase)(void);
	/* Return write protect status */
	int (*is_protected)(void);
};

/*
 * Datablob descriptor
 *
 * It describes the configuration and the status of a datablob system.
 * This is a template. Don't instantiate it. A sub-class should be defined from
 * it for each system.
 */
struct datablob {
	struct datablob_driver *driver;
	/* Size of one record in bytes */
	int record_size;
	/* Cache status (i.e. DATABLOB_CACHE_*) */
	int cache_status;
	/* cache */
	uint8_t cache[];
};

/**
 * Compute CRC8 of a datablob record
 *
 * @param h Pointer to datablob header CRC8 is computed for.
 * @return  CRC8
 */
uint8_t datablob_crc8(const struct datablob_header *h);

/**
 * Add data item to datablob
 *
 * @param p     Pointer where item is added
 * @param tag   Tag of data to be added.
 * @param buf   Data value to be added.
 * @param size  Size of <buf> in bytes.
 * @return      Pointer to next available slot.
 */
uint8_t *datablob_add_data(uint8_t *p, int tag, const void *buf, int size);

/**
 * Find data item by tag
 *
 * @param record  Datablob to be searched.
 * @param tag     Tag of data item to be searched.
 * @return        Data item found.
 */
struct datablob_item *datablob_find_tag(const void *record, int tag);

/**
 * Create a new datablob
 *
 * @param blob  Pointer to buffer where new datablob is created.
 */
void datablob_create(void *blob);

/**
 * Set data in datablob
 *
 * @param blob  Datablob data is set to.
 * @param tag   Tag of data to be set.
 * @param buf   Data value to be set.
 * @param size  Size of <buf> in bytes.
 * @return      EC_SUCCESS or EC_ERROR_*
 */
int datablob_set_data(void *blob, int tag, const uint8_t *buf, uint8_t size);

/**
 * Set data in datablob
 *
 * @param blob  Datablob data is get from.
 * @param tag   Tag of data to be set.
 * @param buf   Buffer where data value is copied to.
 * @param size  Size of <buf> in bytes.
 * @return      EC_SUCCESS or EC_ERROR_*
 */
int datablob_get_data(void *blob, int tag, uint8_t *buf, uint8_t *size);

/**
 * Write datablob cache to storage media
 *
 * @param blob  Datablob to be written.
 * @return      EC_SUCCESS or EC_ERROR_*
 */
int datablob_write(void *blob);

#endif
