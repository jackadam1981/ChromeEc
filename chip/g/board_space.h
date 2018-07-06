/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_CHIP_G_BOARD_SPACE_H
#define __EC_CHIP_G_BOARD_SPACE_H

#include "compile_time_macros.h"
#include "flash_info.h"
#include "stdint.h"

/*
 * Structures for data stored in the board space of INFO1.
 */

/* Structure holding Board ID */
struct board_id {
	uint32_t type;		/* Board type */
	uint32_t type_inv;	/* Board type (inverted) */
	uint32_t flags;		/* Flags */
};

/* Structure holding serial number data */
struct sn_data {
	uint8_t version;
	uint8_t reserved[2];
	uint8_t rma_status;
	uint32_t sn_hash[3];
};

/* Info1 Board space contents. */
struct info1_board_space {
	struct board_id bid;
	uint8_t bid_padding[4];
	struct sn_data sn;
};

#define INFO_BOARD_ID_SIZE		sizeof(struct board_id)
#define INFO_BOARD_ID_OFFSET		(INFO_BOARD_SPACE_OFFSET + \
					 offsetof(struct info1_board_space, \
						  bid))
#define INFO_BOARD_ID_PROTECT_SIZE	16

#define INFO_SN_DATA_SIZE		 sizeof(struct sn_data)
#define INFO_SN_DATA_OFFSET		 (INFO_BOARD_SPACE_OFFSET + \
					  offsetof(struct info1_board_space, \
						   sn))
#define INFO_SN_DATA_PROTECT_SIZE	 16

BUILD_ASSERT((INFO_BOARD_ID_SIZE & 3) == 0);
BUILD_ASSERT((INFO_BOARD_ID_OFFSET & 3) == 0);
BUILD_ASSERT(INFO_BOARD_ID_SIZE <= INFO_BOARD_ID_PROTECT_SIZE);

BUILD_ASSERT((INFO_SN_DATA_SIZE & 3) == 0);
BUILD_ASSERT((INFO_SN_DATA_OFFSET & 3) == 0);
BUILD_ASSERT(INFO_SN_DATA_SIZE <= INFO_SN_DATA_PROTECT_SIZE);

#endif  /* ! __EC_CHIP_G_BOARD_SPACE_H */
