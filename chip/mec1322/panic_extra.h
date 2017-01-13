/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Save the panic data to the backup address
 * Note: Use inline function because the backup offset may corrupt the stack.
 *       The function should be done stackless.
 *
 * @param offset	 The offset of the PANIC_DATA_PTR to the backup address.
 */
static inline void panic_data_backup(int offset)
{
	uint8_t *src_ptr = (uint8_t *)panic_get_data();
	uint8_t *dest_ptr;
	int num_bytes = sizeof(struct panic_data);

	dest_ptr = src_ptr - offset;
	if (src_ptr) {
		while (num_bytes--)
			*dest_ptr++ = *src_ptr++;
	}
}

/**
 * Restore the panic data from the backup address
 *
 * @param offset	 The offset of the PANIC_DATA_PTR to the backup address.
 */
static inline void panic_data_restore(int offset)
{
	uint8_t *src_ptr = ((uint8_t *)PANIC_DATA_PTR) - offset;
	uint8_t *dest_ptr = (uint8_t *)PANIC_DATA_PTR;
	int num_bytes = sizeof(struct panic_data);

	while (num_bytes--)
		*dest_ptr++ = *src_ptr++;
}

