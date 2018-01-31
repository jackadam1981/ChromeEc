/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <weaver_ng.h>

#include <sha256.h>
#include <stdint.h>
#include <string.h>
#include <timer.h>
#include <util.h>
#include "test_util.h"

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

const uint8_t DEFAULT_IV[] = {
		0xaa, 0x65, 0x97, 0xc7, 0x02, 0x23, 0xb8, 0xdc,
		0xb3, 0x55, 0xca, 0x3a, 0xab, 0xd0, 0x03, 0x90, };

const uint8_t EMPTY_HMAC[32] = {};

/* This is not the actual hmac. */
const uint8_t DEFAULT_HMAC[] = {
		0x87, 0x7e, 0xe2, 0xb2, 0x60, 0xeb, 0xf3, 0x4b,
		0x80, 0x3e, 0xca, 0xcb, 0xe6, 0x24, 0x21, 0x86,
		0xd9, 0xe3, 0x91, 0xf7, 0x2d, 0x16, 0x59, 0xd8,
		0x0f, 0x37, 0x0a, 0xf4, 0x64, 0x19, 0x44, 0xe7, };

const uint8_t ROOT_WITH_DEFAULT_HMAC[] = {
		0x24, 0xad, 0xe4, 0xad, 0xf2, 0xdc, 0x40, 0x26,
		0x15, 0x03, 0x16, 0x6f, 0x3c, 0x32, 0x05, 0x99,
		0xf8, 0x25, 0x22, 0x92, 0xb9, 0xc7, 0xcd, 0x18,
		0x37, 0xc2, 0xf2, 0x72, 0x31, 0xdd, 0xc4, 0xaf, };

/******************************************************************************/
/* Config Variables and defines for Mocks.
 */

uint32_t MOCK_restart_count;

const uint8_t *MOCK_rand_bytes_src;
size_t MOCK_rand_bytes_offset;
size_t MOCK_rand_bytes_len;

void (*MOCK_hash_update_cb)(const void *data, size_t len);
static void auth_hash_update_cb(const void *data, size_t len);

const uint8_t *MOCK_hmac;

#define MOCK_AES_XOR_BYTE(b) ((uint8_t)(0x77 + (b & 15)))
int MOCK_aes_fail;

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
	WNG_ES_HELPER(WNG_ERR_LABEL_INVALID)
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

#define TEST_ASRT_NORET(n) \
	do { \
		if (!(n)) { \
			int x = 0;\
			ccprintf("%d: ASSERTION failed: %s\n", __LINE__, #n); \
			task_dump_trace(); \
			x = 1 / x; \
		} \
	} while (0)

/* For debugging and generating test data. */
void print_array(const uint8_t *data, size_t n) __attribute__ ((unused));
void print_array(const uint8_t *data, size_t n)
{
	size_t x;

	if (n > 0) {
		ccprintf("uint8_t data[] = {");
		for (x = 0; x < n - 1; ++x) {
			if ((x & 7) != 7)
				ccprintf("0x%02x, ", data[x]);
			else
				ccprintf("0x%02x,\n", data[x]);
		}
		ccprintf("0x%02x};\n", data[x]);
	}
}

/* For exporting structs. This is useful for validating the results of crypto
 * operations.
 */
void print_hex(const uint8_t *data, size_t n) __attribute__ ((unused));
void print_hex(const uint8_t *data, size_t n)
{
	size_t x;

	for (x = 0; x < n; ++x)
		ccprintf("%02x ", data[x]);
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
	if (param_h == 1)
		return;
	print_array(temp_a, sizeof(temp_a));

	for (hx = 0; hx < param_h - 1; ++hx) {
		/* Case temp_a -> temp_b */
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_a, &temp_b);
		print_array(temp_b, sizeof(temp_b));
		for (kx = 0; kx < param_k - 1; ++kx)
			memcpy((*view)[hx + 1][kx], temp_b, sizeof(temp_b));

		/* Unroll loop to alternate buffers. */
		++hx;
		if (hx >= param_h - 1)
			break;

		/* Case temp_b -> temp_a */
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_b, &temp_a);
		print_array(temp_a, sizeof(temp_a));
		for (kx = 0; kx < param_k - 1; ++kx)
			memcpy((*view)[hx + 1][kx], temp_a, sizeof(temp_a));
	}
	if (hx & 1) {
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_b, &temp_a);
		print_array(temp_a, sizeof(temp_a));
	} else {
		compute_hash((const hash_t (*)[])&(*view)[hx], param_k - 1, 0,
			     (const hash_t *)&temp_a, &temp_b);
		print_array(temp_b, sizeof(temp_b));
	}
}

