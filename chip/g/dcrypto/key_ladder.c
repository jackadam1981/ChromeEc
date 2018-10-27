/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "internal.h"
#include "endian.h"
#include "registers.h"
#include "trng.h"
#include "console.h"

#define LOG_ERROR(msg) 	\
	do { \
		ccprintf("ERR [%s:%d]: %s\n" ,__func__, __LINE__, msg);  \
	} while (0)

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
#define KEYMGR_CERT_27 27
#define KEYMGR_CERT_28 28
#define KEYMGR_CERT_34 34
#define KEYMGR_CERT_35 35
#define KEYMGR_CERT_38 38

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
 * Stir TRNG entropy into RSR and pull some out.
 */
int DCRYPTO_ladder_random(void *output)
{
	int error = 1;
	uint32_t tmp[8];
	int i;

	if (!dcrypto_grab_sha_hw())
		goto fail;

	rand_bytes(tmp, sizeof(tmp));
	error = ladder_step(KEYMGR_CERT_28, tmp);
	if (error)
		goto fail;

	if (!compute_certs(FRK2_CERTS_PREFIX, ARRAY_SIZE(FRK2_CERTS_PREFIX)))
		goto fail;
	/* USR generation requires running the key-ladder till
	 * the end (version 0), plus one additional iteration.
	 */
	for (i = 0; i < MAX_MAJOR_FW_VERSION - 0 + 1; i++)
		if (ladder_step(KEYMGR_CERT_25, NULL))
			goto fail;
	if (i != MAX_MAJOR_FW_VERSION - 0 + 1)
		goto fail;
	if (ladder_step(KEYMGR_CERT_34, ISR_SALT))
		goto fail;

	rand_bytes(tmp, sizeof(tmp));
	error = ladder_step(KEYMGR_CERT_27, tmp);
	if (!error)
		ladder_out(output);

fail:
	dcrypto_release_sha_hw();
	return !error;
}

int dcrypto_ladder_derive(enum dcrypto_appid appid, const uint32_t salt[8],
			  const uint32_t input[8], uint32_t output[8])
{
	int error;

	if (!dcrypto_grab_sha_hw())
		return 0;

	GWRITE_FIELD(KEYMGR, SHA_CERT_OVERRIDE, KEY_PTR, 2 * appid);
	error = ladder_step(KEYMGR_CERT_38, input); /* HMAC */
	if (!error)
		ladder_out(output);

	dcrypto_release_sha_hw();
	return !error;
}

void DCRYPTO_ladder_revoke(void)
{
	/* Revoke certificates */
	GWRITE(KEYMGR, CERT_REVOKE_CTRL0, 0xFFFFFFFF);

#if 0
	GWRITE(KEYMGR, CERT_REVOKE_CTRL1,
		GC_KEYMGR_CERT_REVOKE_CTRL1_DERIVE_TESTMODE_PASSWORD_ROOTKEY_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_DERIVE_TESTMODE_PASSWORD_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_DERIVE_STAGE2_FIRMWARE_HIK0_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_DERIVE_STAGE2_FIRMWARE_HIK1_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_DERIVE_STAGE2_FIRMWARE_HIK2_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_STAGE2_HIK0_FIRMWARE_HASH_CHAIN_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_FW2_HIK0_CHAIN_LAST_LINK_EXPORT_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_STAGE2_HIK1_FIRMWARE_HASH_CHAIN_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_FW2_HIK1_CHAIN_LAST_LINK_EXPORT_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_STAGE2_HIK2_FIRMWARE_HASH_CHAIN_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_FW2_HIK2_CHAIN_LAST_LINK_EXPORT_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_GET_STIRRED_RANDOM_DATA_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_STIR_RANDOM_DATA_AND_UPDATE_RSR_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_STIR_RANDOM_DATA_INTO_USRS_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL1_HIK0_ISR0_KEYS_MASK|
		GC_KEYMGR_CERT_REVOKE_CTRL1_HIK0_USR_KEYS_MASK|
		0);
#endif

#if 0
	REG16(GBASE(KEYMGR) + GOFFSET(KEYMGR, CERT_REVOKE_CTRL2)) =
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK1_ISR1_KEYS_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK1_USR_KEYS_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK2_ISR2_KEYS_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK2_USR_KEYS_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK0_HMAC_USER_DATA_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK1_HMAC_USER_DATA_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HIK2_HMAC_USER_DATA_MASK |
		GC_KEYMGR_CERT_REVOKE_CTRL2_HASH_ROM_FOR_RBC_MASK |
		0;
#endif

	/* Wipe out the hidden keys cached in AES and SHA engines. */
	GWRITE_FIELD(KEYMGR, AES_USE_HIDDEN_KEY, ENABLE, 0);
	GWRITE_FIELD(KEYMGR, SHA_USE_HIDDEN_KEY, ENABLE, 0);

	/* Clear usr_ready[] */
	memset(usr_ready, 0, sizeof(usr_ready));
}

