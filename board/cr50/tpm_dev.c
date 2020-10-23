/*
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRYPTO_TEST_SETUP

#define PCR_C
#include "InternalRoutines.h"
#include "NV_fp.h"
#include "PCR_fp.h"
#include "PCR_Read_fp.h"
#include "PCR_Extend_fp.h"
#include "Startup_fp.h"

#include "common.h"
#include "console.h"
#include "dcrypto.h"

/*
 * Input values for PCR0 extend.
 * Taken from platform/vboot_reference/firmware/2lib/2tpm_bootmode.c
 * These are calculated as:
 *    SHA1("|Developer_Mode||Recovery_Mode||Keyblock_Mode|").
 * Developer_Mode can be 0 or 1.
 * Recovery_Mode can be 0 or 1.
 * Keyblock flags are defined in 2struct.h and assumed always 0 in recovery mode
 * or 7 in non-recovery mode.
 *
 * We map them to Keyblock_Mode as follows:
 *   -----------------------------------------
 *   Keyblock Flags            | Keyblock Mode
 *   -----------------------------------------
 *   0 recovery mode           |     0
 *   7 Normal-signed firmware  |     1
 */

const uint8_t pcr0_values[4][SHA256_DIGEST_SIZE] = {
	/* SHA1(0x00|0x00|0x01) + 00s to SHA256 size */
	{0x25, 0x47, 0xcc, 0x73, 0x6e, 0x95, 0x1f, 0xa4,
	 0x91, 0x98, 0x53, 0xc4, 0x3a, 0xe8, 0x90, 0x86,
	 0x1a, 0x3b, 0x32, 0x64, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

	/* SHA1(0x01|0x00|0x01) + 00s to SHA256 size */
	{0xc4, 0x2a, 0xc1, 0xc4, 0x6f, 0x1d, 0x4e, 0x21,
	 0x1c, 0x73, 0x5c, 0xc7, 0xdf, 0xad, 0x4f, 0xf8,
	 0x39, 0x11, 0x10, 0xe9, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

	/* SHA1(0x00|0x01|0x00) + 00s to SHA256 size */
	{0x62, 0x57, 0x18, 0x91, 0x21, 0x5b, 0x4e, 0xfc,
	 0x1c, 0xea, 0xb7, 0x44, 0xce, 0x59, 0xdd, 0x0b,
	 0x66, 0xea, 0x6f, 0x73, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},

	/* SHA1(0x01|0x01|0x00) + 00s to SHA256 size */
	{0x47, 0xec, 0x8d, 0x98, 0x36, 0x64, 0x33, 0xdc,
	 0x00, 0x2e, 0x77, 0x21, 0xc9, 0xe3, 0x7d, 0x50,
	 0x67, 0x54, 0x79, 0x37, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

const uint8_t zero_value[SHA256_DIGEST_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


const char *pcr0_descr[4] = {
	"(rec=0, dev=0)",
	"(rec=0, dev=1)",
	"(rec=1, dev=0)",
	"(rec=1, dev=1)",
};

const uint8_t *boot_state_value(bool recovery_mode, bool dev_mode)
{
	int index = (recovery_mode ? 2 : 0) + (dev_mode ? 1 : 0);

	return pcr0_values[index];
}

const uint8_t *pcr0_digest(const uint8_t *extend_value)
{
	union hash_ctx ctx;

	if (DCRYPTO_hw_hash_init(&ctx, TPM_ALG_SHA256) != DCRYPTO_OK)
		return NULL;
	HASH_update(&ctx, zero_value, SHA256_DIGEST_SIZE);
	HASH_update(&ctx, extend_value, SHA256_DIGEST_SIZE);
	return (const uint8_t *)HASH_final(&ctx);
}

bool pcr0_reset(void)
{
	BYTE *pcrData = s_pcrs[0].sha256Pcr;

	memset(pcrData, 0, SHA256_DIGEST_SIZE);
	return true;
}

bool pcr0_extend(const uint8_t *extend_value)
{
	static PCR_Extend_In in;
	TPM_RC res = 0;

	memset(&in, 0, sizeof(in));

	in.pcrHandle = PCR_FIRST;
	in.digests.count = 1;
	in.digests.digests[0].hashAlg = TPM_ALG_SHA256;
	memcpy(&in.digests.digests[0].digest, extend_value, SHA256_DIGEST_SIZE);

	res = TPM2_PCR_Extend(&in);
	if (res != 0)
		ccprintf("TPM2_PCR_Extend returned %08x\n", res);
	return res == 0;
}

uint8_t *pcr0_read(void)
{
	static PCR_Read_In in;

	static PCR_Read_Out out;

	TPM_RC res;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.pcrSelectionIn.count = 1;
	in.pcrSelectionIn.pcrSelections[0].hash = TPM_ALG_SHA256;
	in.pcrSelectionIn.pcrSelections[0].sizeofSelect = PCR_SELECT_MAX;
	in.pcrSelectionIn.pcrSelections[0].pcrSelect[0] = 1;
	res = TPM2_PCR_Read(&in, &out);
	if (res != 0)
		ccprintf("TPM2_PCR_Read returned %08x\n", res);
	return out.pcrValues.digests[0].t.buffer;
}

void pcr0_dump(void)
{
	uint8_t *value = pcr0_read();

	ccprintf("PCR0: ");
	for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
		if (i == 16)
			ccprintf("\n      ");
		ccprintf("%02X ", value[i]);
	}
	ccprintf("\n");

	for (int n = 0; n < 4; n++) {
		const uint8_t *digest = pcr0_digest(pcr0_values[n]);

		if (!memcmp(value, digest, SHA256_DIGEST_SIZE)) {
			ccprintf("      %s\n", pcr0_descr[n]);
			break;
		}
	}
}

void pcr0_list(void)
{
	for (int n = 0; n < 4; n++) {
		const uint8_t *digest = pcr0_digest(pcr0_values[n]);

		ccprintf("%s:\n", pcr0_descr[n]);
		for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
			if (i == 16)
				ccprintf("\n");
			ccprintf("%02X ", digest[i]);
		}
		ccprintf("\n\n");
	}
}

void ph_control(int enable)
{
	g_phEnable = enable;
}

void ph_dump(void)
{
	ccprintf("PH = %d\n", g_phEnable);
}

void delete_fwmp(void)
{
	delete_tpm_nvmem(0x100a);
}

void delete_tpm_nvmem(uint16_t obj_index)
{
	NvDeleteEntity(HR_NV_INDEX + obj_index);
	NvCommit();
}

void call_startup(void)
{
	Startup_In in = { 0 };
	TPM_RC res;

	res = TPM2_Startup(&in);
	if (res != 0)
		ccprintf("TPM2_Startup returned %08x\n", res);
}

#endif /* !CRYPTO_TEST_SETUP*/
