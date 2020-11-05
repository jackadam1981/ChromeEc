/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Utility functions for Chrome EC (including Zephyr build)*/

#include "common.h"
#include "console.h"
#include "util.h"


__stdlib_compat int isspace(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}


__stdlib_compat int isdigit(int c)
{
	return c >= '0' && c <= '9';
}


__stdlib_compat int isalpha(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

__stdlib_compat int isupper(int c)
{
	return c >= 'A' && c <= 'Z';
}

__stdlib_compat int isprint(int c)
{
	return c >= ' ' && c <= '~';
}

__stdlib_compat int tolower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}

__stdlib_compat int parse_bool(const char *s, int *dest)
{
	/* off, disable, false, no */
	if (!strcasecmp(s, "off") || !strncasecmp(s, "dis", 3) ||
	    tolower(*s) == 'f' || tolower(*s) == 'n') {
		*dest = 0;
		return 1;
	}

	/* on, enable, true, yes */
	if (!strcasecmp(s, "on") || !strncasecmp(s, "ena", 3) ||
	    tolower(*s) == 't' || tolower(*s) == 'y') {
		*dest = 1;
		return 1;
	}

	/* dunno */
	return 0;
}

/* Constant-time memory comparison */
int safe_memcmp(const void *s1, const void *s2, size_t size)
{
	const uint8_t *us1 = s1;
	const uint8_t *us2 = s2;
	int result = 0;

	if (size == 0)
		return 0;

	/*
	 * Code snippet without data-dependent branch due to Nate Lawson
	 * (nate@root.org) of Root Labs.
	 */
	while (size--)
		result |= *us1++ ^ *us2++;

	return result != 0;
}

void reverse(void *dest, size_t len)
{
	int i;
	uint8_t *start = dest;
	uint8_t *end = start + len;

	for (i = 0; i < len / 2; ++i) {
		uint8_t tmp = *start;

		*start++ = *--end;
		*end = tmp;
	}
}


__stdlib_compat char *strzcpy(char *dest, const char *src, int len)
{
	char *d = dest;
	if (len <= 0)
		return dest;
	while (len > 1 && *src) {
		*(d++) = *(src++);
		len--;
	}
	*d = '\0';
	return dest;
}

int uint64divmod(uint64_t *n, int d)
{
	uint64_t q = 0, mask;
	int r = 0;

	/* Divide-by-zero returns zero */
	if (!d) {
		*n = 0;
		return 0;
	}

	/* Common powers of 2 = simple shifts */
	if (d == 2) {
		r = *n & 1;
		*n >>= 1;
		return r;
	} else if (d == 16) {
		r = *n & 0xf;
		*n >>= 4;
		return r;
	}

	/* If v fits in 32-bit, we're done. */
	if (*n <= 0xffffffff) {
		uint32_t v32 = *n;
		r = v32 % d;
		*n = v32 / d;
		return r;
	}

	/* Otherwise do integer division the slow way. */
	for (mask = (1ULL << 63); mask; mask >>= 1) {
		r <<= 1;
		if (*n & mask)
			r |= 1;
		if (r >= d) {
			r -= d;
			q |= mask;
		}
	}
	*n = q;
	return r;
}

int get_next_bit(uint32_t *mask)
{
	int bit = 31 - __builtin_clz(*mask);
	*mask &= ~BIT(bit);
	return bit;
}

bool bytes_are_trivial(const uint8_t *buffer, size_t size)
{
	size_t i;
	uint8_t result0 = 0;
	uint8_t result1 = 0;

	for (i = 0; i < size; i++) {
		result0 |= buffer[i] ^ 0x00;
		result1 |= buffer[i] ^ 0xff;
	}
	return (result0 == 0) || (result1 == 0);
}

bool is_aligned(uint32_t addr, uint32_t align)
{
	if (!POWER_OF_TWO(align))
		return false;

	return (addr & (align - 1)) == 0;
}

/****************************************************************************/
/* stateful conditional stuff */

enum cond_internal_bits {
	COND_CURR_MASK = BIT(0),		/* current value */
	COND_RISE_MASK = BIT(1),		/* set if 0->1 */
	COND_FALL_MASK = BIT(2),		/* set if 1->0 */
};

void cond_init(cond_t *c, int val)
{
	if (val)
		*c = COND_CURR_MASK;
	else
		*c = 0;
}

int cond_is(cond_t *c, int val)
{
	if (val)
		return *c & COND_CURR_MASK;
	else
		return !(*c & COND_CURR_MASK);
}


void cond_set(cond_t *c, int val)
{
	if (val && cond_is(c, 0))
		*c |= COND_RISE_MASK;
	else if (!val && cond_is(c, 1))
		*c |= COND_FALL_MASK;
	if (val)
		*c |= COND_CURR_MASK;
	else
		*c &= ~COND_CURR_MASK;
}


int cond_went(cond_t *c, int val)
{
	int ret;

	if (val) {
		ret = *c & COND_RISE_MASK;
		*c &= ~COND_RISE_MASK;
	} else {
		ret = *c & COND_FALL_MASK;
		*c &= ~COND_FALL_MASK;
	}

	return ret;
}

/****************************************************************************/
/* console command parsing */

/**
 * Parse offset and size from command line argv[shift] and argv[shift+1]
 *
 * Default values: If argc<=shift, leaves offset unchanged, returning error if
 * *offset<0.  If argc<shift+1, leaves size unchanged, returning error if
 * *size<0.
 */
int parse_offset_size(int argc, char **argv, int shift,
			     int *offset, int *size)
{
	char *e;
	int i;

	if (argc > shift) {
		i = (uint32_t)strtoi(argv[shift], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		*offset = i;
	} else if (*offset < 0)
		return EC_ERROR_PARAM_COUNT;

	if (argc > shift + 1) {
		i = (uint32_t)strtoi(argv[shift + 1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
		*size = i;
	} else if (*size < 0)
		return EC_ERROR_PARAM_COUNT;

	return EC_SUCCESS;
}

void hexdump(const uint8_t *data, int len)
{
	int i, j;

	if (!data || !len)
		return;

	for (i = 0; i < len; i += 16) {
		/* Left column (Hex) */
		for (j = i; j < i + 16; j++) {
			if (j < len)
				ccprintf(" %02x", data[j]);
			else
				ccprintf("   ");
		}
		/* Right column (ASCII) */
		ccprintf(" |");
		for (j = i; j < i + 16; j++) {
			int c = j < len ? data[j] : ' ';
			ccprintf("%c", isprint(c) ? c : '.');
		}
		ccprintf("|\n");
	}
}

void wait_for_ready(volatile uint32_t *reg, uint32_t enable, uint32_t ready)
{
	if (*reg & ready)
		return;

	/* Enable */
	*reg |= enable;
	/* Wait for ready */
	while (!(*reg & ready))
		;
}
