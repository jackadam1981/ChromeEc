/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "flash.h"

int app_cipher(uint32_t salt, uint8_t *out, const uint8_t *in, size_t len)
{
	struct APPKEY_CTX ctx;
	uint32_t iv[4] = {0, 0, salt, 0};

	if (!DCRYPTO_appkey_init(NVMEM, &ctx))
		return 0;

	if (!DCRYPTO_aes_ctr(out, ctx.key, 128, (uint8_t*) iv, in, len))
		return 0;

	DCRYPTO_appkey_finish(&ctx);
	return 1;
}