#ifdef CR50_DEV
static void _dump_registers(void) {
	cflush();
	ccprintf(" -----------------------------------------\n");
	ccprintf("CERT_REVOKE_CTRL[0-2]: %08X %08X %04X\n",
					GREG32(KEYMGR, CERT_REVOKE_CTRL0),
					GREG32(KEYMGR, CERT_REVOKE_CTRL1),
					REG16(GBASE(KEYMGR) + GOFFSET(KEYMGR, CERT_REVOKE_CTRL2))
					);
	ccprintf("HKEY_ERR_FLAGS       : %08X\n",
		GREG32(KEYMGR, HKEY_ERR_FLAGS));
	ccprintf("AES_CTRL             : %08X\n", GREG32(KEYMGR, AES_CTRL));
	ccprintf("AES_USE_HIDDEN_      : %08X\n",
		GREG32(KEYMGR, AES_USE_HIDDEN_KEY));
	cflush();

	ccprintf("HKEY_FRR[0-7]        : %08X %08X %08X %08X %08X %08X %08X %08X\n",
					GREG32(KEYMGR, HKEY_FRR0),
					GREG32(KEYMGR, HKEY_FRR1),
					GREG32(KEYMGR, HKEY_FRR2),
					GREG32(KEYMGR, HKEY_FRR3),
					GREG32(KEYMGR, HKEY_FRR4),
					GREG32(KEYMGR, HKEY_FRR5),
					GREG32(KEYMGR, HKEY_FRR6),
					GREG32(KEYMGR, HKEY_FRR7));
	ccprintf(" -----------------------------------------\n");
	cflush();
}

/**
 * Test function: Do the cipher before and after DCRYPTO_ladder_revoke.
                  Compare the result
 */
