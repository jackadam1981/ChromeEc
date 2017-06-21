/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Base-32 encoding/decoding */

#include "common.h"
#include "base32.h"
#include "util.h"

uint8_t crc5_sym(int sym, uint8_t previous_crc)
{
	unsigned crc = previous_crc << 8;
	int i;

	/*
	 * This is a modified CRC-8 which only folds in a 5-bit
	 * symbol, and it only keeps the bottom 5 bits of the CRC.
	 */
	crc ^= (sym << 11);
	for (i = 5; i; i--) {
		if (crc & 0x8000)
			crc ^= (0x1070 << 3);
		crc <<= 1;
	}
	return (uint8_t)((crc >> 8) & 0x1f);
}

/* A-Z0-9 with I,O,0,1 removed */
const char base32_map[33] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

/**
 * Decode a base32 symbol.
 *
 * @param sym Input symbol
 * @return The symbol value or -1 if error.
 */
static int decode_sym(int sym)
{
	int i = 0;

	for (i = 0; i < 32; i++) {
		if (sym == base32_map[i])
			return i;
	}

	return -1;
}

int base32_encode(char *dest, int destlen_chars,
		  const void *srcbits, int srclen_bits,
		  int add_crc_every)
{
	const uint8_t *src = srcbits;
	int destlen_needed;
	int crc = 0, crc_count = 0;
	int didx = 0;
	int i;

	/* Make sure we can at least store the terminating null */
	if (destlen_chars < 1)
		return EC_ERROR_INVAL;
	*dest = 0;

	/* Make sure destination is big enough */
	destlen_needed = (srclen_bits + 4) / 5;
	if (add_crc_every) {
		/* Must be an exact number of groups to add CRC */
		if (destlen_needed % add_crc_every)
			return EC_ERROR_INVAL;
		destlen_needed += destlen_needed / add_crc_every;
	}
	destlen_needed++;  /* For terminating null */
	if (destlen_chars < destlen_needed)
		return EC_ERROR_INVAL;

	for (i = 0; i < srclen_bits; i += 5) {
		int sym;
		int sidx = i / 8;
		int bit_offs = i % 8;

		if (bit_offs <= 3) {
			/* Entire symbol fits in that byte */
			sym = src[sidx] >> (3 - bit_offs);
		} else {
			/* Use the bits we have left */
			sym = src[sidx] << (bit_offs - 3);

			/* Use the bits from the next byte, if any */
			if (i + 1 < srclen_bits)
				sym |= src[sidx + 1] >> (11 - bit_offs);
		}

		sym &= 0x1f;

		/* Pad incomplete symbol with 0 bits */
		if (srclen_bits - i < 5)
			sym &= 0x1f << (5 + i - srclen_bits);

		dest[didx++] = base32_map[sym];

		/* Add CRC if needed */
		if (add_crc_every) {
			crc = crc5_sym(sym, crc);
			if (++crc_count == add_crc_every) {
				dest[didx++] = base32_map[crc];
				crc_count = crc = 0;
			}
		}
	}

	/* Terminate string and return */
	dest[didx] = 0;
	return EC_SUCCESS;
}

/**
 * base32-decode data from a null-terminated string
 *
 * Ignores whitespace in the source string.
 *
 * Since encode may pad the last symbol with up to 4 0-bits, it is not an error
 * if the destination is smaller than the decoded string by up to 4 0-bits.  If
 * the destination is sufficiently small that an entire input symbol would be
 * dropped, though, that's an error.
 *
 * @param dest			Destination
 * @param destlen_bits		Maximum length to decode *in bits*
 * @param src			Source string
 * @param crc_after_every	If non-zero, expect CRC symbol after every
 *				group of this many symbols.
 * @return Number of decoded *bits*, or -1 if error.
 */
int base32_decode(uint8_t *dest, int destlen_bits, const char *src,
		  int crc_after_every)
{
	int crc = 0, crc_count = 0;
	int didx = 0, out_bits = 0;
	int queue = 0, qbits = 0;

	for (; *src; src++) {
		int sym, dbits;

		if (isspace(*src))
			continue;

		sym = decode_sym(*src);
		if (sym < 0)
			return -1;  /* Bad input symbol */

		/* Check CRC if needed */
		if (crc_after_every) {
			if (crc_count == crc_after_every) {
				if (crc != sym)
					return -1;
				crc_count = crc = 0;
				continue;
			} else {
				crc = crc5_sym(sym, crc);
				crc_count++;
			}
		}

		/*
		 * Stop if we're out of space.  Have to do this after checking
		 * the CRC, or we might not check the last CRC.
		 */
		if (out_bits >= destlen_bits)
			break;

		/* Add symbol to the queue */
		queue = (queue << 5) | sym;
		qbits += 5;

		/* Pull off output bytes */
		dbits = MIN(8, destlen_bits - out_bits);
		while (qbits >= dbits && dbits > 0) {
			dest[didx++] = (queue >> (qbits - dbits))
					<< (8 - dbits);
			qbits -= dbits;
			queue &= 0xff >> (8 - qbits);
			out_bits += dbits;
			dbits = MIN(8, destlen_bits - out_bits);
		}
	}

	/*
	 * Consume any bits left from the last source symbol.  This happens
	 * if the destination is bigger than the source, and the decoded data
	 * is not an even multiple of bytes.
	 */
	if (qbits > 0 && out_bits < destlen_bits) {
		int dbits = MIN(qbits, destlen_bits - out_bits);

		dest[didx++] = (queue >> (qbits - dbits)) << (8 - dbits);
		qbits -= dbits;
		queue &= 0xff >> (8 - qbits);
		out_bits += dbits;
	}

	/* If we have CRCs, should have a full group */
	if (crc_after_every && crc_count)
		return -1;

	return out_bits;
}