static void setup_reset_tree_defaults(merkle_tree_t *merkle_tree,
				      wng_request_t *request,
				      wng_response_t *response)
{
	memcpy(merkle_tree, &EMPTY_TREE, sizeof(*merkle_tree));

	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_RESET_TREE;
	request->header.data_length = sizeof(wng_request_reset_tree_t);

	request->data.reset_tree.param_logk = 2; /* k = 4 */
	request->data.reset_tree.param_h = 6; /* L = 12 */

	memset(response, 0x77, sizeof(*response));

	/* This is based on the assumption that the hmac_key and wrap_key are
	 * contiguous and assigned in that order from rand_bytes().
	 */
	MOCK_rand_bytes_src = (uint8_t *)EMPTY_TREE.hmac_key;
	MOCK_rand_bytes_offset = 0;
	MOCK_rand_bytes_len = sizeof(EMPTY_TREE.hmac_key) +
			sizeof(EMPTY_TREE.wrap_key);
}

static void setup_default_empty_path(hash_t (*hashes)[])
		__attribute__((unused));
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

static void setup_insert_leaf_defaults(merkle_tree_t *merkle_tree,
				       wng_request_t *request,
				       wng_response_t *response)
{
	memcpy(merkle_tree, &EMPTY_TREE, sizeof(*merkle_tree));

	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_INSERT_LEAF;
	request->header.data_length = sizeof(wng_request_insert_leaf_t) +
			get_path_length(&EMPTY_TREE);

	memcpy(&request->data.insert_leaf.idat,
	       &DEFAULT_IDAT, sizeof(DEFAULT_IDAT));
	setup_default_empty_path(&request->data.insert_leaf.path_hashes);
//	compute_empty_path(&request->data.insert_leaf.path_hashes,
//			   1 << EMPTY_TREE.param_logk, EMPTY_TREE.param_h);

	memset(response, 0x77, sizeof(*response));

	MOCK_rand_bytes_src = DEFAULT_IV;
	MOCK_rand_bytes_offset = 0;
	MOCK_rand_bytes_len = sizeof(DEFAULT_IV);
	MOCK_hash_update_cb = 0;
	MOCK_hmac = DEFAULT_HMAC;
	MOCK_aes_fail = 0;
}

static void setup_remove_leaf_defaults(merkle_tree_t *merkle_tree,
				       wng_request_t *request,
				       wng_response_t *response)
{
	memcpy(merkle_tree, &EMPTY_TREE, sizeof(*merkle_tree));
	memcpy(merkle_tree->root, ROOT_WITH_DEFAULT_HMAC,
	       sizeof(ROOT_WITH_DEFAULT_HMAC));

	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_REMOVE_LEAF;
	request->header.data_length = sizeof(wng_request_remove_leaf_t) +
				      get_path_length(&EMPTY_TREE);

	request->data.remove_leaf.leaf_location = DEFAULT_IDAT.label;
	memcpy(request->data.remove_leaf.leaf_hmac, DEFAULT_HMAC,
	       sizeof(request->data.remove_leaf.leaf_hmac));
	setup_default_empty_path(&request->data.remove_leaf.path_hashes);

	memset(response, 0x77, sizeof(*response));
}

static void setup_try_auth_defaults(const leaf_data_t *leaf_data,
				    merkle_tree_t *merkle_tree,
				    wng_request_t *request,
				    wng_response_t *response)
{
	memcpy(merkle_tree, &EMPTY_TREE, sizeof(*merkle_tree));
	if (leaf_data->attempt_count != 6 && leaf_data->attempt_count != 10) {
		memcpy(merkle_tree->root, ROOT_WITH_DEFAULT_HMAC,
		       sizeof(ROOT_WITH_DEFAULT_HMAC));
		memcpy(request->data.try_auth.wrapped_leaf_data.hmac,
		       DEFAULT_HMAC,
		       sizeof(request->data.try_auth.wrapped_leaf_data.hmac));

		/* Gets overwritten by auth_hash_update_cb. */
		MOCK_hmac = DEFAULT_HMAC;
	} else {
		memcpy(request->data.try_auth.wrapped_leaf_data.hmac,
		       EMPTY_HMAC,
		       sizeof(request->data.try_auth.wrapped_leaf_data.hmac));

		/* Gets overwritten by auth_hash_update_cb. */
		MOCK_hmac = EMPTY_HMAC;
	}

	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_TRY_AUTH;
	request->header.data_length = sizeof(wng_request_try_auth_t) +
				      get_path_length(&EMPTY_TREE);

	request->data.try_auth.leaf_location = DEFAULT_IDAT.label;
	memcpy(request->data.try_auth.low_entropy_secret,
	       DEFAULT_IDAT.low_entropy_secret,
	       sizeof(request->data.try_auth.low_entropy_secret));
	memcpy(request->data.try_auth.wrapped_leaf_data.iv, DEFAULT_IV,
	       sizeof(request->data.try_auth.wrapped_leaf_data.iv));
	DCRYPTO_aes_ctr(request->data.try_auth.wrapped_leaf_data.cipher_text,
			EMPTY_TREE.wrap_key, sizeof(EMPTY_TREE.wrap_key) * 8,
			DEFAULT_IV, (const uint8_t *)leaf_data,
			sizeof(*leaf_data));
	setup_default_empty_path(&request->data.try_auth.path_hashes);

	memset(response, 0x77, sizeof(*response));

	MOCK_update_timestamp.boot_count = 0;
	MOCK_update_timestamp.timer_value = 0;
	MOCK_rand_bytes_src = DEFAULT_IV;
	MOCK_rand_bytes_offset = 0;
	MOCK_rand_bytes_len = sizeof(DEFAULT_IV);
	MOCK_hash_update_cb = auth_hash_update_cb;
	MOCK_aes_fail = 0;
}