static int test_keyladder_revocation(int argc, char *argv[])
{
	uint32_t appid = 0;
	uint8_t sha1_digest[SHA_DIGEST_SIZE];
	uint32_t inp0[SHA256_DIGEST_WORDS] = {0};
	uint32_t enc1[SHA256_DIGEST_WORDS] = {0};	// Encrypt result
	uint32_t dec2[SHA256_DIGEST_WORDS] = {0};	// Decrypt result before revocation
	uint32_t dec3[SHA256_DIGEST_WORDS] = {0};	// Decrypt result after revocation
	int retval = EC_ERROR_UNKNOWN;
	uint32_t *buf_debug = NULL;
	int do_revoke = 1;

	rand_bytes(inp0, sizeof(inp0));
	rand_bytes(enc1, sizeof(enc1) / 2);	// fill with garbage just the half
	rand_bytes(dec2, sizeof(dec2) / 2);	// fill with garbage just the half
	rand_bytes(dec3, sizeof(dec3) / 2);	// fill with garbage just the half

	rand_bytes(sha1_digest, sizeof(sha1_digest));

	/* Warm Up: practice cipher for all appid */
	for (appid = 0; appid < 7; appid++)
		DCRYPTO_app_cipher(appid, sha1_digest, enc1, inp0, sizeof(enc1));

	appid = (argc > 1) ? atoi(argv[1]) : 0;

	if (argc > 2)
		do_revoke = memcmp(argv[2], "0", 1);

	/*
	 * Use the built in dcrypto engine to generate the sha1 hash of the
	 * buffer.
	 */

	ccprintf("                  appid  %d\n", appid);
	ccprintf("            revoke test  %s\n", do_revoke ? "TRUE" : "FALSE");
	ccprintf("            sha1_digest  %.20h\n", sha1_digest);
	ccprintf("                   inp0  %.32h\n", inp0);

	do {
		/*  */
		if (!DCRYPTO_app_cipher(appid, sha1_digest, enc1, inp0, sizeof(enc1))) {
			LOG_ERROR("cipher(inp0) fails");
			buf_debug = enc1;
			break;
		}
		ccprintf("     cipher(inp0)->enc1  %.32h\n", enc1);

		if (!DCRYPTO_app_cipher(appid, sha1_digest, dec2, enc1, sizeof(dec2))) {
			LOG_ERROR("cipher(enc1) fails");
			buf_debug = dec2;
			break;
		}
		ccprintf("     cipher(enc1)->dec2  %.32h\n", dec2);

#if 1
		/* Check whether cipher() result is consistent. */
		memset(usr_ready, 0, sizeof(usr_ready));
		if (!DCRYPTO_app_cipher(appid, sha1_digest, dec2, enc1, sizeof(dec2))) {
			LOG_ERROR("cipher(enc1) fails");
			buf_debug = dec2;
			break;
		}
		ccprintf("     cipher(enc1)->dec2  %.32h\n", dec2);
		/* Check whether cipher() result is consistent. */
		memset(usr_ready, 0, sizeof(usr_ready));
		if (!DCRYPTO_app_cipher(appid, sha1_digest, dec2, enc1, sizeof(dec2))) {
			LOG_ERROR("cipher(enc1) fails");
			buf_debug = dec2;
			break;
		}
		ccprintf("     cipher(enc1)->dec2  %.32h\n", dec2);
#endif

		if (!DCRYPTO_equals(inp0, dec2, sizeof(enc1))) {
			LOG_ERROR("inp0 != cipher(cipher(inp0))");
			buf_debug = dec2;
			break;
		}

		_dump_registers();
		/*   */
		if (do_revoke) {
			ccprintf("  [ keyladder revocation ]\n");
			DCRYPTO_ladder_revoke();
		}

		/*   */
		if (!DCRYPTO_app_cipher(appid, sha1_digest, dec3, enc1, sizeof(dec3))) {
			LOG_ERROR("cipher(dec3) fails");
			buf_debug = dec3;
			break;

		}
		ccprintf("     cipher(enc1)->dec3  %.32h\n", dec3);

		/*   */
		if (do_revoke && DCRYPTO_equals(dec2, dec3, sizeof(dec3))) {
			LOG_ERROR("Revocation didn't affect cipher result");
			buf_debug = dec3;
			break;
		}

#if 1
		/* Check whether cipher() result is random. */
		memset(usr_ready, 0, sizeof(usr_ready));
		if (!DCRYPTO_app_cipher(appid, sha1_digest, dec3, enc1,
			sizeof(dec3))) {
			LOG_ERROR("cipher(dec3) fails");
			buf_debug = dec3;
			break;

		}
		ccprintf("     cipher(enc1)->dec3  %.32h\n", dec3);

		/*   */
		memset(usr_ready, 0, sizeof(usr_ready));
		if (!DCRYPTO_app_cipher(appid, sha1_digest, dec3, enc1,
			sizeof(dec3))) {
			LOG_ERROR("cipher(dec3) fails");
			buf_debug = dec3;
			break;

		}
		ccprintf("     cipher(enc1)->dec3  %.32h\n", dec3);
#endif

		retval = EC_SUCCESS;
	} while(0);

	_dump_registers();
	if (retval) {
		if (buf_debug)
			ccprintf("          last output    %.32h\n", buf_debug);
		GREG32(KEYMGR, HKEY_ERR_FLAGS) = 0xFFFFFFFF;
	}

	return retval;
}
DECLARE_CONSOLE_COMMAND(testc, test_keyladder_revocation,
			NULL,
			"Test Keyladder Revocation");
#endif

