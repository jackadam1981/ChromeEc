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
	return EC_ERROR_UNIMPLEMENTED;
}

/******************************************************************************/
/* Test data
 */
const int EMPTY_TREE_PATH_LENGTH = 18;
const merkle_tree_t EMPTY_TREE = {
		2 /* param_logk */,
		6 /* param_h */,
		/* root */
		{0x81, 0xaa, 0xe9, 0xde, 0x93, 0xf4, 0xdf, 0x88,
		 0x18, 0xfa, 0xff, 0xbd, 0xb7, 0x09, 0xc0, 0x86,
		 0x48, 0xdd, 0xcd, 0x35, 0x00, 0xf2, 0x88, 0xd6,
		 0x3f, 0xa6, 0x5e, 0x80, 0x10, 0x19, 0x41, 0x17},
		/* hmac_key */
		{0x96, 0xc6, 0xb1, 0x64, 0xb6, 0xa7, 0xa8, 0x01,
		 0xd5, 0x1d, 0x8e, 0x97, 0x24, 0x86, 0xf8, 0x6f,
		 0xd4, 0x84, 0x0f, 0x95, 0x52, 0x93, 0x8d, 0x7d,
		 0x00, 0xbb, 0xba, 0xc8, 0xed, 0x7f, 0xa4, 0x7a},
		/* wrap_key */
		{0x95, 0xc9, 0x0a, 0xd4, 0xb3, 0x61, 0x1b, 0xcf,
		 0x1b, 0x49, 0x2b, 0xd6, 0x5d, 0xbc, 0x80, 0xa9,
		 0xf4, 0x83, 0xf2, 0x84, 0xd4, 0x04, 0x57, 0x7f,
		 0x02, 0xae, 0x37, 0x64, 0xae, 0xda, 0x71, 0x2a},
		{} /* public_key */,
		{} /* private_key */, };

const leaf_immutable_data_t DEFAULT_IDAT = {
		0x1b10000000000000llu /* label = {0, 1, 2, 3, 0, 1} */,
		/* delay_schedule */
		{{5, 20}, {6, 60}, {7, 300}, {8, 600},
		 {9, 1800}, {10, 3600}, {50, WNG_BLOCK_ATTEMPTS}, {0, 0},
		 {0, 0}, {0, 0}, {0, 0}, {0, 0},
		 {0, 0}, {0, 0}, {0, 0}, {0, 0}, },
		/* low_entropy_secret */
		{0xba, 0xbc, 0x98, 0x9d, 0x97, 0x20, 0xcf, 0xea,
		 0xaa, 0xbd, 0xb2, 0xe3, 0xe0, 0x2c, 0x5c, 0x55,
		 0x06, 0x60, 0x93, 0xbd, 0x07, 0xe2, 0xba, 0x92,
		 0x10, 0x19, 0x24, 0xb1, 0x29, 0x33, 0x5a, 0xe2},
		/* high_entropy_secret */
		{0xe3, 0x46, 0xe3, 0x62, 0x01, 0x5d, 0xfe, 0x0a,
		 0xd3, 0x67, 0xd7, 0xef, 0xab, 0x01, 0xad, 0x0e,
		 0x3a, 0xed, 0xe8, 0x2f, 0x99, 0xd1, 0x2d, 0x13,
		 0x4d, 0x4e, 0xe4, 0x02, 0xbe, 0x71, 0x8e, 0x40},
		/* reset_secret */
		{0x8c, 0x33, 0x8c, 0xa7, 0x0f, 0x81, 0xa4, 0xee,
		 0x24, 0xcd, 0x04, 0x84, 0x9c, 0xa8, 0xfd, 0xdd,
		 0x14, 0xb0, 0xad, 0xe6, 0xb7, 0x6a, 0x10, 0xfc,
		 0x03, 0x22, 0xcb, 0x71, 0x31, 0xd3, 0x74, 0xd6}, };

/******************************************************************************/
/* Helper functions
 */

