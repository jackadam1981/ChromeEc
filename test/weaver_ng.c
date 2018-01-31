/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <weaver_ng.h>

#include <sha256.h>
#include <stdint.h>
#include <string.h>
#include <util.h>
#include "test_util.h"

/******************************************************************************/
/* Mock implementations of TPM, TRNG, and Dcrypto functionality.
 */

uint32_t MOCK_restart_count;
uint32_t get_restart_count(void)
{
	return MOCK_restart_count;
}

const uint8_t *MOCK_rand_bytes_src;
size_t MOCK_rand_bytes_offset;
void rand_bytes(void *buffer, size_t len)
{
	if (!MOCK_rand_bytes_src)
		return;

	memcpy(buffer, MOCK_rand_bytes_src + MOCK_rand_bytes_offset, len);
	MOCK_rand_bytes_offset += len;
}

void HASH_update(struct HASH_CTX *ctx, const void *data, size_t len)
{
	uint32_t zero = 0;
	/* TODO(allenwebb) implement this. */
	task_dump_trace();
	zero = 1 / zero;
}

const uint8_t *DCRYPTO_SHA256_hash(const void *data, uint32_t n,
				   uint8_t *digest)
{
	struct sha256_ctx ctx;
	uint8_t *tmp;

	SHA256_init(&ctx);
	memcpy(ctx.buf, digest, SHA256_DIGEST_SIZE);

	SHA256_update(&ctx, data, n);
	tmp = SHA256_final(&ctx);
	memcpy(digest, tmp, SHA256_DIGEST_SIZE);
	return digest;
}

void DCRYPTO_HMAC_SHA256_init(LITE_HMAC_CTX *ctx, const void *key,
			      unsigned int len)
{
	uint32_t zero = 0;
	/* TODO(allenwebb) implement this. */
	task_dump_trace();
	zero = 1 / zero;
}

const uint8_t *DCRYPTO_HMAC_final(LITE_HMAC_CTX *ctx)
{
	uint32_t zero = 0;
	/* TODO(allenwebb) implement this. */
	task_dump_trace();
	zero = 1 / zero;
	return 0;
}

int DCRYPTO_aes_ctr(uint8_t *out, const uint8_t *key, uint32_t key_bits,
		    const uint8_t *iv, const uint8_t *in, size_t in_len)
{
	/* TODO(allenwebb) implement this. */
	TEST_ASSERT(0);
	return 0;
}

/******************************************************************************/
/* Helper functions
 */

/* For debugging and generating test data. */
static void print_array(uint8_t (*data)[], size_t n) __attribute__ ((unused));
static void print_array(uint8_t (*data)[], size_t n)
{
	size_t x;

	if (n > 0) {
		ccprintf("uint8_t data[] = {");
		for (x = 0; x < n - 1; ++x) {
			if ((x & 7) != 7)
				ccprintf("0x%02x, ", (*data)[x]);
			else
				ccprintf("0x%02x,\n", (*data)[x]);
		}
		ccprintf("0x%02x};\n", (*data)[x]);
	}
}

/******************************************************************************/
/* Test data
 */
const merkle_tree_t empty_tree = {
	2 /* param_logk */,
	6 /* param_h */,
	{0x81, 0xaa, 0xe9, 0xde, 0x93, 0xf4, 0xdf, 0x88,
	 0x18, 0xfa, 0xff, 0xbd, 0xb7, 0x09, 0xc0, 0x86,
	 0x48, 0xdd, 0xcd, 0x35, 0x00, 0xf2, 0x88, 0xd6,
	 0x3f, 0xa6, 0x5e, 0x80, 0x10, 0x19, 0x41, 0x17} /* root */,
	{0x96, 0xc6, 0xb1, 0x64, 0xb6, 0xa7, 0xa8, 0x01,
	 0xd5, 0x1d, 0x8e, 0x97, 0x24, 0x86, 0xf8, 0x6f,
	 0xd4, 0x84, 0x0f, 0x95, 0x52, 0x93, 0x8d, 0x7d,
	 0x00, 0xbb, 0xba, 0xc8, 0xed, 0x7f, 0xa4, 0x7a} /* hmac_key */,
	{0x95, 0xc9, 0x0a, 0xd4, 0xb3, 0x61, 0x1b, 0xcf,
	 0x1b, 0x49, 0x2b, 0xd6, 0x5d, 0xbc, 0x80, 0xa9,
	 0xf4, 0x83, 0xf2, 0x84, 0xd4, 0x04, 0x57, 0x7f,
	 0x02, 0xae, 0x37, 0x64, 0xae, 0xda, 0x71, 0x2a} /* wrap_key */,
	{} /* public_key */,
	{} /* private_key */,
};

