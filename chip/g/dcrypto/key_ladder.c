/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h" /*TEMP DEBUG*/
#include "dcrypto.h"
#include "internal.h"
#include "endian.h"
#include "registers.h"
#include "trng.h"

static void ladder_init(void)
{
	/* Do not reset keyladder engine here, as before.
	 *
	 * Should not be needed and if it is, it is indicative
	 * of sync error between this and sha engine usage.
	 * Reset will make this flow work, but will have broken
	 * the other pending sha flow.
	 * Hence leave as is and observe the error.
	 */

	/* Enable random stalls for key-ladder usage.  Note that
	 * the stall rate used for key-ladder operations is
	 * 25% (vs. 12% for generic SHA operations).  This distinction
	 * is made so as to increase the difficulty in characterizng
	 * the key-ladder engine via random inputs provided over the
	 * generic SHA interface.
	 */
	/* Turn off random nops (which are enabled by default). */
	GWRITE_FIELD(KEYMGR, SHA_RAND_STALL_CTL, STALL_EN, 0);
	/* Configure random nop percentage at 25%. */
	GWRITE_FIELD(KEYMGR, SHA_RAND_STALL_CTL, FREQ, 1);
	/* Now turn on random nops. */
	GWRITE_FIELD(KEYMGR, SHA_RAND_STALL_CTL, STALL_EN, 1);
}

static int ladder_step(uint32_t cert, const uint32_t input[8])
{
	ccprintf("[STEP%d]", cert);
	GREG32(KEYMGR, SHA_ITOP) = 0;  /* clear status */

	GREG32(KEYMGR, SHA_USE_CERT_INDEX) =
		(cert << GC_KEYMGR_SHA_USE_CERT_INDEX_LSB) |
		GC_KEYMGR_SHA_USE_CERT_ENABLE_MASK;

	GREG32(KEYMGR, SHA_CFG_EN) =
		GC_KEYMGR_SHA_CFG_EN_INT_EN_DONE_MASK;
	GREG32(KEYMGR, SHA_TRIG) =
		GC_KEYMGR_SHA_TRIG_TRIG_GO_MASK;

	if (input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[0];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[1];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[2];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[3];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[4];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[5];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[6];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[7];

		GREG32(KEYMGR, SHA_TRIG) = GC_KEYMGR_SHA_TRIG_TRIG_STOP_MASK;
	}

	while (!GREG32(KEYMGR, SHA_ITOP))
		;

	GREG32(KEYMGR, SHA_ITOP) = 0;  /* clear status */

	return !!GREG32(KEYMGR, HKEY_ERR_FLAGS);
}

static int compute_certs(const uint32_t *certs, size_t num_certs)
{
	int i;

	for (i = 0; i < num_certs; i++) {
		if (ladder_step(certs[i], NULL))
			return 0;
	}

	return 1;
}

#define KEYMGR_CERT_0 0
#define KEYMGR_CERT_3 3
#define KEYMGR_CERT_4 4
#define KEYMGR_CERT_5 5
#define KEYMGR_CERT_7 7
#define KEYMGR_CERT_15 15
#define KEYMGR_CERT_20 20
#define KEYMGR_CERT_25 25
#define KEYMGR_CERT_26 26
#define KEYMGR_CERT_34 34
#define KEYMGR_CERT_35 35

static const uint32_t FRK2_CERTS_PREFIX[] = {
	KEYMGR_CERT_0,
	KEYMGR_CERT_3,
	KEYMGR_CERT_4,
	KEYMGR_CERT_5,
	KEYMGR_CERT_7,
	KEYMGR_CERT_15,
	KEYMGR_CERT_20,
};

static const uint32_t FRK2_CERTS_POSTFIX[] = {
	KEYMGR_CERT_26,
};

#define MAX_MAJOR_FW_VERSION 254

int DCRYPTO_ladder_compute_frk2(size_t fw_version, uint8_t *frk2)
{
	int result = 0;

	if (fw_version > MAX_MAJOR_FW_VERSION)
		return 0;

	if (!dcrypto_grab_sha_hw())
		return 0;

	do {
		int i;

		ladder_init();

		if (!compute_certs(FRK2_CERTS_PREFIX,
					ARRAY_SIZE(FRK2_CERTS_PREFIX)))
			break;

		for (i = 0; i < MAX_MAJOR_FW_VERSION - fw_version; i++) {
			if (ladder_step(KEYMGR_CERT_25, NULL))
				break;
		}

		if (!compute_certs(FRK2_CERTS_POSTFIX,
					ARRAY_SIZE(FRK2_CERTS_POSTFIX)))
			break;

		memcpy(frk2, (void *) GREG32_ADDR(KEYMGR, HKEY_FRR0),
			AES256_BLOCK_CIPHER_KEY_SIZE);

		result = 1;
	} while (0);

	dcrypto_release_sha_hw();
	return result;
}

/* ISR salt (SHA256("ISR_SALT")) to use for USR generation. */
static const uint32_t ISR_SALT[8] = {
	0x6ba1b495, 0x4b7ca214, 0xfe07e922, 0x09735185,
	0xfcca43ca, 0xc6d4dfd9, 0x5fc2fcca, 0xaa45400b
};

/* Map of populated USR registers. */
static int usr_ready[8]  = {};

