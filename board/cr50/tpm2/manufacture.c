/* Copyright 2016 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "tpm_manufacture.h"

#include "Global.h"
#include "NV_fp.h"
#include "Platform.h"
#include "TPM_Types.h"
#include "TpmBuildSwitches.h"
#include "tpm_types.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

#define EK_CERT_NV_START_INDEX  0x01C00000

/**
 * b/262324344 debugging solution to detect when EPS is reset to zero.
 * tpm_registers.c can't include Implementation.h directly.
 * TODO (b/262324344): remove when solved.
 */
const uint16_t *GP_EPS_LEN = &gp.EPSeed.t.size;

uint16_t nv_eps_len(void)
{
	NV_RESERVED_ITEM ri;
	/*
	 * Make sure we read length of TPM2B struct properly. Note, it is
	 * stored in machine format, so no ending conversion is needed.
	 */
	uint16_t eps_seed_len = 0; /* Shall match TPM2B size type. */

	BUILD_ASSERT(sizeof(gp.EPSeed.t.size) == sizeof(eps_seed_len));
	NvGetReserved(NV_EP_SEED, &ri);
	_plat__NvMemoryRead(ri.offset, sizeof(eps_seed_len), &eps_seed_len);
	if (eps_seed_len > ri.size - sizeof(gp.EPSeed.t.size))
		eps_seed_len = 0;
	return eps_seed_len;
}

int tpm_manufactured(void)
{
	uint32_t nv_ram_index;
	const uint32_t rsa_ek_nv_index = EK_CERT_NV_START_INDEX;
	const uint32_t ecc_ek_nv_index = EK_CERT_NV_START_INDEX + 1;
	uint16_t eps_seed_len;

	/*
	 * If nvram_index (value written at NV RAM offset of zero) is all
	 * ones, or either endorsement certificate is not installed or EPS seed
	 * length is invalid, consider the chip un-manufactured.
	 *
	 * Thus, wiping flash NV ram allows to re-manufacture the chip.
	 */
	_plat__NvMemoryRead(0, sizeof(nv_ram_index), &nv_ram_index);
	eps_seed_len = nv_eps_len();

	if ((nv_ram_index == ~0) || (eps_seed_len < PRIMARY_SEED_SIZE) ||
	    (NvIsUndefinedIndex(rsa_ek_nv_index) == TPM_RC_SUCCESS) ||
	    (NvIsUndefinedIndex(ecc_ek_nv_index) == TPM_RC_SUCCESS)) {
		CPRINTF("%s: NOT manufactured\n", __func__);
		return 0;
	}

	CPRINTF("%s: manufactured\n", __func__);
	cflush();
	return 1;
}
