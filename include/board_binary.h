/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_BOARD_BINARY_H
#define __CROS_EC_BOARD_BINARY_H

#define BLOB_TYPE(n)    ((n) & 0xff)

/**
 * enum of structure types recognized by bb_lookup
 */
enum blob_struct_type {BATTERY_INFO = 0x01, FAST_CHARGE_PARMS = 0x02};

/**
 * Search a board binary for a structure
 *
 * @param type  The type of structure to search for.
 * @return void ptr to structure if found, else NULL
 */
void *bb_lookup(enum blob_struct_type type);

#endif  /* __CROS_EC_BOARD_BINARY_H */