static void setup_reset_auth_defaults(merkle_tree_t *merkle_tree,
				      wng_request_t *request,
				      wng_response_t *response)
{
	leaf_data_t leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	leaf_data.attempt_count = 6;

	memcpy(merkle_tree, &EMPTY_TREE, sizeof(*merkle_tree));

	request->header.version = WNG_PROTOCOL_VERSION;
	request->header.type = WNG_MTQ_RESET_AUTH;
	request->header.data_length = sizeof(wng_request_reset_auth_t) +
				      get_path_length(&EMPTY_TREE);

	request->data.reset_auth.leaf_location = DEFAULT_IDAT.label;
	memcpy(request->data.reset_auth.reset_secret,
	       DEFAULT_IDAT.reset_secret,
	       sizeof(request->data.reset_auth.reset_secret));
	memcpy(request->data.reset_auth.wrapped_leaf_data.hmac,
	       EMPTY_HMAC,
	       sizeof(request->data.reset_auth.wrapped_leaf_data.hmac));
	memcpy(request->data.reset_auth.wrapped_leaf_data.iv, DEFAULT_IV,
	       sizeof(request->data.reset_auth.wrapped_leaf_data.iv));
	DCRYPTO_aes_ctr(request->data.reset_auth.wrapped_leaf_data.cipher_text,
			EMPTY_TREE.wrap_key, sizeof(EMPTY_TREE.wrap_key) * 8,
			DEFAULT_IV, (const uint8_t *)&leaf_data,
			sizeof(leaf_data));
	setup_default_empty_path(&request->data.reset_auth.path_hashes);

	memset(response, 0x77, sizeof(*response));

	MOCK_rand_bytes_src = DEFAULT_IV;
	MOCK_rand_bytes_offset = 0;
	MOCK_rand_bytes_len = sizeof(DEFAULT_IV);
	MOCK_hash_update_cb = auth_hash_update_cb;
	MOCK_hmac = EMPTY_HMAC; /* Gets overwritten by auth_hash_update_cb. */
	MOCK_aes_fail = 0;
}

static int test_handle_error_msg(merkle_tree_t *merkle_tree,
				 const wng_request_t *request,
				 wng_response_t *response, uint32_t code)
{
	TEST_RET_EQ(wng_handle_request(merkle_tree, request, response), code);

	TEST_ASSERT(response->header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response->header.type == WNG_MT_ERROR_MSG);
	TEST_ASSERT(response->header.data_length == 0);
	TEST_RET_EQ(response->header.result_code, code);
	TEST_ASSERT_ARRAY_EQ(response->header.root, merkle_tree->root,
			     sizeof(response->header.root));
	return EC_SUCCESS;
}

/* Changes MOCK_hmac in a deterministic way based on the contents of the data
 * with the goal of making it easier to catch bugs in the handling of try_auth
 * and reset_auth requests.
 */
static void auth_hash_update_cb(const void *data, size_t len)
{
	leaf_data_t leaf_data = {};
	int aes_fail = MOCK_aes_fail;

	TEST_ASRT_NORET(len == sizeof(leaf_data));
	MOCK_aes_fail = 0;
	DCRYPTO_aes_ctr((uint8_t *)&leaf_data, EMPTY_TREE.wrap_key,
			sizeof(EMPTY_TREE.wrap_key) * 8, DEFAULT_IV, data,
			sizeof(leaf_data));
	MOCK_aes_fail = aes_fail;

	switch (leaf_data.attempt_count) {
	case 10:
	case 6:
		MOCK_hmac = EMPTY_HMAC;
		break;
	default:
		MOCK_hmac = DEFAULT_HMAC;
		break;
	}
}

