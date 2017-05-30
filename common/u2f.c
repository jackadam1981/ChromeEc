/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* APDU dispatcher and U2F command handlers. */

#include "console.h"
#include "cryptoc/p256.h"
#include "cryptoc/sha256.h"
#include "dcrypto.h"
#include "nvcounter.h"
#include "u2f_impl.h"
#include "u2f.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ##args)

/* Crypto parameters */
#define AES_BLOCK_LEN 16
#define KH_LEN 64

/* Interleave bytes of two 32 byte arrays */
static void interleave32(const uint8_t *a, const uint8_t *b, uint8_t *out)
{
	size_t i;

	for (i = 0; i < 32; ++i) {
		out[2 * i + 0] = a[i];
		out[2 * i + 1] = b[i];
	}
}

/* De-interleave 64 bytes into two 32 arrays. */
static void deinterleave64(const uint8_t *in, uint8_t *a, uint8_t *b)
{
	size_t i;

	for (i = 0; i < 32; ++i) {
		a[i] = in[2 * i + 0];
		b[i] = in[2 * i + 1];
	}
}

/* (un)wrap w/ the origin dependent KEK. */
static int wrap_kh(const uint8_t *origin, const uint8_t *in,
		   uint8_t *out, enum encrypt_mode mode)
{
	uint8_t kek[SHA256_DIGEST_SIZE];
	uint8_t iv[AES_BLOCK_LEN] = {0};
	int i;

	/* KEK derivation */
	if (u2f_gen_kek(origin, kek, sizeof(kek)))
		return EC_ERROR_UNKNOWN;

	DCRYPTO_aes_init(kek, 256, iv, CIPHER_MODE_CBC, mode);

	for (i = 0; i < 4; i++)
		DCRYPTO_aes_block(in + i * AES_BLOCK_LEN,
				  out + i * AES_BLOCK_LEN);

	return EC_SUCCESS;
}

static int anonymous_cert(const p256_int *d, const p256_int *pk_x,
			  const p256_int *pk_y,  uint8_t *cert, const int n)
{
	return DCRYPTO_x509_gen_u2f_cert(d, pk_x, pk_y, NULL, cert, n);
}

static int individual_cert(const p256_int *d, const p256_int *pk_x,
			   const p256_int *pk_y,  uint8_t *cert, const int n)
{
	p256_int serial;

	board_get_serial(&serial);
	return DCRYPTO_x509_gen_u2f_cert(d, pk_x, pk_y, &serial, cert, n);
}

static uint16_t u2f_version(struct apdu apdu, uint8_t *obuf, uint16_t *obuf_len)
{
	static const char version[] = "U2F_V2";

	if (apdu.len)
		return U2F_SW_WRONG_LENGTH;

	memcpy(obuf, version, sizeof(version) - 1 /* not ending zero */);
	*obuf_len = sizeof(version) - 1;

	return U2F_SW_NO_ERROR;
}

