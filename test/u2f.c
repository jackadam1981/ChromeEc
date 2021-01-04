/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_util.h"
#include "u2f_impl.h"

/******************************************************************************/
/* Mock implementations of cr50 board.
 */
int system_get_chip_unique_id(uint8_t **id)
{
	return P256_NBYTES;
}

/******************************************************************************/
/* Mock implementations of Dcrypto functionality.
 */
int DCRYPTO_ladder_random(void *output)
{
	memset(output, 0, P256_NBYTES);
	/* Return 1 for success, 0 for error. */
	return 1;
}

int DCRYPTO_x509_gen_u2f_cert_name(const p256_int *d, const p256_int *pk_x,
				   const p256_int *pk_y, const p256_int *serial,
				   const char *name, uint8_t *cert,
				   const int n)
{
	/* Return the size of certificate, 0 means error. */
	return 0;
}

int dcrypto_p256_ecdsa_sign(struct drbg_ctx *drbg, const p256_int *key,
			    const p256_int *message, p256_int *r, p256_int *s)
{
	memset(r, 0, sizeof(p256_int));
	memset(s, 0, sizeof(p256_int));
	/* Return 1 for success, 0 for error. */
	return 1;
}

void hmac_drbg_init_rfc6979(struct drbg_ctx *ctx,
			    const p256_int *key,
			    const p256_int *message)
{
	memset(ctx, 0, sizeof(struct drbg_ctx));
}

void HASH_update(struct HASH_CTX *ctx, const void *data, size_t len)
{
	if (ctx)
		SHA256_update(ctx, data, len);
}

uint8_t *HASH_final(struct HASH_CTX *ctx)
{
	return SHA256_final(ctx);
}

void DCRYPTO_SHA256_init(LITE_SHA256_CTX *ctx, uint32_t sw_required)
{
	SHA256_init(ctx);
}

void DCRYPTO_HMAC_SHA256_init(LITE_HMAC_CTX *ctx, const void *key,
			      unsigned int len)
{
	SHA256_init(&ctx->hash);
}

const uint8_t *DCRYPTO_HMAC_final(LITE_HMAC_CTX *ctx)
{
	return SHA256_final(&ctx->hash);
}

/******************************************************************************/
/* Mock implementations of U2F functionality.
 */
static int presence;

static struct u2f_state state;

struct u2f_state *get_state(void)
{
	return &state;
}

enum touch_state pop_check_presence(int consume)
{
	enum touch_state ret = presence ?
		POP_TOUCH_YES : POP_TOUCH_NO;

	if (consume)
		presence = 0;
	return ret;
}

int u2f_origin_user_keypair(const uint8_t *key_handle, size_t key_handle_size,
			    p256_int *d, p256_int *pk_x, p256_int *pk_y)
{
	return EC_SUCCESS;
}

int g2f_individual_keypair(p256_int *d, p256_int *pk_x, p256_int *pk_y)
{
	return EC_SUCCESS;
}

/******************************************************************************/
/* Tests begin here.
 */
static uint8_t buffer[512];

static uint8_t kAuthTimeSecretHash[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
};

test_static int test_u2f_generate_no_require_presence(void)
{
	struct u2f_generate_req *req = (struct u2f_generate_req *)buffer;
	size_t response_size = sizeof(struct u2f_generate_resp);
	int ret;

	memset(buffer, 0, sizeof(buffer));
	req->flags = 0;
	presence = 0;
	ret = u2f_generate(
		VENDOR_CC_U2F_GENERATE, &buffer,
		sizeof(struct u2f_generate_req),
		&response_size);

	TEST_ASSERT(ret == VENDOR_RC_SUCCESS);
	return EC_SUCCESS;
}

test_static int test_u2f_generate_require_presence(void)
{
	struct u2f_generate_req *req = (struct u2f_generate_req *)buffer;
	size_t response_size = sizeof(struct u2f_generate_resp);
	int ret;

	memset(buffer, 0, sizeof(buffer));
	req->flags = U2F_AUTH_FLAG_TUP;
	presence = 0;
	ret = u2f_generate(
		VENDOR_CC_U2F_GENERATE, &buffer,
		sizeof(struct u2f_generate_req),
		&response_size);
	TEST_ASSERT(ret == VENDOR_RC_NOT_ALLOWED);

	memset(buffer, 0, sizeof(buffer));
	req->flags = U2F_AUTH_FLAG_TUP;
	response_size = sizeof(struct u2f_generate_resp);
	presence = 1;
	ret = u2f_generate(
		VENDOR_CC_U2F_GENERATE, &buffer,
		sizeof(struct u2f_generate_req),
		&response_size);
	TEST_ASSERT(ret == VENDOR_RC_SUCCESS);

	return EC_SUCCESS;
}

test_static int test_u2f_generate_sign_versioned(void)
{
	struct u2f_generate_req *generate_req
		= (struct u2f_generate_req *)buffer;
	size_t response_size = sizeof(struct u2f_generate_versioned_resp);
	int ret;
	struct u2f_generate_versioned_resp *generate_resp
		= (struct u2f_generate_versioned_resp *)buffer;
	struct u2f_versioned_key_handle vkh;
	struct u2f_sign_versioned_req *sign_req
		= (struct u2f_sign_versioned_req *)buffer;

	/* Generate. */
	memset(buffer, 0, sizeof(buffer));
	generate_req->flags = U2F_UV_ENABLED_KH;
	memcpy(generate_req->authTimeSecretHash, kAuthTimeSecretHash,
	       sizeof(generate_req->authTimeSecretHash));
	presence = 0;
	ret = u2f_generate(
		VENDOR_CC_U2F_GENERATE, &buffer,
		sizeof(struct u2f_generate_req),
		&response_size);
	TEST_ASSERT(ret == VENDOR_RC_SUCCESS);
	TEST_ASSERT(generate_resp->keyHandle.header.version == 1);
	TEST_ASSERT_ARRAY_EQ(
		generate_resp->keyHandle.authorization_secret_hash,
		kAuthTimeSecretHash,
		sizeof(generate_resp->keyHandle.authorization_secret_hash));

	/* Sign. */
	vkh = generate_resp->keyHandle;
	memset(buffer, 0, sizeof(buffer));
	response_size = sizeof(struct u2f_sign_resp);
	/* Since this is version 1, don't enforce presence. */
	sign_req->flags = 0;
	memcpy(&sign_req->keyHandle, &vkh,
	       sizeof(struct u2f_versioned_key_handle));
	ret = u2f_sign(
		VENDOR_CC_U2F_SIGN, &buffer,
		sizeof(struct u2f_sign_versioned_req),
		&response_size);
	TEST_ASSERT(ret == VENDOR_RC_SUCCESS);

	return EC_SUCCESS;
}

void run_test(void)
{
	RUN_TEST(test_u2f_generate_no_require_presence);
	RUN_TEST(test_u2f_generate_require_presence);
	RUN_TEST(test_u2f_generate_sign_versioned);

	test_print_result();
}