/******************************************************************************/
/* Mock implementations of TPM, TRNG, and Dcrypto functionality.
 */

uint32_t get_restart_count(void)
{
	return MOCK_restart_count;
}

void rand_bytes(void *buffer, size_t len)
{
	if (!MOCK_rand_bytes_src)
		return;

	TEST_ASRT_NORET(len <= MOCK_rand_bytes_len - MOCK_rand_bytes_offset);

	memcpy(buffer, MOCK_rand_bytes_src + MOCK_rand_bytes_offset, len);
	MOCK_rand_bytes_offset += len;
	if (MOCK_rand_bytes_len == MOCK_rand_bytes_offset)
		MOCK_rand_bytes_offset = 0;
}

void HASH_update(struct HASH_CTX *ctx, const void *data, size_t len)
{
	TEST_ASRT_NORET(len ==
			     sizeof(((wrapped_leaf_data_t *)0)[0].cipher_text));

	if (MOCK_hash_update_cb)
		MOCK_hash_update_cb(data, len);
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
	TEST_ASRT_NORET(len == sizeof(EMPTY_TREE.hmac_key));
	TEST_ASRT_NORET(memcmp(key, EMPTY_TREE.hmac_key,
			       sizeof(EMPTY_TREE.hmac_key)) == 0);
}

const uint8_t *DCRYPTO_HMAC_final(LITE_HMAC_CTX *ctx)
{
	return MOCK_hmac;
}

/* Perform a symmetric transformation of the data to simulate AES without
 * requiring a full AES-CTR implementation.
 */
int DCRYPTO_aes_ctr(uint8_t *out, const uint8_t *key, uint32_t key_bits,
		    const uint8_t *iv, const uint8_t *in, size_t in_len)
{
	size_t x;

	if (MOCK_aes_fail) {
		--MOCK_aes_fail;
		return EC_ERROR_UNKNOWN;
	}

	TEST_ASSERT(key_bits == 256);
	TEST_ASSERT_ARRAY_EQ(key, EMPTY_TREE.wrap_key,
			     sizeof(EMPTY_TREE.wrap_key));
	TEST_ASSERT_ARRAY_EQ(iv, DEFAULT_IV, sizeof(DEFAULT_IV));
	TEST_ASSERT(in_len == sizeof(leaf_data_t));

	for (x = 0; x < in_len; ++x)
		out[x] = MOCK_AES_XOR_BYTE(x) ^ in[x];
	return EC_SUCCESS;
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

static int handle_request_version_mismatch(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_tree_defaults(&merkle_tree, &request, &response);

	request.header.version = WNG_PROTOCOL_VERSION + 1;

	return test_handle_error_msg(&merkle_tree, &request, &response,
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

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_TYPE_INVALID);
}

/******************************************************************************/
/* Reset Tree test cases.
 */

static int handle_reset_tree_invalid_length(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_tree_defaults(&merkle_tree, &request, &response);

	++request.header.data_length;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LENGTH_INVALID);
}

static int handle_reset_tree_param_logk_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_tree_defaults(&merkle_tree, &request, &response);

	/* Test lower bound. */
	request.data.reset_tree.param_logk = PARAM_LOGK_MIN - 1;

	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_PARAM_LOGK_INVALID),
		    EC_SUCCESS);

	/* Test upper bound. */
	request.data.reset_tree.param_logk = PARAM_LOGK_MAX + 1;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_PARAM_LOGK_INVALID);
}

static int handle_reset_tree_param_h_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_tree_defaults(&merkle_tree, &request, &response);

	/* Test lower bound. */
	request.data.reset_tree.param_h = PARAM_H_MIN - 1;

	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_PARAM_H_INVALID),
		    EC_SUCCESS);

	/* Test upper bound. */
	request.data.reset_tree.param_h =
			PARAM_H_MAX(request.data.reset_tree.param_logk) + 1;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_PARAM_H_INVALID);
}

static int handle_reset_tree_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_tree_defaults(&merkle_tree, &request, &response);

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_TREE);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_RET_EQ(response.header.result_code, EC_SUCCESS);
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

static int handle_insert_leaf_invalid_length(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_insert_leaf_defaults(&merkle_tree, &request, &response);

	++request.header.data_length;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LENGTH_INVALID);
}

static int handle_insert_leaf_label_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_insert_leaf_defaults(&merkle_tree, &request, &response);

	request.data.insert_leaf.idat.label |= 3;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LABEL_INVALID);
}

