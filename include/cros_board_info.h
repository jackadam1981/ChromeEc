/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cros Board Info
 */
#ifndef __CROS_EC_CROS_BOARD_INFO_H
#define __CROS_EC_CROS_BOARD_INFO_H

#include "common.h"
#include "datablob.h"
#include "ec_commands.h"

#define CBI_EEPROM_SIZE		256

/**
 * Board info accessors
 *
 * @param version/sku_id/oem_id/id/fw_config [OUT] Data read from EEPROM
 * @return EC_SUCCESS on success or EC_ERROR_* otherwise.
 *         EC_ERROR_BUSY to indicate data is not ready.
 */
int cbi_get_board_version(uint32_t *version);
int cbi_get_sku_id(uint32_t *sku_id);
int cbi_get_oem_id(uint32_t *oem_id);
int cbi_get_model_id(uint32_t *id);
int cbi_get_fw_config(uint32_t *fw_config);
int cbi_get_pcb_supplier(uint32_t *pcb_supplier);

/**
 * Get data from CBI store
 *
 * @param tag   Tag of the target data.
 * @param buf   Buffer where data is passed.
 * @param size  (IN) Size of <buf>. (OUT) Size of the data returned.
 * @return EC_SUCCESS on success or EC_ERROR_* otherwise.
 *         EC_ERROR_BUSY to indicate data is not ready.
 */
int cbi_get_board_info(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size);

/**
 * Set data in CBI store
 *
 * @param tag   Tag of the target data.
 * @param buf   Buffer where data is passed.
 * @param size  (IN) Size of <buf>. (OUT) Size of the data returned.
 * @return EC_SUCCESS on success or EC_ERROR_* otherwise.
 */
int cbi_set_board_info(enum cbi_data_tag tag, const uint8_t *buf, uint8_t size);

/*
 * Utility functions
 */

/**
 * Calculate 8-bit CRC of CBI
 *
 * @param h	Pointer to CBI header
 * @return	CRC value
 */
uint8_t cbi_crc8(const struct datablob_header *h);

/**
 * Store data in memory in CBI data format
 *
 * @param p	Pointer to the buffer where a new data item will be stored. It
 * 		should be pointing to the data section of CBI.
 * @param tag	Tag of the data item
 * @param buf	Pointer to the buffer containing the data being copied.
 * @param size	Size of the data in bytes. Must be 0 < size < 256.
 * @return	Address of the byte following the stored data in the
 * 		destination buffer
 */
uint8_t *cbi_set_data(uint8_t *p, enum cbi_data_tag tag,
		      const void *buf, int size);

/**
 * Store string data in memory in CBI data format.
 *
 * @param p	Pointer to the buffer where a new data item will be stored. It
 * 		should be pointing to the data section of CBI.
 * @param tag	Tag of the data item
 * @param str	Pointer to the string data being copied. If pointer is NULL,
 * 		this function will ignore adding the tag as well. Else, the
 * 		string data will be added to CBI using size of strlen + 1. This
 * 		string is assumed to be NUL-terminated and NUL gets stored in
 * 		CBI along with the string data.
 * @return	Address of the byte following the stored data in the destination
 * 		buffer.
 */
uint8_t *cbi_set_string(uint8_t *p, enum cbi_data_tag tag, const char *str);

/**
 * Find a data field in CBI
 *
 * @param cbi	Buffer containing CBI struct
 * @param tag	Tag of the data field to search
 * @return	Pointer to the data or NULL if not found.
 */
struct datablob_item *cbi_find_tag(const void *cbi, enum cbi_data_tag tag);

/**
 * Callback implemented by board to manipulate data
 *
 * Note that this is part of the APIs (cbi_get_*) which can be called in any
 * order any time. Your callback should return EC_SUCCESS only after it has all
 * the data needed for manipulation. Until then, it should return EC_ERROR_BUSY.
 * That'll provide a consistent view to the callers, which is critical for CBI
 * to be functional.
 *
 * @param tag	Tag of the data field to be manipulated
 * @param buf	Pointer to the buffer containing the data being manipulated.
 * @param size	size of the date in bytes
 * @return EC_SUCCESS to indicate the data is ready.
 *         EC_ERROR_BUSY to indicate supplemental data is not ready.
 */
int cbi_board_override(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size);

#ifdef TEST_BUILD
/**
 * Test only declarations. Don'ty use them in firmware.
 */
int cbi_create(void);
int cbi_write(void);
void cbi_invalidate_cache(void);
#endif

#endif /* __CROS_EC_CROS_BOARD_INFO_H */
