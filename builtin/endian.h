/* Copyright 2016 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <endian.h>

uint16_t htobe16(uint16_t host_16bits);
uint16_t htole16(uint16_t host_16bits);
uint16_t be16toh(uint16_t big_endian_16bits);
uint16_t le16toh(uint16_t little_endian_16bits);

uint32_t htobe32(uint32_t host_32bits);
uint32_t htole32(uint32_t host_32bits);
uint32_t be32toh(uint32_t big_endian_32bits);
uint32_t le32toh(uint32_t little_endian_32bits);

uint64_t htobe64(uint64_t host_64bits);
uint64_t htole64(uint64_t host_64bits);
uint64_t be64toh(uint64_t big_endian_64bits);
uint64_t le64toh(uint64_t little_endian_64bits);
#ifndef __EC_BUILTIN_ENDIAN_H
#define __EC_BUILTIN_ENDIAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Functions to convert byte order in various sized big endian integers to
 * host byte order. Note that the code currently does not require functions
 * for converting little endian integers.
 */
#if (__BYTE_ORDER__  == __ORDER_LITTLE_ENDIAN__)

static inline uint16_t be16toh(uint16_t in)
{
	return __builtin_bswap16(in);
}
static inline uint32_t be32toh(uint32_t in)
{
	return __builtin_bswap32(in);
}
static inline uint64_t be64toh(uint64_t in)
{
	return __builtin_bswap64(in);
}

#endif  /* __BYTE_ORDER__  == __ORDER_LITTLE_ENDIAN__ */

#define htobe16 be16toh
#define htobe32 be32toh
#define htobe64 be64toh

#ifdef __cplusplus
}
#endif

#endif  /* __EC_BUILTIN_ENDIAN_H */