static int handle_insert_leaf_delay_schedule_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	delay_schedule_t *ds = &request.data.insert_leaf.idat.delay_schedule;

	setup_insert_leaf_defaults(&merkle_tree, &request, &response);

	/* Non-increasing attempt_count. */
	(*ds)[1].attempt_count = 0;
	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_DELAY_SCHEDULE_INVALID),
		    EC_SUCCESS);
	(*ds)[1].attempt_count = DEFAULT_IDAT.delay_schedule[1].attempt_count;

	/* Non-increasing time_diff. */
	(*ds)[1].time_diff = 0;
	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_DELAY_SCHEDULE_INVALID),
		    EC_SUCCESS);
	(*ds)[1].time_diff = DEFAULT_IDAT.delay_schedule[1].time_diff;

	/* attempt_count noise. */
	(*ds)[14].attempt_count = 99;
	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_DELAY_SCHEDULE_INVALID),
		    EC_SUCCESS);
	(*ds)[14].attempt_count = DEFAULT_IDAT.delay_schedule[14].attempt_count;

	/* time_diff noise. */
	(*ds)[14].time_diff = 99;
	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_DELAY_SCHEDULE_INVALID),
		    EC_SUCCESS);

	/* Empty delay_schedule. */
	memset(&(*ds)[0], 0, sizeof(*ds));
	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_DELAY_SCHEDULE_INVALID);
}

static int handle_insert_leaf_path_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_insert_leaf_defaults(&merkle_tree, &request, &response);

	request.data.insert_leaf.path_hashes[0][0] ^= 0xff;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_PATH_AUTH_FAILED);
}

static int handle_insert_leaf_crypto_failure(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_insert_leaf_defaults(&merkle_tree, &request, &response);

	MOCK_aes_fail = 1;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_CRYPTO_FAILURE);
}

static int handle_insert_leaf_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	size_t x;
	leaf_data_t leaf_data;
	uint8_t *plain_text = (uint8_t *)&leaf_data;
	uint8_t *cipher_text = (uint8_t *)&response.data.insert_leaf
			.wrapped_leaf_data.cipher_text;

	setup_insert_leaf_defaults(&merkle_tree, &request, &response);

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_INSERT_LEAF);
	TEST_ASSERT(response.header.data_length ==
				    sizeof(response.data.insert_leaf));
	TEST_RET_EQ(response.header.result_code, EC_SUCCESS);

	memset(&leaf_data, 0, sizeof(leaf_data));
	leaf_data.version = WNG_STORAGE_VERSION;
	memcpy(&leaf_data.idat, &request.data.insert_leaf.idat,
	       sizeof(leaf_data.idat));

	TEST_ASSERT_ARRAY_EQ(response.header.root, ROOT_WITH_DEFAULT_HMAC,
			     sizeof(ROOT_WITH_DEFAULT_HMAC));
	TEST_ASSERT_ARRAY_EQ(response.data.insert_leaf.wrapped_leaf_data.hmac,
			     DEFAULT_HMAC, sizeof(DEFAULT_HMAC));
	for (x = 0; x < sizeof(leaf_data); ++x)
		TEST_ASSERT(plain_text[x] == (cipher_text[x] ^
					    MOCK_AES_XOR_BYTE(x)));

	return EC_SUCCESS;
}

/******************************************************************************/
/* Remove leaf test cases.
 */

static int handle_remove_leaf_invalid_length(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_remove_leaf_defaults(&merkle_tree, &request, &response);

	++request.header.data_length;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LENGTH_INVALID);
}

static int handle_remove_leaf_label_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_remove_leaf_defaults(&merkle_tree, &request, &response);

	request.data.remove_leaf.leaf_location |= 3;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LABEL_INVALID);
}

static int handle_remove_leaf_path_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_remove_leaf_defaults(&merkle_tree, &request, &response);

	request.data.remove_leaf.path_hashes[0][0] ^= 0xff;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_PATH_AUTH_FAILED);
}

static int handle_remove_leaf_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_remove_leaf_defaults(&merkle_tree, &request, &response);

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_REMOVE_LEAF);
	TEST_ASSERT(response.header.data_length == 0);
	TEST_RET_EQ(response.header.result_code, EC_SUCCESS);

	TEST_ASSERT_ARRAY_EQ(response.header.root, EMPTY_TREE.root,
			     sizeof(EMPTY_TREE.root));

	return EC_SUCCESS;
}

/******************************************************************************/
/* Try auth test cases.
 */

static int handle_try_auth_invalid_length(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	++request.header.data_length;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LENGTH_INVALID);
}

