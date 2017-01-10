/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "flash.h"

int flash_cipher(uint8_t *out, const uint8_t *in, size_t len)
{
	struct APPKEY_CTX ctx;
	size_t index = 0;   /* Input implicitly starts at block index 0. */

	if (!DCRYPTO_appkey_init(NVMEM, &ctx))
		return 0;

	while (len >= 16) {
		DCRYPTO_appkey_cipher_block(&ctx, out, in, index);
		out += 16;
		in += 16;
		index++;
	}

	if (len) {
		uint8_t in_block[16];
		uint8_t out_block[16];

		memcpy(in_block, in, len);
		memset(in_block + len, 0, 16 - len);
		if (!DCRYPTO_appkey_cipher_block(
				&ctx, out_block, in_block, index)) {
			DCRYPTO_appkey_finish(&ctx);
			return 0;
		}
		memcpy(out, out_block, len);
	}

	DCRYPTO_appkey_finish(&ctx);
	return 1;
}