int dcrypto_ladder_compute_usr(enum dcrypto_appid id,
			       const uint32_t usr_salt[8])
{
	int result = 0;

	/* Check for USR readiness. */
	if (usr_ready[id])
		return 1;

	if (!dcrypto_grab_sha_hw())
		return 0;

	do {
		int i;

		/* The previous check performed without lock acquisition. */
		if (usr_ready[id]) {
			result = 1;
			break;
		}

		ladder_init();

		if (!compute_certs(FRK2_CERTS_PREFIX,
					ARRAY_SIZE(FRK2_CERTS_PREFIX)))
			break;

		/* USR generation requires running the key-ladder till
		 * the end (version 0), plus one additional iteration.
		 */
		for (i = 0; i < MAX_MAJOR_FW_VERSION - 0 + 1; i++) {
			if (ladder_step(KEYMGR_CERT_25, NULL))
				break;
		}
		if (i != MAX_MAJOR_FW_VERSION - 0 + 1)
			break;

		if (ladder_step(KEYMGR_CERT_34, ISR_SALT))
			break;

		/* Output goes to USR[appid] (the multiply by 2 is an
		 * artifact of slot addressing).
		 */
		GWRITE_FIELD(KEYMGR, SHA_CERT_OVERRIDE, DIGEST_PTR, 2 * id);
		ccprintf("OVERRIDE %d\n", id);
		if (ladder_step(KEYMGR_CERT_35, usr_salt))
			break;

		/* Check for key-ladder errors. */
		if (GREG32(KEYMGR, HKEY_ERR_FLAGS))
			break;

		/* Key deposited in USR[id], and ready to use. */
		usr_ready[id] = 1;

		result = 1;
	} while (0);

	dcrypto_release_sha_hw();
	return result;
}

static void ladder_out(uint32_t output[8])
{
	output[0] = GREG32(KEYMGR, SHA_STS_H0);
	output[1] = GREG32(KEYMGR, SHA_STS_H1);
	output[2] = GREG32(KEYMGR, SHA_STS_H2);
	output[3] = GREG32(KEYMGR, SHA_STS_H3);
	output[4] = GREG32(KEYMGR, SHA_STS_H4);
	output[5] = GREG32(KEYMGR, SHA_STS_H5);
	output[6] = GREG32(KEYMGR, SHA_STS_H6);
	output[7] = GREG32(KEYMGR, SHA_STS_H7);
}

/*
 * Stir trng entropy into rsr and pull some out.
 */
int DCRYPTO_ladder_random(void *output)
{
	int error  = 1;
	uint32_t tmp[8];
	int i;

	if (!dcrypto_grab_sha_hw())
		goto fail;

	rand_bytes(tmp, sizeof(tmp));
	error = ladder_step(28, tmp);
	if (error)
		goto fail;

	if (!compute_certs(FRK2_CERTS_PREFIX, ARRAY_SIZE(FRK2_CERTS_PREFIX)))
		goto fail;
	/* USR generation requires running the key-ladder till
	 * the end (version 0), plus one additional iteration.
	 */
	for (i = 0; i < MAX_MAJOR_FW_VERSION - 0 + 1; i++) {
		if (ladder_step(KEYMGR_CERT_25, NULL))
			goto fail;
		cflush();
	}
	if (i != MAX_MAJOR_FW_VERSION - 0 + 1)
		goto fail;
	if (ladder_step(KEYMGR_CERT_34, ISR_SALT))
		goto fail;

	rand_bytes(tmp, 32);
	error = ladder_step(27, tmp);
	if (!error)
		ladder_out(output);
	else
		ccprintf("%s: error %x\n", __func__, error);

fail:
	dcrypto_release_sha_hw();
	return error;
}

/*
 * Diversification seeds.
 * Value is SHA256(varname)
 */
static const uint32_t KL_SEED_ATTEST[8] = {
	0x40640139, 0xcbfacf4a, 0xc2c2c27b, 0x9f2d9cba,
	0x8e3d41c3, 0x43bfe954, 0x81cd534f, 0x23804b05};
static const uint32_t KL_SEED_WRAP[8] = {
	0x4161c150, 0xb43c0c3c, 0xb1c62871, 0xa2abfc84,
	0x666d2091, 0x47c8f902, 0xdc5b993e, 0xe89daab8};
static const uint32_t KL_SEED_ORIGIN[8] = {
	0x06a7f502, 0x213c40c4, 0x5f3d4f19, 0x52ca943b,
	0x234e2fae, 0xddb6dc13, 0xaa9556c0, 0xb2d538f1};

int ladder_derive(const uint32_t salt[8], const uint32_t input[8],
	uint32_t output[8])
{
	int error = 1;

	cflush();
	error = ladder_step(35, salt); /* isr2 -> usr0 */
	if (error)
		ccprintf("%s:0 error %x\n", __func__, error);
	cflush();
	error = error || ladder_step(38, input); /* hmac */
	if (error)
		ccprintf("%s:1 error %x/%x\n", __func__, error,
			 GREG32(KEYMGR, HKEY_ERR_FLAGS));
	if (!error)
		ladder_out(output);

	return error;
}

int kl_derive_attest(const uint32_t input[8], uint32_t output[8])
{
	return ladder_derive(KL_SEED_ATTEST, input, output);
}

int kl_derive_wrap(const uint32_t input[8], uint32_t output[8])
{
	return ladder_derive(KL_SEED_WRAP, input, output);
}

int kl_derive_origin(const uint32_t input[8], uint32_t output[8])
{
	return ladder_derive(KL_SEED_ORIGIN, input, output);
}
