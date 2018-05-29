/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Packs bootblock into binary, one can access it with bootblock_start and
 * bootblock_end.
 */

#ifndef __CROS_EC_BOOTBLOCK_H
#define __CROS_EC_BOOTBLOCK_H

/* The address of bootblock_start is the first byte of the bootblock. */
extern const uint8_t bootblock_start;
extern const uint8_t bootblock_end;

#endif
