/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "debug_printf.h"
#include "registers.h"
#include "setup.h"

static void _sha_write(const void* data, size_t n)
{
	const uint8_t* bp = (const uint8_t*)data;
	const uint32_t* wp;

	while (n && ((uint32_t)bp & 3)) {  /* Feed unaligned start bytes. */
		*((uint8_t *)GREG32_ADDR(KEYMGR, SHA_INPUT_FIFO)) = *bp++;
		n -= 1;
	}

	wp = (uint32_t*)bp;
	while (n >= 32) { /* Feed groups of aligned words. */
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		n -= 32;
	}

	while (n >= 4) {  /* Feed individual aligned words. */
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		n -= 4;
	}

	bp = (uint8_t*)wp;
	while (n) {  /* Feed remaing bytes. */
		*((uint8_t *)GREG32_ADDR(KEYMGR, SHA_INPUT_FIFO))  = *bp++;
		n -= 1;
	}
}

static void _sha_wait(uint32_t* digest)
{
	int i;

	/*
	 * Wait for result. TODO: what harm does glitching do? Read out
	 * non-digest? Old digest?
	 */
	while (!GREG32(KEYMGR, SHA_ITOP))
		;

	/* Read out final digest. */
	   for (i = 0; i < 8; ++i) {
		   *digest++ = GREG32_ADDR(KEYMGR, SHA_STS_H0)[i];
	   }

	   GREG32(KEYMGR, SHA_ITOP) = 0;  /* Clear status. */
}

void hwSHA256(const void* data, size_t n, uint32_t* digest) {
	GREG32(KEYMGR, SHA_ITOP) = 0;  /* Clear status. */

	GREG32(KEYMGR, SHA_CFG_MSGLEN_LO) =  n;
	GREG32(KEYMGR, SHA_CFG_MSGLEN_HI) = 0;

	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, INT_EN_DONE, 1);
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_GO, 1);

	_sha_write(data, n);
	_sha_wait(digest);
}

void hwKeyLadderStep(uint32_t cert, const uint32_t* input)
{
	uint32_t flags;

	GREG32(KEYMGR, SHA_ITOP) = 0;  /* clear status. */

	debug_printf("Cert %2u: ", cert);

	GWRITE_FIELD(KEYMGR, SHA_USE_CERT, INDEX, cert);
	GWRITE_FIELD(KEYMGR, SHA_USE_CERT, ENABLE, 1);
	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, INT_EN_DONE, 1);
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_GO, 1);

	if (input) {
		int i;

		for (i = 0; i < 8; ++i)
			GREG32(KEYMGR, SHA_INPUT_FIFO) = *input++;

		GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_STOP, 1);
	}

	while (!GREG32(KEYMGR, SHA_ITOP))
	       ;

	GREG32(KEYMGR, SHA_ITOP) = 0;  /* Clear status. */

	flags = GREG32(KEYMGR, HKEY_ERR_FLAGS);
	if (flags)
		debug_printf("Cert %2u: fail %x\n", cert, flags);
	else
		debug_printf("flags %x\n", flags);
}

#if 0
void hwSHA256_init(hwSHA256_CTX* ctx) {
  GREG32(KEYMGR, SHA_ITOP_OFFSET, 0);  // clear status

  GREG32(KEYMGR, SHA_CFG_EN_OFFSET,
            KEYMGR_SHA_CFG_EN_LIVESTREAM_MASK | KEYMGR_SHA_CFG_EN_INT_EN_DONE_MASK);

  GREG32(KEYMGR, SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_GO_MASK);
}

void hwSHA256_update(hwSHA256_CTX* ctx, const void* data, size_t n) {
  _sha_write(data, n);
}

const uint8_t* hwSHA256_final(hwSHA256_CTX* ctx) {
  GREG32(KEYMGR, SHA_TRIG_OFFSET, KEYMGR_SHA_TRIG_TRIG_STOP_MASK);

  _sha_wait(ctx->digest);

  return (const uint8_t*)(ctx->digest);
}
#endif

