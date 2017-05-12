/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_CHIP_G_FLASH_INFO_H
#define __EC_CHIP_G_FLASH_INFO_H

#include <stddef.h>

#include "signed_header.h"

/*
 * Info1 space available to the app firmware is split in several areas. Of
 * interest are the two spaces used for rollback prevention of RO and RW image
 * versions.
 *
 * Each bit in the image infomap header section is mapped into a 4 byte word
 * in the Info1 space.
 */
#define INFO_RO_MAP_OFFSET 0
#define INFO_RO_MAP_SIZE   (INFO_MAX * 4)
#define INFO_RW_MAP_OFFSET INFO_RO_MAP_SIZE
#define INFO_RW_MAP_SIZE   (INFO_MAX * 4)

/* Info1 space holds the Board ID */
#define INFO_BOARD_ID_OFFSET		0x400
#define INFO_BOARD_ID_SIZE		12
#define INFO_BOARD_ID_PROTECT_SIZE	16  /* Must be power of 2 */
/* Board ID has 3 words of fields - type, type (bit-inverted), and flags */
#define INFO_BOARD_ID_TYPE_OFFSET	(INFO_BOARD_ID_OFFSET + 0)
#define INFO_BOARD_ID_TYPE_INV_OFFSET	(INFO_BOARD_ID_OFFSET + 4)
#define INFO_BOARD_ID_FLAGS_OFFSET	(INFO_BOARD_ID_OFFSET + 8)

int flash_info_read_enable(uint32_t offset, size_t size);
/* This in fact enables both read and write. */
int flash_info_write_enable(uint32_t offset, size_t size);
void flash_info_write_disable(void);
int flash_info_physical_write(int byte_offset, int num_bytes, const char *data);
int flash_physical_info_read_word(int byte_offset, uint32_t *dst);

void flash_open_ro_window(uint32_t offset, size_t size_b);

#endif  /* ! __EC_CHIP_G_FLASH_INFO_H */
