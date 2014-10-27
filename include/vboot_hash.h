/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Verified boot hashing memory module for Chrome EC */

#ifndef __CROS_EC_VBOOT_HASH_H
#define __CROS_EC_VBOOT_HASH_H

#include "common.h"

/**
 * Invalidate the hash if the hashed data overlaps the specified region.
 *
 * @param offset	Region start offset in flash
 * @param size		Size of region in bytes
 *
 * @return non-zero if the region overlapped the hashed region.
 */
int vboot_hash_invalidate(int offset, int size);

uint32_t vboot_hash_get_image_size(enum system_image_copy_t copy);


void vboot_hash_get_next_chunk_addr(
				const uint8_t *vbootbuf ,
				uint32_t offset,
				uint32_t nbytes);



#endif  /* __CROS_EC_VBOOT_HASH_H */