static int handle_try_auth_label_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	request.data.remove_leaf.leaf_location |= 3;

	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_LABEL_INVALID),
		    EC_SUCCESS);

	leaf_data.idat.label |= 3;
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LABEL_INVALID);
}

static int handle_try_auth_path_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	request.data.try_auth.path_hashes[0][0] ^= 0xff;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_PATH_AUTH_FAILED);
}

static int handle_try_auth_hmac_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	MOCK_hash_update_cb = 0;
	MOCK_hmac = EMPTY_TREE.root;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_HMAC_AUTH_FAILED);
}

static int handle_try_auth_crypto_failure(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	MOCK_aes_fail = 1;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_CRYPTO_FAILURE);
}

static int handle_try_auth_rate_limit_reached(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	/* Test WNG_BLOCK_ATTEMPTS. */
	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	leaf_data.attempt_count = 51;
	MOCK_update_timestamp.boot_count = 1;
	MOCK_update_timestamp.timer_value = 7200llu * SECOND;
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_RATE_LIMIT_REACHED),
		    EC_SUCCESS);

	/* Test same boot_count case. */
	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	leaf_data.attempt_count = 10;
	leaf_data.timestamp.boot_count = 0;
	leaf_data.timestamp.timer_value = 7200llu * SECOND;
	MOCK_update_timestamp.boot_count = 0;
	MOCK_update_timestamp.timer_value = leaf_data.timestamp.timer_value +
			3599llu * SECOND;
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_RATE_LIMIT_REACHED),
		    EC_SUCCESS);

	/* Test boot_count + 1 case. */
	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	leaf_data.attempt_count = 10;
	leaf_data.timestamp.boot_count = 0;
	leaf_data.timestamp.timer_value = 7200llu * SECOND;
	MOCK_update_timestamp.boot_count = 1;
	MOCK_update_timestamp.timer_value = 3599llu * SECOND;
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_RATE_LIMIT_REACHED);
}

static int handle_try_auth_lowent_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};
	leaf_data_t ret_leaf_data = {};

	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	leaf_data.attempt_count = 5;
	leaf_data.idat.low_entropy_secret[
			sizeof(leaf_data.idat.low_entropy_secret) - 1] =
				~leaf_data.idat.low_entropy_secret[
				 sizeof(leaf_data.idat.low_entropy_secret) - 1];
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);
	MOCK_update_timestamp.boot_count = 1;
	MOCK_update_timestamp.timer_value = 65 * SECOND;

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    WNG_ERR_LOWENT_AUTH_FAILED);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_TRY_AUTH);
	TEST_ASSERT(response.header.data_length ==
				    sizeof(wng_response_try_auth_t));
	TEST_RET_EQ(response.header.result_code, WNG_ERR_LOWENT_AUTH_FAILED);

	TEST_ASSERT(memcmp(response.header.root, merkle_tree.root,
			   sizeof(merkle_tree.root)) != 0);

	TEST_ASSERT_ARRAY_EQ(response.data.try_auth.wrapped_leaf_data.hmac,
			     EMPTY_HMAC, sizeof(EMPTY_HMAC));
	TEST_ASSERT_ARRAY_EQ(response.data.try_auth.wrapped_leaf_data.iv,
			     DEFAULT_IV, sizeof(DEFAULT_IV));
	DCRYPTO_aes_ctr((uint8_t *)&ret_leaf_data, EMPTY_TREE.wrap_key,
			sizeof(EMPTY_TREE.wrap_key) * 8, DEFAULT_IV,
			response.data.try_auth.wrapped_leaf_data.cipher_text,
			sizeof(ret_leaf_data));
	TEST_ASSERT_ARRAY_EQ((uint8_t *)&ret_leaf_data.idat,
			     (uint8_t *)&leaf_data.idat, sizeof(DEFAULT_IDAT));
	TEST_ASSERT(ret_leaf_data.attempt_count == leaf_data.attempt_count + 1);
	TEST_ASSERT(ret_leaf_data.timestamp.boot_count == 1);
	TEST_ASSERT(ret_leaf_data.timestamp.timer_value == 65 * SECOND);
	return EC_SUCCESS;
}

