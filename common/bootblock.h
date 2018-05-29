/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_BOOTBLOCK_H
#define __CROS_EC_BOOTBLOCK_H

#include <stdint.h>

/* Returns the bootblock data */
const uint8_t *bootblock_get_data(void);

/* Returns the bootblock data size */
int bootblock_get_size(void);

#endif /* __CROS_EC_BOOTBLOCK_H */
