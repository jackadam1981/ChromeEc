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

uint8_t get_current_pcr_digest(const uint8_t bitmask[2], uint8_t hash_size,
		uint8_t *sha256_of_selected_pcr)
{
	LITE_SHA256_CTX ctx;
	TPML_DIGEST digest;
	uint32_t pcr_counter;
	TPML_PCR_SELECTION selection;
	size_t x;
	size_t y;
	uint8_t digest_index;

	selection.count = 1;
	selection.pcrSelections[0].hash = TPM_ALG_SHA256;
	selection.pcrSelections[0].sizeofSelect = PCR_SELECT_MIN;
	memset(&selection.pcrSelections[0].pcrSelect, 0, PCR_SELECT_MIN);
	memcpy(&selection.pcrSelections[0].pcrSelect, bitmask, sizeof(bitmask));

	PCRRead(&selection, &digest, &pcr_counter);
	DCRYPTO_SHA256_init(&ctx, 0);
	/** For each bit present in bitmask, include the corresponding
	 *  PCR value in the digest.
	 */
	digest_index = 0;
	for (x = 0; x < 2; ++x) {
		if (selection.pcrSelections[0].pcrSelect[x] != bitmask[x])
			return 1;
		for (y = 0; y < 8; ++y) {
			if ((bitmask[x] & (1 << y)) != 0) {
				HASH_update(
					&ctx,
					digest.digests[digest_index].b.buffer,
					hash_size);
				digest_index++;
			}
		}
	}
	memcpy(sha256_of_selected_pcr, HASH_final(&ctx), hash_size);
	return 0;
}