static int handle_try_auth_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	/* Test same boot_count case. */
	memcpy(&leaf_data.idat, &DEFAULT_IDAT, sizeof(leaf_data.idat));
	leaf_data.attempt_count = 6;
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);
	MOCK_update_timestamp.boot_count = 0;
	MOCK_update_timestamp.timer_value = 65 * SECOND;

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	TEST_ASSERT(memcmp(response.header.root, merkle_tree.root,
			   sizeof(merkle_tree.root)) != 0);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_TRY_AUTH);
	TEST_ASSERT(response.header.data_length ==
				    sizeof(wng_response_try_auth_t));
	TEST_RET_EQ(response.header.result_code, EC_SUCCESS);

	TEST_ASSERT_ARRAY_EQ(response.header.root, ROOT_WITH_DEFAULT_HMAC,
			     sizeof(ROOT_WITH_DEFAULT_HMAC));

	TEST_ASSERT_ARRAY_EQ(response.data.try_auth.wrapped_leaf_data.hmac,
			     DEFAULT_HMAC, sizeof(DEFAULT_HMAC));
	TEST_ASSERT_ARRAY_EQ(response.data.try_auth.wrapped_leaf_data.iv,
			     DEFAULT_IV, sizeof(DEFAULT_IV));
	DCRYPTO_aes_ctr((uint8_t *)&leaf_data, EMPTY_TREE.wrap_key,
			sizeof(EMPTY_TREE.wrap_key) * 8, DEFAULT_IV,
			response.data.try_auth.wrapped_leaf_data.cipher_text,
			sizeof(leaf_data));
	TEST_ASSERT_ARRAY_EQ((uint8_t *)&leaf_data.idat,
			     (uint8_t *)&DEFAULT_IDAT, sizeof(DEFAULT_IDAT));
	TEST_ASSERT(leaf_data.attempt_count == 0);

	/* Test boot_count + 1 case. */
	leaf_data.attempt_count = 6;
	leaf_data.timestamp.boot_count = 0;
	leaf_data.timestamp.timer_value = 7200llu * SECOND;
	setup_try_auth_defaults(&leaf_data, &merkle_tree, &request, &response);
	MOCK_update_timestamp.boot_count = 1;
	MOCK_update_timestamp.timer_value = 65 * SECOND;

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	TEST_ASSERT(memcmp(response.header.root, merkle_tree.root,
			   sizeof(merkle_tree.root)) != 0);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_TRY_AUTH);
	TEST_ASSERT(response.header.data_length ==
		    sizeof(wng_response_try_auth_t));
	TEST_RET_EQ(response.header.result_code, EC_SUCCESS);

	TEST_ASSERT_ARRAY_EQ(response.header.root, ROOT_WITH_DEFAULT_HMAC,
			     sizeof(ROOT_WITH_DEFAULT_HMAC));

	TEST_ASSERT_ARRAY_EQ(response.data.try_auth.wrapped_leaf_data.hmac,
			     DEFAULT_HMAC, sizeof(DEFAULT_HMAC));
	TEST_ASSERT_ARRAY_EQ(response.data.try_auth.wrapped_leaf_data.iv,
			     DEFAULT_IV, sizeof(DEFAULT_IV));
	DCRYPTO_aes_ctr((uint8_t *)&leaf_data, EMPTY_TREE.wrap_key,
			sizeof(EMPTY_TREE.wrap_key) * 8, DEFAULT_IV,
			response.data.try_auth.wrapped_leaf_data.cipher_text,
			sizeof(leaf_data));
	TEST_ASSERT_ARRAY_EQ((uint8_t *)&leaf_data.idat,
			     (uint8_t *)&DEFAULT_IDAT, sizeof(DEFAULT_IDAT));
	TEST_ASSERT(leaf_data.attempt_count == 0);
	return EC_SUCCESS;
}

/******************************************************************************/
/* Reset auth test cases.
 */

static int handle_reset_auth_invalid_length(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	++request.header.data_length;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LENGTH_INVALID);
}

static int handle_reset_auth_label_invalid(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	const size_t offset = (void *)(&(((leaf_data_t *)0)->idat.label)) -
			(void *)0;

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	request.data.remove_leaf.leaf_location |= 3;

	TEST_RET_EQ(test_handle_error_msg(&merkle_tree, &request, &response,
					  WNG_ERR_LABEL_INVALID),
		    EC_SUCCESS);

	setup_reset_auth_defaults(&merkle_tree, &request, &response);
	request.data.reset_auth.wrapped_leaf_data.cipher_text[offset] ^= 0xff;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_LABEL_INVALID);
}

static int handle_reset_auth_path_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	request.data.reset_auth.path_hashes[0][0] ^= 0xff;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_PATH_AUTH_FAILED);
}

static int handle_reset_auth_hmac_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	MOCK_hash_update_cb = 0;
	MOCK_hmac = EMPTY_TREE.root;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_HMAC_AUTH_FAILED);
}

static int handle_reset_auth_crypto_failure(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	MOCK_aes_fail = 1;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_CRYPTO_FAILURE);
}