/* U2F REGISTER command  */
static uint16_t u2f_register(struct apdu apdu, uint8_t *obuf,
			     uint16_t *obuf_len)
{
	const U2F_REGISTER_REQ *req = (const U2F_REGISTER_REQ *)apdu.data;
	U2F_REGISTER_RESP *resp = (U2F_REGISTER_RESP *)obuf;
	int l, m_off; /* msg length and interior offset */

	p256_int r, s;   /* ecdsa signature */
	/* Origin keypair */
	uint8_t od_seed[SHA256_DIGEST_SIZE];
	p256_int od, opk_x, opk_y;
	/* KDF, Key handle */
	HASH_CTX sha;
	uint8_t kh[U2F_APPID_SIZE + sizeof(p256_int)];
	uint8_t buf[U2F_APPID_SIZE + sizeof(p256_int)];
	/* sha256({RFU, app ID, nonce, keyhandle, public key}) */
	p256_int h;
	const uint8_t rfu = U2F_REGISTER_HASH_ID;
	const uint8_t pk_start = U2F_POINT_UNCOMPRESSED;
	p256_int att_d;
	uint32_t cert_len;

	if (apdu.len != sizeof(U2F_REGISTER_REQ)) {
		CPRINTF("#ERR REGISTER wrong length");
		return U2F_SW_WRONG_LENGTH;
	}

	/* Check user presence, w/ optional consume */
	if (pop_check_presence(apdu.p1 & G2F_CONSUME) != POP_TOUCH_YES &&
	    (apdu.p1 & U2F_AUTH_FLAG_TUP) != 0) {
		return U2F_SW_CONDITIONS_NOT_SATISFIED;
	}

	/* Generate origin-specific keypair */
	if (u2f_origin_keypair(od_seed, &od, &opk_x, &opk_y) !=
	    EC_SUCCESS) {
		CPRINTF("#ERR Origin-specific keypair generation failed");
		return U2F_SW_WTF + 1;
	}

	/* Generate key handle */
	/* Interleave origin ID, origin priv key, wrap and export. */
	interleave32(req->appId, od_seed, buf);
	if (wrap_kh(req->appId, buf, kh, ENCRYPT_MODE) != EC_SUCCESS)
		return U2F_SW_WTF + 2;

	/* Response message hash for signing */
	DCRYPTO_SHA256_init(&sha, 0);
	HASH_update(&sha, &rfu, sizeof(rfu));
	HASH_update(&sha, req->appId, U2F_APPID_SIZE);
	HASH_update(&sha, req->chal, U2F_CHAL_SIZE);
	HASH_update(&sha, kh, sizeof(kh));
	HASH_update(&sha, &pk_start, sizeof(pk_start));
	/* Insert origin-specific public keys into the response */
	p256_to_bin(&opk_x, resp->pubKey.x); /* endianness */
	p256_to_bin(&opk_y, resp->pubKey.y); /* endianness */
	HASH_update(&sha, resp->pubKey.x, sizeof(p256_int));
	HASH_update(&sha, resp->pubKey.y, sizeof(p256_int));
	p256_from_bin(HASH_final(&sha), &h);

	/* Construct remainder of the response */
	resp->registerId = U2F_REGISTER_ID;
	l = sizeof(resp->registerId);
	resp->pubKey.pointFormat = U2F_POINT_UNCOMPRESSED;
	l += sizeof(resp->pubKey);
	resp->keyHandleLen = sizeof(kh);
	l += sizeof(resp->keyHandleLen);
	memcpy(resp->keyHandleCertSig, kh, sizeof(kh));
	l += sizeof(kh);
	m_off = sizeof(kh);

	if (use_g2f() && apdu.p1 & G2F_ATTEST) {
		/* Use a hw-derived keypair for Individual attestation */
		if (g2f_individual_keypair(&att_d, &opk_x, &opk_y)) {
			CPRINTF("#ERR Attestation key generation failed");
			return U2F_SW_WTF + 3;
		}
		cert_len = individual_cert(&att_d, &opk_x, &opk_y,
				resp->keyHandleCertSig + m_off,
				U2F_MAX_ATT_CERT_SIZE);
	} else {
		/* Anon attestation keypair; use origin key to self-sign */
		cert_len = anonymous_cert(&od, &opk_x, &opk_y,
				resp->keyHandleCertSig + m_off,
				U2F_MAX_ATT_CERT_SIZE);
		att_d = od;
	}
	if (cert_len == 0)
		return U2F_SW_WTF + 4;

	l += cert_len;
	m_off += cert_len;

	/* Sign over the response w/ the attestation key */
	if (!dcrypto_p256_ecdsa_sign(&att_d, &h, &r, &s)) {
		p256_clear(&att_d);
		CPRINTF("#ERR signing error");
		return U2F_SW_WTF + 5;
	}
	p256_clear(&att_d);

	/* Signature -> ASN.1 DER encoded bytes */
	l += DCRYPTO_asn1_sigp(resp->keyHandleCertSig + m_off, &r, &s);

	*obuf_len = l;

	return U2F_SW_NO_ERROR; /* APDU success */
}

