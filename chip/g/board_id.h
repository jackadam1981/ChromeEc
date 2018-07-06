/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_CHIP_G_BOARD_ID_H
#define __EC_CHIP_G_BOARD_ID_H

#include "common.h"
#include "signed_header.h"
#include "util.h"

/* Structure holding Board ID */
struct board_id {
	uint32_t type;		/* Board type */
	uint32_t type_inv;	/* Board type (inverted) */
	uint32_t flags;		/* Flags */
};

/* Structure holding serial number bits */
struct sn_bits {
	uint32_t sn[2];
};

/* Info1 Board space contents. */
struct info1_board_space {
	struct board_id bid;
	uint32_t padding;
	struct sn_bits sn;
};

#define INFO_BOARD_ID_SIZE			sizeof(struct board_id)
#define INFO_BOARD_SPACE_PROTECT_SIZE		16
#define INFO_SN_BITS_SIZE			sizeof(struct sn_bits)
#define INFO_BOARD_SPACE_SN_OFFSET		(INFO_BOARD_SPACE_OFFSET + \
						 offsetof( \
						 struct info1_board_space, sn))
#define INFO_BOARD_SPACE_SN_PROTECT_SIZE	8

/**
 * Check the current header vs. the supplied Board ID
 *
 * @param board_id	Pointer to a Board ID structure to check
 * @param h		Pointer to the currently running image's header
 *
 * @return 0 if no mismatch, non-zero if mismatch
 */
uint32_t check_board_id_vs_header(const struct board_id *id,
				  const struct SignedHeader *h);

/**
 * Check board ID from the flash INFO1 space.
 *
 * @param id	Pointer to a Board ID structure to fill
 *
 * @return EC_SUCCESS of an error code in cases of vairous failures to read.
 */
int read_board_id(struct board_id *id);

/**
 * Return the image header for the current image copy
 */
const struct SignedHeader *get_current_image_header(void);

/**
 * Check if board ID in the image matches board ID field in the INFO1.
 *
 * Pass the pointer to the image header to check. If the pointer is set to
 * NULL, check board ID against the currently running image's header.
 *
 * Return true if there is a mismatch (the code should not run).
 */
uint32_t board_id_mismatch(const struct SignedHeader *h);

BUILD_ASSERT((offsetof(struct info1_board_space, bid) & 3) == 0);
BUILD_ASSERT((INFO_BOARD_ID_SIZE & 3) == 0);
BUILD_ASSERT(INFO_BOARD_ID_SIZE <= INFO_BOARD_SPACE_PROTECT_SIZE);
BUILD_ASSERT((offsetof(struct info1_board_space, sn) & 3) == 0);
BUILD_ASSERT((INFO_SN_BITS_SIZE & 3) == 0);
BUILD_ASSERT(INFO_SN_BITS_SIZE <= INFO_BOARD_SPACE_SN_PROTECT_SIZE);

#endif  /* ! __EC_CHIP_G_BOARD_ID_H */