#define WNG_ES_HELPER(x) case x: return #x;
static const char *wng_error_str(int code)
{
	switch (code) {
	WNG_ES_HELPER(EC_SUCCESS)
	WNG_ES_HELPER(EC_ERROR_UNKNOWN)
	WNG_ES_HELPER(EC_ERROR_UNIMPLEMENTED)
	WNG_ES_HELPER(WNG_ERR_VERSION_MISMATCH)
	WNG_ES_HELPER(WNG_ERR_LENGTH_INVALID)
	WNG_ES_HELPER(WNG_ERR_TYPE_INVALID)
	WNG_ES_HELPER(WNG_ERR_PARAM_LOGK_INVALID)
	WNG_ES_HELPER(WNG_ERR_PARAM_H_INVALID)
	WNG_ES_HELPER(WNG_ERR_DELAY_SCHEDULE_INVALID)
	WNG_ES_HELPER(WNG_ERR_PATH_AUTH_FAILED)
	WNG_ES_HELPER(WNG_ERR_HMAC_AUTH_FAILED)
	WNG_ES_HELPER(WNG_ERR_LOWENT_AUTH_FAILED)
	WNG_ES_HELPER(WNG_ERR_RESET_AUTH_FAILED)
	WNG_ES_HELPER(WNG_ERR_CRYPTO_FAILURE)
	WNG_ES_HELPER(WNG_ERR_RATE_LIMIT_REACHED)
	default:
		return "?";
	}
}

#define TEST_RET_EQ(n, m) \
	do { \
		if (n != m) { \
			ccprintf("%d: ASSERTION failed: %s (%d) != %s (%d)\n", \
			__LINE__, wng_error_str(n), n, wng_error_str(m), m); \
			task_dump_trace(); \
			return EC_ERROR_UNKNOWN; \
		} \
	} while (0)

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

static void compute_empty_path(hash_t (*path)[], uint8_t param_k,
			       param_h_t param_h) __attribute__((unused));
static void compute_empty_path(hash_t (*path)[], uint8_t param_k,
			       param_h_t param_h)
{
	hash_t (*view)[param_h][param_k - 1] = (void *)path;
	hash_t temp_a = {};
	hash_t temp_b;
	param_h_t hx = 0;
	uint8_t kx = 0;

	memset(&(*view), 0, sizeof((*view)));
	ccprintf("sizeof((*view)[hx]) == %d\n", sizeof((*view)[hx]));
	if (param_h == 1)
		return;
	print_array(&temp_a, sizeof(temp_a));

	for (hx = 0; hx < param_h - 1; ++hx) {
		/* Case temp_a -> temp_b */
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_a, &temp_b);
		print_array(&temp_b, sizeof(temp_b));
		for (kx = 0; kx < param_k - 1; ++kx)
			memcpy((*view)[hx + 1][kx], temp_b, sizeof(temp_b));

		/* Unroll loop to alternate buffers. */
		++hx;
		if (hx >= param_h - 1)
			break;

		/* Case temp_b -> temp_a */
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_b, &temp_a);
		print_array(&temp_a, sizeof(temp_a));
		for (kx = 0; kx < param_k - 1; ++kx)
			memcpy((*view)[hx + 1][kx], temp_a, sizeof(temp_a));
	}
	if (hx & 1) {
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_b, &temp_a);
		print_array(&temp_a, sizeof(temp_a));
	} else {
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_a, &temp_b);
		print_array(&temp_b, sizeof(temp_b));
	}
}

static void setup_default_reset_tree_request(wng_request_t *request)
{
	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_RESET_TREE;
	request->header.data_length = sizeof(wng_request_reset_tree_t);

	request->data.reset_tree.param_logk = 2; /* k = 4 */
	request->data.reset_tree.param_h = 6; /* L = 12 */
}