static uint16_t u2f_authenticate(struct apdu apdu, uint8_t *obuf,
				 uint16_t *obuf_len)
{
	const U2F_AUTHENTICATE_REQ *req = (const void *)apdu.data;
	U2F_AUTHENTICATE_RESP *resp = (U2F_AUTHENTICATE_RESP *)obuf;
	uint8_t unwrapped_kh[KH_LEN];
	uint8_t od_seed[SHA256_DIGEST_SIZE];

	p256_int origin_d;
	uint8_t origin[U2F_APPID_SIZE];

	HASH_CTX sha;
	p256_int h, r, s;
	uint8_t sig_len;

	uint32_t count = 0;

	if (apdu.len != U2F_APPID_SIZE + U2F_CHAL_SIZE + 1 + KH_LEN) {
		CPRINTF("#ERR AUTHENTICATE wrong length %d", apdu.len);
		return U2F_SW_WRONG_LENGTH;
	}

	/* Unwrap key handle */
	if (wrap_kh(req->appId, req->keyHandle, unwrapped_kh, DECRYPT_MODE))
		return U2F_SW_WTF + 1;
	deinterleave64(unwrapped_kh, origin, od_seed);

	/* Check whether appId (i.e. origin) matches.
	 * Constant time.
	 */
	p256_from_bin(origin, &r);
	p256_from_bin(req->appId, &s);
	if (p256_cmp(&r, &s) != 0)
		return U2F_SW_WRONG_DATA;

	/* Origin check only? */
	if (apdu.p1 & U2F_AUTH_CHECK_ONLY)
		return U2F_SW_CONDITIONS_NOT_SATISFIED;

	/* Sense user presence, with optional consume */
	resp->flags =
		pop_check_presence(apdu.p1 & G2F_CONSUME) == POP_TOUCH_YES;

	/* Mandatory user presence? */
	if ((apdu.p1 & U2F_AUTH_ENFORCE) != 0 && resp->flags == 0)
		return U2F_SW_CONDITIONS_NOT_SATISFIED;

	/* Increment-only counter in flash. OK to share between origins. */
	count = nvcounter_incr();
	resp->ctr[0] = (count >> 24) & 0xFF;
	resp->ctr[1] = (count >> 16) & 0xFF;
	resp->ctr[2] = (count >> 8) & 0xFF;
	resp->ctr[3] = count & 0xFF;

	/* Message signature */
	DCRYPTO_SHA256_init(&sha, 0);
	HASH_update(&sha, req->appId, U2F_APPID_SIZE);
	HASH_update(&sha, &(resp->flags), sizeof(uint8_t));
	HASH_update(&sha, resp->ctr, U2F_CTR_SIZE);
	HASH_update(&sha, req->chal, U2F_CHAL_SIZE);
	p256_from_bin(HASH_final(&sha), &h);

	if (u2f_origin_key(od_seed, &origin_d))
		return U2F_SW_WTF + 2;

	if (!dcrypto_p256_ecdsa_sign(&origin_d, &h, &r, &s)) {
		p256_clear(&origin_d);
		return U2F_SW_WTF + 3;
	}
	p256_clear(&origin_d);

	sig_len = DCRYPTO_asn1_sigp(resp->sig, &r, &s);

	*obuf_len = sizeof(resp->flags) + U2F_CTR_SIZE + sig_len;
	return U2F_SW_NO_ERROR;
}

uint16_t u2f_apdu_rcv(const uint8_t *ibuf, uint16_t in_len, uint8_t *obuf)
{
	uint16_t obuf_len = 0;
	uint16_t sw = U2F_SW_CLA_NOT_SUPPORTED;
	/*
	 * APDU structure:
	 * [CLA INS P1 P2 [LC1 [LC2 LC3 <request-data>]]]
	 */
	uint8_t cla = ibuf[0];
	uint8_t ins = ibuf[1];
	struct apdu apdu = {
		.p1 = ibuf[2],
		.p2 = ibuf[3],
		.len = 0,
		.data = ibuf + 5
	};

	/* ISO7618 LC decoding */
	if (in_len >= 5)
		apdu.len = ibuf[4];

	if (apdu.len == 0 && in_len >= 7) {
		apdu.len = (ibuf[5] << 8) | ibuf[6];
		apdu.data += 2;
	}

	CPRINTF("%T/%d U2F APDU ", apdu.len);

	if (cla == 0x00) { /* Always 0x00 */
		sw = U2F_SW_INS_NOT_SUPPORTED;

		switch (ins) {
		case (U2F_REGISTER):
			CPRINTF("REGISTER");
			sw = u2f_register(apdu, obuf, &obuf_len);
			break;

		case (U2F_AUTHENTICATE):
			CPRINTF("AUTHENTICATE");
			sw = u2f_authenticate(apdu, obuf, &obuf_len);
			break;

		case (U2F_VERSION):
			CPRINTF("VERSION");
			sw = u2f_version(apdu, obuf, &obuf_len);
			break;
		}

		/* Not a U2F INS. Try internal extensions next. */
		if (sw == U2F_SW_INS_NOT_SUPPORTED &&
		    use_g2f() && u2f_custom_dispatch)
			sw = u2f_custom_dispatch(ins, apdu, obuf, &obuf_len);
	}

	/* append SW status word */
	obuf[obuf_len++] = sw >> 8;
	obuf[obuf_len++] = sw;

	CPRINTF(" resp %04x len %d\n", sw, obuf_len);

	return obuf_len;
}
