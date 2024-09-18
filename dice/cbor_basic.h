/*
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __GSC_UTILS_DICE_CBOR_BASIC_H
#define __GSC_UTILS_DICE_CBOR_BASIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	kCborUint = (0 << 5),
	kCborNint = (1 << 5),
	kCborBstr = (2 << 5),
	kCborTstr = (3 << 5),
	kCborArr = (4 << 5),
	kCborMap = (5 << 5),
	kCborTag = (6 << 5),
	kCborSimple = (7 << 5)
} cbor_major_type_t;

#define CBOR_HDR1(major, value) ((uint8_t)(major) | (uint8_t)((value) & 0x1f))

#define CBOR_FALSE CBOR_HDR1(kCborSimple, 20)
#define CBOR_TRUE  CBOR_HDR1(kCborSimple, 21)
#define CBOR_NULL  CBOR_HDR1(kCborSimple, 22)

#define CBOR_BYTES1 24
#define CBOR_BYTES2 25
#define CBOR_BYTES4 26
#define CBOR_BYTES8 27

/* NINTs in [-24..-1] range ("0 bytes") */
#define CBOR_NINT0_LEN	  1
#define CBOR_NINT0(label) CBOR_HDR1(kCborNint, -(label) - 1)

/* UINTs in [0..23] range ("0 bytes") */
#define CBOR_UINT0_LEN	  1
#define CBOR_UINT0(label) CBOR_HDR1(kCborUint, (label))

/* Building block for 32 bit (4 bytes) integer representations */
#define CBOR_INT32_LEN (1 + 4)
#define CBOR_INT32(major, value)                                 \
	{                                                        \
		CBOR_HDR1(major, CBOR_BYTES4),                   \
			(uint8_t)(((value) & 0xFF000000) >> 24), \
			(uint8_t)(((value) & 0x00FF0000) >> 16), \
			(uint8_t)(((value) & 0x0000FF00) >> 8),  \
			(uint8_t)((value) & 0x000000FF)          \
	}

/* 32 bit (4 bytes) negative integers */
#define CBOR_NINT32_LEN	   CBOR_INT32_LEN
#define CBOR_NINT32(value) CBOR_INT32(kCborNint, (-(value) - 1))

/* 32 bit (4 bytes) positive integers */
#define CBOR_UINT32_LEN	   CBOR_INT32_LEN
#define CBOR_UINT32(value) CBOR_INT32(kCborUint, value)

/* BSTR with 1 byte size */
#define CBOR_BSTR_HDR8(size)                            \
	{                                               \
		CBOR_HDR1(kCborBstr, CBOR_BYTES1), size \
	}

/* BSTR with 2 byte size */
#define CBOR_BSTR_HDR16(size)                              \
	{                                                  \
		CBOR_HDR1(kCborBstr, CBOR_BYTES2),         \
			(uint8_t)(((size) & 0xFF00) >> 8), \
			(uint8_t)((size) & 0x00FF)         \
	}

/* BSTR of length 1 */
typedef struct {
	uint8_t cbor_hdr;
	uint8_t value;
} cbor_bstr1_t;
#define CBOR_BSTR1_HDR CBOR_HDR1(kCborBstr, 1)
#define CBOR_BSTR1_EMPTY          \
	{                         \
		CBOR_BSTR1_HDR, 0 \
	}

/* BSTR of length 32: UDS, CDI, digest */
typedef struct {
	uint8_t cbor_hdr[2];
	uint8_t value[32];
} cbor_bstr32_t;
#define CBOR_BSTR32_HDR CBOR_BSTR_HDR8(32)
#define CBOR_BSTR32_EMPTY                                                     \
	{                                                                     \
		CBOR_BSTR32_HDR,                                              \
		{                                                             \
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0      \
		}                                                             \
	}

/* TSTR of length 2*20 = 40: UDS_ID, CDI_ID as hex */
typedef struct {
	uint8_t cbor_hdr[2];
	uint8_t value[40];
} cbor_tstr40_t;
#define CBOR_TSTR40_HDR CBOR_BSTR_HDR8(40)
#define CBOR_TSTR40_EMPTY                                                     \
	{                                                                     \
		CBOR_TSTR40_HDR,                                              \
		{                                                             \
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  \
				0, 0, 0, 0, 0, 0, 0                           \
		}                                                             \
	}

/* BSTR of length 64: signature */
typedef struct {
	uint8_t cbor_hdr[2];
	uint8_t value[64];
} cbor_bstr64_t;
#define CBOR_BSTR64_HDR CBOR_BSTR_HDR8(64)
#define CBOR_BSTR64_EMPTY                                                      \
	{                                                                      \
		CBOR_BSTR64_HDR,                                               \
		{                                                              \
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  \
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   \
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   \
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 \
		}                                                              \
	}

/* UINT32 */
typedef struct {
	uint8_t cbor_hdr;
	uint8_t value[4];
} cbor_uint32_t;
#define CBOR_UINT32_HDR CBOR_HDR1(kCborUint, CBOR_BYTES4)
#define CBOR_UINT32_ZERO           \
	{                          \
		CBOR_UINT32_HDR,   \
		{                  \
			0, 0, 0, 0 \
		}                  \
	}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __CGSC_UTILS_DICE_CBOR_BASIC_H */