static void setup_default_empty_path(hash_t (*hashes)[])
{
	uint8_t param_k = 1 << EMPTY_TREE.param_logk;
	hash_t (*view)[EMPTY_TREE.param_h][(1 << EMPTY_TREE.param_logk) - 1] =
			(void *)hashes;
	const hash_t level_hashes[5] = {
			/* Values for level 5 are all 0 for empty. */
			/*SHA256 for level 5, values for level 4*/
			{0x38, 0x72, 0x3a, 0x2e, 0x5e, 0x8a, 0x17, 0xaa,
			 0x79, 0x50, 0xdc, 0x00, 0x82, 0x09, 0x94, 0x4e,
			 0x89, 0x8f, 0x69, 0xa7, 0xbd, 0x10, 0xa2, 0x3c,
			 0x83, 0x9d, 0x34, 0x1e, 0x93, 0x5f, 0xd5, 0xca},
			/*SHA256 for level 4, values for level 3*/
			{0xfe, 0xc1, 0x2b, 0x09, 0x33, 0x31, 0x28, 0x34,
			 0x79, 0x1f, 0x07, 0x64, 0x1a, 0xed, 0x30, 0x53,
			 0x11, 0x1f, 0x15, 0x3e, 0x1e, 0x3e, 0xd1, 0xf0,
			 0xcd, 0x16, 0xcb, 0x39, 0x25, 0xfd, 0x5f, 0x84},
			/*SHA256 for level 3, values for level 2*/
			{0xb6, 0xd4, 0x9c, 0x89, 0x76, 0x45, 0x9c, 0xe9,
			 0x9c, 0x0b, 0xad, 0x5d, 0x71, 0xdf, 0x92, 0x77,
			 0xf6, 0x82, 0x62, 0x63, 0x81, 0x9f, 0xc9, 0x2f,
			 0x61, 0x9c, 0x29, 0x67, 0x52, 0x37, 0x01, 0x51},
			/*SHA256 for level 2, values for level 1*/
			{0x87, 0xeb, 0x61, 0x6b, 0x2c, 0x42, 0x07, 0x5e,
			 0x70, 0x2d, 0x48, 0x49, 0xf2, 0xe0, 0x13, 0x11,
			 0xc4, 0xe6, 0x98, 0xfa, 0x22, 0x7e, 0x65, 0xc6,
			 0x66, 0x33, 0x6b, 0xb6, 0xd7, 0xb9, 0x45, 0xfa},
			/*SHA256 for level 1, values for level 0*/
			{0x80, 0x91, 0x04, 0x3f, 0x6c, 0x29, 0x06, 0x35,
			 0x86, 0x99, 0x21, 0x88, 0x1f, 0xd9, 0xae, 0xb8,
			 0x35, 0x94, 0x26, 0x19, 0x64, 0x68, 0x4f, 0x4f,
			 0x4c, 0x66, 0x13, 0xa9, 0x66, 0x69, 0x25, 0x0e},};
	uint8_t hx;
	uint8_t kx;

	memset(*(*view)[0], 0, sizeof((*view)[0]));
	for (hx = 1; hx < EMPTY_TREE.param_h; ++hx) {
		for (kx = 0; kx < param_k - 1; ++kx) {
			memcpy((*view)[hx][kx], level_hashes[hx - 1],
			       sizeof((*view)[0][0]));
		}
	}
}

static void setup_default_insert_leaf_request(wng_request_t *request)
{

	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_INSERT_LEAF;
	request->header.data_length = sizeof(wng_request_insert_leaf_t) +
			get_path_length(&EMPTY_TREE);

	memcpy(&request->data.insert_leaf.idat,
	       &DEFAULT_IDAT, sizeof(DEFAULT_IDAT));
	setup_default_empty_path(&request->data.insert_leaf.path_hashes);
}

/******************************************************************************/
/* Basic operation test cases.
 */

static int get_path_length_test(void)
{
	merkle_tree_t merkle_tree;

	memcpy(&merkle_tree, &EMPTY_TREE, sizeof(merkle_tree));

	TEST_ASSERT(get_path_length(&merkle_tree) == EMPTY_TREE_PATH_LENGTH);
	return EC_SUCCESS;
}

