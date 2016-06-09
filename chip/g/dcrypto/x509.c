/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"

#include <stdint.h>

#define MAX_ASN1_OBJ_LEN_BYTES 2

/* From https://tools.ietf.org/html/rfc5754#section-3.2
 *
 * Only the object bytes below, the DER encoding header ([0x30 0x0d])
 * is verified by the parser. */
static const uint8_t OID_SHA256_WITH_RSA_ENCRYPTION[13] = {
	0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
	0x01, 0x01, 0x0b, 0x05, 0x00
};

/*
 * An ASN.1 DER (Definite Encoding Rules) parser.
 * Details about the format are available here:
 *     https://en.wikipedia.org/wiki/X.690#Definite_form
 */
static size_t asn1_parse(const uint8_t *p, size_t available,
			uint8_t expected_type, const uint8_t **out,
			size_t *out_len)
{
	size_t obj_len_bytes = 0;
	size_t obj_len = 0;

	if (available < 2)
		return 0;
	if (p[0] != expected_type)
		return 0;
	if ((p[1] & 128) == 0) {
		if (2 + p[1] > available)
			return 0;
		obj_len = p[1];
		if (out)
			*out = &p[2];
		if (out_len)
			*out_len = obj_len;
		return 2 + obj_len;
	} else if ((p[1] & ~128) > MAX_ASN1_OBJ_LEN_BYTES ||
		2 + (p[1] & ~128) > available) {
		return 0;
	} else {
		int i;

		obj_len_bytes = p[1] & ~128;
		for (i = 0; i < obj_len_bytes; i++) {
			obj_len <<= 8;
			obj_len |= p[2 + i];
		}

		if (2 + obj_len_bytes + obj_len > available)
			return 0;
		if (out)
			*out = &p[2 + obj_len_bytes];
		if (out_len)
			*out_len = obj_len;
		return 2 + obj_len_bytes + obj_len;
	}
}

#define V_ASN1_CONSTRUCTED 0x20
#define V_ASN1_SEQUENCE    0x10
#define V_ASN1_BIT_STRING  0x03

/* This method verifies that the provided X509 certificate was issued
 * by the specified ceritifcate authority.
 *
 * cert is a pointer to a DER encoded X509 certificate, as specified
 * in https://tools.ietf.org/html/rfc5280#section-4.1.  In ASN.1
 * notation, the certificate has the following structure:
 *
 *   Certificate  ::=  SEQUENCE  {
 *        tbsCertificate       TBSCertificate,
 *        signatureAlgorithm   AlgorithmIdentifier,
 *        signatureValue       BIT STRING  }
 *
 *   TBSCertificate  ::=  SEQUENCE  { }
 *   AlgorithmIdentifier  ::=  SEQUENCE  { }
 *
 * where signatureValue = SIGN(HASH(tbsCertificate)), with SIGN and
 * HASH specified by signatureAlgorithm.
 */
int DCRYPTO_x509_verify(const uint8_t *cert, size_t len, struct RSA *ca_pub_key)
{
	const uint8_t *p = cert;
	const uint8_t *tbs;
	size_t tbs_len;
	const uint8_t *alg_oid;
	size_t alg_oid_len;
	const uint8_t *sig;
	size_t sig_len;
	size_t consumed;
	size_t obj_len;

	uint8_t digest[SHA256_DIGEST_SIZE];

	/* Read Certificate SEQUENCE. */
	consumed = asn1_parse(p, len, V_ASN1_CONSTRUCTED | V_ASN1_SEQUENCE,
			NULL, &obj_len);
	if (consumed == 0)
		return 0;
	if (consumed != len)
		return 0;     /* Invalid SEQUENCE. */
	p += consumed - obj_len;
	len -= consumed - obj_len;

	/* Read tbsCertificate SEQUENCE. */
	consumed = asn1_parse(p, len, V_ASN1_CONSTRUCTED | V_ASN1_SEQUENCE,
			NULL, NULL);
	if (consumed == 0)
		return 0;
	tbs = p;
	tbs_len = consumed;
	p += consumed;
	len -= consumed;

	/* Read signatureAlgorithm SEQUENCE. */
	consumed = asn1_parse(p, len, V_ASN1_CONSTRUCTED | V_ASN1_SEQUENCE,
			&alg_oid, &alg_oid_len);
	if (consumed == 0)
		return 0;
	if (alg_oid_len != sizeof(OID_SHA256_WITH_RSA_ENCRYPTION))
		return 0;
	if (memcmp(alg_oid, OID_SHA256_WITH_RSA_ENCRYPTION,
			sizeof(OID_SHA256_WITH_RSA_ENCRYPTION)) != 0)
		return 0;
	p += consumed;
	len -= consumed;

	/* Read signatureValue BIT STRING. */
	consumed = asn1_parse(p, len, V_ASN1_BIT_STRING,
			&sig, &sig_len);
	if (consumed == 0)
		return 0;
	if (consumed != len)
		return 0;   /* Not all input bytes consumed. */
	if (sig_len < bn_size(&ca_pub_key->N))
		return 0;

	/* Check that leading signature bytes (if any) are zero. */
	while (sig_len > bn_size(&ca_pub_key->N)) {
		if (sig[0] != 0)
			return 0;
		sig++;
		sig_len--;
	}

	DCRYPTO_SHA256_hash(tbs, tbs_len, digest);
	return DCRYPTO_rsa_verify(ca_pub_key, digest, sizeof(digest),
				sig, sig_len, PADDING_MODE_PKCS1, HASH_SHA256);
}
