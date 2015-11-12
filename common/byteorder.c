/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>

#if (__BYTE_ORDER__  == __ORDER_LITTLE_ENDIAN__)

/*
 * Should support for big endian targets become necessary, this file will have
 * to be extended to provide it.
 */
static inline void swap_n(void *in, void *out, size_t size)
{
	int i;

	for (i = 0; i < size; i++)
		((uint8_t *)out)[size - i - 1] = ((uint8_t *)in)[i];
}

uint16_t be16toh(uint16_t in)
{
	uint16_t out;

	swap_n(&in, &out, sizeof(out));
	return out;
}

uint32_t be32toh(uint32_t in)
{
	uint32_t out;

	swap_n(&in, &out, sizeof(out));
	return out;
}

uint64_t be64toh(uint64_t in)
{
	uint64_t out;

	swap_n(&in, &out, sizeof(out));
	return out;
}

#endif  /* __BYTE_ORDER__  == __ORDER_BIG_ENDIAN__ */
