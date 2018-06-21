/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pinweaver_tpm_imports.h>

#include <Global.h>
#include <InternalRoutines.h>
#include <dcrypto.h>
#include <util.h>

void get_storage_seed(void *buf, size_t *len)
{
	*len = MIN(*len, sizeof(gp.SPSeed));
	memcpy(buf, &gp.SPSeed, *len);
}

void get_current_pcr_digest(uint16_t bitmask, uint8_t hash_size,
		uint8_t *sha256_of_selected_pcr)
{
	LITE_SHA256_CTX ctx;
	TPML_DIGEST digest;
	uint32_t pcr_counter;
	TPML_PCR_SELECTION selection;
	size_t y;
	uint8_t digest_index;

	selection.count = 1;
	selection.pcrSelections[0].hash = TPM_ALG_SHA256;
	selection.pcrSelections[0].sizeofSelect = sizeof(bitmask);
	for (y = 0; y < selection.pcrSelections[0].sizeofSelect; ++y)
		selection.pcrSelections[0].pcrSelect[y] = (bitmask >> (y * 8));

	PCRRead(&selection, &digest, &pcr_counter);
	DCRYPTO_SHA256_init(&ctx, 0);
	/** For each bit present in bitmask, include the corresponding
	 *  PCR value in the digest.
	 */
	digest_index = 0;
	for (y = 0; y < 8 * sizeof(bitmask); ++y) {
		if ((bitmask & (1 << y)) != 0) {
			HASH_update(
				&ctx,
				digest.digests[digest_index].b.buffer,
				hash_size);
			digest_index++;
		}
	}
	memcpy(sha256_of_selected_pcr, HASH_final(&ctx), hash_size);
}