static int handle_reset_auth_reset_auth_failed(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	request.data.reset_auth.reset_secret[0] ^= 0xff;

	return test_handle_error_msg(&merkle_tree, &request, &response,
				     WNG_ERR_RESET_AUTH_FAILED);
}

static int handle_reset_auth_success(void)
{
	merkle_tree_t merkle_tree;
	wng_request_t request;
	wng_response_t response;
	leaf_data_t leaf_data = {};

	setup_reset_auth_defaults(&merkle_tree, &request, &response);

	TEST_RET_EQ(wng_handle_request(&merkle_tree, &request, &response),
		    EC_SUCCESS);

	TEST_ASSERT(memcmp(response.header.root, merkle_tree.root,
			   sizeof(merkle_tree.root)) != 0);

	TEST_ASSERT(response.header.version == WNG_PROTOCOL_VERSION);
	TEST_ASSERT(response.header.type == WNG_MTA_RESET_AUTH);
	TEST_ASSERT(response.header.data_length ==
		    sizeof(wng_response_reset_auth_t));
	TEST_RET_EQ(response.header.result_code, EC_SUCCESS);

	TEST_ASSERT_ARRAY_EQ(response.header.root, ROOT_WITH_DEFAULT_HMAC,
			     sizeof(ROOT_WITH_DEFAULT_HMAC));

	TEST_ASSERT_ARRAY_EQ(response.data.reset_auth.wrapped_leaf_data.hmac,
			     DEFAULT_HMAC, sizeof(DEFAULT_HMAC));
	TEST_ASSERT_ARRAY_EQ(response.data.reset_auth.wrapped_leaf_data.iv,
			     DEFAULT_IV, sizeof(DEFAULT_IV));
	DCRYPTO_aes_ctr((uint8_t *)&leaf_data, EMPTY_TREE.wrap_key,
			sizeof(EMPTY_TREE.wrap_key) * 8, DEFAULT_IV,
			response.data.reset_auth.wrapped_leaf_data.cipher_text,
			sizeof(leaf_data));
	TEST_ASSERT_ARRAY_EQ((uint8_t *)&leaf_data.idat,
			     (uint8_t *)&DEFAULT_IDAT, sizeof(DEFAULT_IDAT));
	TEST_ASSERT(leaf_data.attempt_count == 0);
	return EC_SUCCESS;
}

/******************************************************************************/
/* Main test function. Encapsulates the test cases..
 */

void run_test(void)
{
	test_reset();

	/* Test basic operations. */
	RUN_TEST(get_path_length_test);
	RUN_TEST(get_index_test);

	/* Test header validation. */
	RUN_TEST(handle_request_version_mismatch);
	RUN_TEST(handle_request_invalid_type);

	/* Test reset tree. */
	RUN_TEST(handle_reset_tree_invalid_length);
	RUN_TEST(handle_reset_tree_param_logk_invalid);
	RUN_TEST(handle_reset_tree_param_h_invalid);
	RUN_TEST(handle_reset_tree_success);

	/* Test insert leaf. */
	RUN_TEST(handle_insert_leaf_invalid_length);
	RUN_TEST(handle_insert_leaf_label_invalid);
	RUN_TEST(handle_insert_leaf_delay_schedule_invalid);
	RUN_TEST(handle_insert_leaf_path_auth_failed);
	RUN_TEST(handle_insert_leaf_crypto_failure);
	RUN_TEST(handle_insert_leaf_success);

	/* Test remove leaf. */
	RUN_TEST(handle_remove_leaf_invalid_length);
	RUN_TEST(handle_remove_leaf_label_invalid);
	RUN_TEST(handle_remove_leaf_path_auth_failed);
	RUN_TEST(handle_remove_leaf_success);

	/* Test try auth. */
	RUN_TEST(handle_try_auth_invalid_length);
	RUN_TEST(handle_try_auth_label_invalid);
	RUN_TEST(handle_try_auth_path_auth_failed);
	RUN_TEST(handle_try_auth_hmac_auth_failed);
	RUN_TEST(handle_try_auth_crypto_failure);
	RUN_TEST(handle_try_auth_rate_limit_reached);
	RUN_TEST(handle_try_auth_lowent_auth_failed);
	RUN_TEST(handle_try_auth_success);

	/* Test reset auth. */
	RUN_TEST(handle_reset_auth_invalid_length);
	RUN_TEST(handle_reset_auth_label_invalid);
	RUN_TEST(handle_reset_auth_path_auth_failed);
	RUN_TEST(handle_reset_auth_hmac_auth_failed);
	RUN_TEST(handle_reset_auth_crypto_failure);
	RUN_TEST(handle_reset_auth_reset_auth_failed);
	RUN_TEST(handle_reset_auth_success);

	test_print_result();
}