/******************************************************************************/
/* Header validation test cases.
 */

static int TEST_ASSERT_ERROR_MSG(merkle_tree_t *merkle_tree,
				  wng_response_t *response, uint32_t code)
{
	TEST_ASSERT(response->header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response->header.type == WNG_MT_ERROR_MSG);
	TEST_ASSERT(response->header.data_length == 0);
	TEST_ASSERT(response->header.result_code == code);
	TEST_ASSERT_ARRAY_EQ(response->header.root, merkle_tree->root,
			     sizeof(response->header.root));
	return EC_SUCCESS;
}

static int handle_request_version_mismatch(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memcpy(&merkle_tree, &empty_tree, sizeof(merkle_tree));
	memset(&response, 0x77, sizeof(response));

	request.header.version = WNG_PROTOCOL_VERSION + 1;
	request.header.type = WNG_MTA_RESET_TREE;
	request.header.data_length = sizeof(wng_request_reset_tree_t);
	request.data.reset_tree.param_logk = 2;
	request.data.reset_tree.param_h = 12;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    WNG_ERR_VERSION_MISMATCH);
	return TEST_ASSERT_ERROR_MSG(&merkle_tree, &response,
				     WNG_ERR_VERSION_MISMATCH);
}

static int handle_request_invalid_type(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memcpy(&merkle_tree, &empty_tree, sizeof(merkle_tree));
	memset(&response, 0x77, sizeof(response));

	request.header.version = WNG_PROTOCOL_VERSION;
	request.header.type = WNG_MT_INVALID;
	request.header.data_length = 0;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    WNG_ERR_TYPE_INVALID);
	return TEST_ASSERT_ERROR_MSG(&merkle_tree, &response,
				     WNG_ERR_TYPE_INVALID);
}

static int handle_reset_tree_invalid_length(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memcpy(&merkle_tree, &empty_tree, sizeof(merkle_tree));
	memset(&response, 0x77, sizeof(response));

	request.header.version = WNG_PROTOCOL_VERSION;
	request.header.type = WNG_MTQ_RESET_TREE;
	request.header.data_length = sizeof(wng_request_reset_tree_t) + 1;
	request.data.reset_tree.param_logk = 2;
	request.data.reset_tree.param_h = 12;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    WNG_ERR_LENGTH_INVALID);
	return TEST_ASSERT_ERROR_MSG(&merkle_tree, &response,
				     WNG_ERR_LENGTH_INVALID);
}

/******************************************************************************/
/* Reset Tree test cases.
 */

