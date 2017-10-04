/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cros Board Info
 */
#ifndef __CROS_EC_CBI_H
#define __CROS_EC_CBI_H

#include "common.h"

#define CBI_VERSION_MAJOR 0
#define CBI_VERSION_MINOR 0
static const uint8_t cbi_magic[] = { 0x43, 0x42, 0x49 };

struct cbi_header {
	uint8_t magic[3];
	uint8_t crc;
	union {
		struct {
			uint8_t minor;
			uint8_t major;
		};
		uint16_t version;
	};
	uint16_t total_size;
} __attribute__((packed));

struct board_info {
	struct cbi_header head;
	union {
		struct {
			uint8_t minor;
			uint8_t major;
		};
		uint16_t version;
	};
	uint8_t oem_id;
	uint8_t sku_id;
} __attribute__((packed));

int cbi_get_board_version(void);
int cbi_get_sku_id(void);
int cbi_get_oem_id(void);

#endif /* __CROS_EC_CBI_H */
