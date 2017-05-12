/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_BOARD_ID__H
#define __EC_BOARD_CR50_BOARD_ID__H

#include "common.h"

/* Structure holding Board ID */
struct board_id {
	uint32_t type;		/* Board type */
	uint32_t type_inv;	/* Board type (inverted) */
	uint32_t flags;		/* Flags */
};

/**
 * Read the current board ID
 *
 * @param id		Destination for Board ID
 *
 * @return EC_SUCCESS, or non-zero error code.
 */
int read_board_id(struct board_id *id);

/**
 * Write the board ID to INFO1
 *
 * @param id		Board ID to write
 *
 * @return EC_SUCCESS, or non-zero error code.
 */
int write_board_id(const struct board_id *id);

/**
 * Check BoardID locking
 *
 * @return EC_SUCCESS, or non-zero error code.
 */
int check_board_id(void);

#endif  /* ! __EC_BOARD_CR50_BOARD_ID_H */