static int handle_reset_tree_param_logk_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memset(&merkle_tree, 0, sizeof(merkle_tree));
	MOCK_rand_bytes_src = (uint8_t *)0;
	MOCK_rand_bytes_offset = 0;
	memset(&response, 0x77, sizeof(response));

	/* Test lower bound. */
	request.header.version = WNG_PROTOCOL_VERSION;
	request.header.type = WNG_MTQ_RESET_TREE;
	request.header.data_length = sizeof(wng_request_reset_tree_t);
	request.data.reset_tree.param_logk = PARAM_LOGK_MIN - 1;
	request.data.reset_tree.param_h = 6;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    WNG_ERR_PARAM_LOGK_INVALID);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_TREE);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_ASSERT(response.header.result_code == WNG_ERR_PARAM_LOGK_INVALID);
	TEST_ASSERT_ARRAY_EQ(response.header.root, merkle_tree.root,
			     sizeof(merkle_tree.root));

	/* Test upper bound. */
	request.data.reset_tree.param_logk = PARAM_LOGK_MAX + 1;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    WNG_ERR_PARAM_LOGK_INVALID);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_TREE);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_ASSERT(response.header.result_code == WNG_ERR_PARAM_LOGK_INVALID);
	TEST_ASSERT_ARRAY_EQ(response.header.root, merkle_tree.root,
			     sizeof(merkle_tree.root));

	return EC_SUCCESS;
}

static int handle_reset_tree_param_h_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memset(&merkle_tree, 0, sizeof(merkle_tree));
	MOCK_rand_bytes_src = (uint8_t *)0;
	MOCK_rand_bytes_offset = 0;
	memset(&response, 0x77, sizeof(response));

	/* Test lower bound. */
	request.header.version = WNG_PROTOCOL_VERSION;
	request.header.type = WNG_MTQ_RESET_TREE;
	request.header.data_length = sizeof(wng_request_reset_tree_t);
	request.data.reset_tree.param_logk = 2;
	request.data.reset_tree.param_h = PARAM_H_MIN - 1;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    WNG_ERR_PARAM_H_INVALID);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_TREE);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_ASSERT(response.header.result_code == WNG_ERR_PARAM_H_INVALID);
	TEST_ASSERT_ARRAY_EQ(response.header.root, merkle_tree.root,
			     sizeof(merkle_tree.root));

	/* Test upper bound. */
	request.data.reset_tree.param_h =
			PARAM_H_MAX(request.data.reset_tree.param_logk) + 1;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
				    WNG_ERR_PARAM_H_INVALID);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_TREE);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_ASSERT(response.header.result_code == WNG_ERR_PARAM_H_INVALID);
	TEST_ASSERT_ARRAY_EQ(response.header.root, merkle_tree.root,
			     sizeof(merkle_tree.root));

	return EC_SUCCESS;
}

static int handle_reset_tree_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memset(&merkle_tree, 0, sizeof(merkle_tree));
	/* This is based on the assumption that the hmac_key and wrap_key are
	 * contiguous and assigned in that order from rand_bytes().
	 */
	MOCK_rand_bytes_src = empty_tree.hmac_key;
	MOCK_rand_bytes_offset = 0;
	memset(&response, 0x77, sizeof(response));

	request.header.version = WNG_PROTOCOL_VERSION;
	request.header.type = WNG_MTQ_RESET_TREE;
	request.header.data_length = sizeof(wng_request_reset_tree_t);
	request.data.reset_tree.param_logk = 2;
	request.data.reset_tree.param_h = 6;

	TEST_ASSERT(wng_handle_request(&merkle_tree, &request, &response) ==
		    EC_SUCCESS);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_TREE);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_ASSERT(response.header.result_code == EC_SUCCESS);
	print_array(&response.header.root, sizeof(response.header.root));
	TEST_ASSERT_ARRAY_EQ(response.header.root, empty_tree.root,
			     sizeof(empty_tree.root));

	TEST_ASSERT_ARRAY_EQ((uint8_t *)&merkle_tree, (uint8_t *)&empty_tree,
			     sizeof(empty_tree) -
			     sizeof(empty_tree.public_key) -
			     sizeof(empty_tree.private_key));

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	/* Test header validation. */
	RUN_TEST(handle_request_version_mismatch);
	RUN_TEST(handle_request_invalid_type);
	RUN_TEST(handle_reset_tree_invalid_length);

	/* Test reset tree. */
	RUN_TEST(handle_reset_tree_param_logk_invalid);
	RUN_TEST(handle_reset_tree_param_h_invalid);
	RUN_TEST(handle_reset_tree_success);

	test_print_result();
}