static int get_index_test(void)
{
	merkle_tree_t merkle_tree;
	size_t x;

	memcpy(&merkle_tree, &EMPTY_TREE, sizeof(merkle_tree));

	{
		const index_t expected_indeces[] = {0, 1, 2, 3, 0, 1};
		const label_t label = 0x1b10000000000000llu;

		for (x = 0; x < ARRAY_SIZE(expected_indeces); ++x) {
			TEST_ASSERT(get_index(&merkle_tree, label, x) ==
				    expected_indeces[x]);
		}
	}

	merkle_tree.param_logk = 8;
	merkle_tree.param_h = 8;

	{
		const index_t expected_indeces[] = {0x80, 0x00, 0x81, 0x01,
						    0x82, 0x02, 0x83, 0x03};
		const label_t label = 0x8000810182028303llu;

		for (x = 0; x < ARRAY_SIZE(expected_indeces); ++x) {
			TEST_ASSERT(get_index(&merkle_tree, label, x) ==
				    expected_indeces[x]);
		}
	}

	return EC_SUCCESS;
}

/******************************************************************************/
/* Header validation test cases.
 */

static int TEST_ASSERT_ERROR_MSG(merkle_tree_t *merkle_tree,
				  wng_response_t *response, uint32_t code)
{
	TEST_ASSERT(response->header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response->header.type == WNG_MT_ERROR_MSG);
	TEST_ASSERT(response->header.data_length == 0);
	TEST_RET_EQ(response->header.result_code, code);
	TEST_ASSERT_ARRAY_EQ(response->header.root, merkle_tree->root,
			     sizeof(response->header.root));
	return EC_SUCCESS;
}

static int handle_request_version_mismatch(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memcpy(&merkle_tree, &EMPTY_TREE, sizeof(merkle_tree));
	setup_default_reset_tree_request(&request);
	memset(&response, 0x77, sizeof(response));

	request.header.version = WNG_PROTOCOL_VERSION + 1;

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

	memcpy(&merkle_tree, &EMPTY_TREE, sizeof(merkle_tree));
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

	memcpy(&merkle_tree, &EMPTY_TREE, sizeof(merkle_tree));
	setup_default_reset_tree_request(&request);
	memset(&response, 0x77, sizeof(response));

	request.header.data_length = sizeof(wng_request_reset_tree_t) + 1;

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
	setup_default_reset_tree_request(&request);
	memset(&response, 0x77, sizeof(response));

	/* Test lower bound. */
	request.data.reset_tree.param_logk = PARAM_LOGK_MIN - 1;

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
	MOCK_rand_bytes_src = EMPTY_TREE.hmac_key;
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
	TEST_ASSERT_ARRAY_EQ(response.header.root, EMPTY_TREE.root,
			     sizeof(EMPTY_TREE.root));

	TEST_ASSERT_ARRAY_EQ((uint8_t *)&merkle_tree, (uint8_t *)&EMPTY_TREE,
			     sizeof(EMPTY_TREE) -
			     sizeof(EMPTY_TREE.public_key) -
			     sizeof(EMPTY_TREE.private_key));

	return EC_SUCCESS;
}

/******************************************************************************/
/* Insert leaf test cases.
 */

static int handle_insert_leaf_success(void) __attribute((unused));
static int handle_insert_leaf_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	memcpy(&merkle_tree, &EMPTY_TREE, sizeof(merkle_tree));
	setup_default_insert_leaf_request(&request);
	memset(&response, 0x77, sizeof(response));

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	ccprintf("CODE: %d\n", response.header.result_code);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_INSERT_LEAF);
	TEST_ASSERT(response.header.data_length ==
				    sizeof(response.data.insert_leaf));
	TEST_ASSERT(response.header.result_code == EC_SUCCESS);
	print_array(&response.header.root, sizeof(response.header.root));

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	/* Test basic operations. */
	RUN_TEST(get_path_length_test);
	RUN_TEST(get_index_test);

	/* Test header validation. */
	RUN_TEST(handle_request_version_mismatch);
	RUN_TEST(handle_request_invalid_type);
	RUN_TEST(handle_reset_tree_invalid_length);

	/* Test reset tree. */
	RUN_TEST(handle_reset_tree_param_logk_invalid);
	RUN_TEST(handle_reset_tree_param_h_invalid);
	RUN_TEST(handle_reset_tree_success);

	/* Test insert leaf. */
	RUN_TEST(handle_insert_leaf_success);

	test_print_result();
}
