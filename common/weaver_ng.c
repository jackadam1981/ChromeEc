/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <weaver_ng.h>

#include <common.h>
#include <console.h>
#include <timer.h>
#include <trng.h>
#include <util.h>
#include <weaver_ng_tpm_imports.h>

/* Compile time sanity checks. */
_Static_assert(sizeof(hash_t) >= SHA256_DIGEST_SIZE,
	       "hash_t is too small for a SHA256 digest/");

_Static_assert(sizeof(leaf_data_t) % WNG_WRAP_BLOCK_SIZE == 0,
	       "sizeof(leaf_data_t) % 16 should be zero");

_Static_assert(sizeof(((wng_request_t *)0)->data) ==
			       sizeof(((wng_request_t *)0)->data.raw),
	       "wng_request_t.data.raw should be the largest member of the "
			       "union");

_Static_assert(sizeof(((wng_response_t *)0)->data) ==
	       sizeof(((wng_response_t *)0)->data.raw),
	       "wng_response_t.data.raw should be the largest member of the "
			       "union");

int create_merkle_tree(bits_per_level_t bits_per_level, height_t height,
			merkle_tree_t *merkle_tree)
{
	uint16_t fan_out = 1 << bits_per_level;
	hash_t child_hashes[fan_out];
	height_t hx;
	uint16_t kx;

	if (bits_per_level > BITS_PER_LEVEL_MAX ||
			bits_per_level < BITS_PER_LEVEL_MIN)
		return WNG_ERR_BITS_PER_LEVEL_INVALID;
	if (height > HEIGHT_MAX(bits_per_level) || height < HEIGHT_MIN ||
			((fan_out - 1) * height) * sizeof(hash_t) + 128 >
					WNG_MAX_PATH_SIZE)
		return WNG_ERR_HEIGHT_INVALID;

	merkle_tree->bits_per_level = bits_per_level;
	merkle_tree->height = height;

	/* Initialize the root hash. */
	memset(child_hashes, 0, sizeof(child_hashes));
	memset(merkle_tree->root, 0, sizeof(merkle_tree->root));
	DCRYPTO_SHA256_hash(child_hashes[0], sizeof(child_hashes),
			    merkle_tree->root);
	for (hx = 1; hx < height; ++hx) {
		for (kx = 0; kx < fan_out; ++kx) {
			memcpy(child_hashes[kx], &merkle_tree->root,
			       sizeof(merkle_tree->root));
		}
		memset(merkle_tree->root, 0, sizeof(merkle_tree->root));
		DCRYPTO_SHA256_hash(child_hashes[0], sizeof(child_hashes),
				    merkle_tree->root);
	}

	rand_bytes(merkle_tree->hmac_key, sizeof(merkle_tree->hmac_key));

	rand_bytes(merkle_tree->wrap_key, sizeof(merkle_tree->wrap_key));

	/* TODO(allenwebb) generate public private key pair */
	return EC_SUCCESS;
}

int store_merkle_tree(uint8_t slot, const merkle_tree_t *merkle_tree)
{
	/* TODO(allenwebb)
	 * 1) Find some flash that can be dedicated to this feature
	 * 2) Implement this function
	 */
	return EC_ERROR_UNIMPLEMENTED;
}

int load_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree)
{
	/* TODO(allenwebb) Implement this function. */
	return EC_ERROR_UNIMPLEMENTED;
}

index_t get_index(const merkle_tree_t *merkle_tree, label_t label,
		  height_t level)
{
	index_t mask = ~((~(index_t)0) << merkle_tree->bits_per_level);
	uint8_t shift_by = (sizeof(label) << 3) -
			merkle_tree->bits_per_level * (level + 1);
	return (label >> shift_by) & mask;
}

int get_path_length(const merkle_tree_t *merkle_tree)
{
	return ((1 << merkle_tree->bits_per_level) - 1) * merkle_tree->height;
}

void compute_hmac(const merkle_tree_t *merkle_tree,
		  const wrapped_leaf_data_t *wrapped_leaf_data, hash_t *result)
{
	LITE_HMAC_CTX hmac;

	DCRYPTO_HMAC_SHA256_init(&hmac, merkle_tree->hmac_key,
				 sizeof(merkle_tree->hmac_key));
	HASH_update(&hmac.hash, wrapped_leaf_data->cipher_text,
		    sizeof(wrapped_leaf_data->cipher_text));
	memcpy(*result, DCRYPTO_HMAC_final(&hmac), sizeof(*result));
}

void compute_hash(const hash_t (*hashes)[], uint16_t num_hashes,
		  index_t location, const hash_t *child_hash, hash_t *result)
{
	hash_t buffer[num_hashes + 1];

	if (location > 0) {
		memcpy(buffer[0], (*hashes)[0],
		       sizeof((*hashes)[0]) * location);
	}
	memcpy(buffer[location], *child_hash, sizeof(*child_hash));
	if (location < num_hashes) {
		memcpy(buffer[location + 1], (*hashes)[location],
				    sizeof((*hashes)[0]) *
						    (num_hashes - location));
	}
	DCRYPTO_SHA256_hash(buffer, sizeof(buffer), *result);
}

void compute_root_hash(const merkle_tree_t *merkle_tree, label_t path,
		       const hash_t (*hashes)[], const hash_t *child_hash,
		       hash_t *new_root)
{
	uint16_t fan_out = 1 << merkle_tree->bits_per_level;
	/* Ugly way to convert a 1D array to a 2D one. */
	const hash_t (*view)[merkle_tree->height][fan_out - 1] =
			(void *)hashes;
	hash_t temp_a;
	hash_t temp_b;
	height_t hx = 0;
	index_t index = get_index(merkle_tree, path,
				  merkle_tree->height - hx - 1);

	/* Case child_hash -> new_root */
	if (merkle_tree->height == 1) {
		compute_hash(&(*view)[hx], fan_out - 1, index, child_hash,
			     new_root);
		return;
	}

	/* Case child_hash -> temp_a */
	compute_hash(&(*view)[hx], fan_out - 1, index, child_hash, &temp_a);
	for (hx = 1; hx < merkle_tree->height - 1; ++hx) {
		/* Case temp_a -> temp_b */
		index = get_index(merkle_tree, path,
				  merkle_tree->height - hx - 1);
		compute_hash(&(*view)[hx], fan_out - 1, index,
			     (const hash_t *)&temp_a,
			     &temp_b);

		/* Unroll loop to alternate buffers. */
		++hx;
		if (hx >= merkle_tree->height - 1)
			break;

		/* Case temp_b -> temp_a */
		index = get_index(merkle_tree, path,
				  merkle_tree->height - hx - 1);
		compute_hash(&(*view)[hx], fan_out - 1, index,
			     (const hash_t *)&temp_b,
			     &temp_a);
	}

	/* Handle last case temp_? -> new_root. */
	index = get_index(merkle_tree, path, 0);
	compute_hash(&(*view)[hx], fan_out - 1, index,
		     (const hash_t *)((hx & 0x1) == 0 ? &temp_b : &temp_a),
		     new_root);
}

int authenticate_path(const merkle_tree_t *merkle_tree, label_t path,
		      const hash_t (*hashes)[], const hash_t *child_hash)
{
	hash_t parent;

	compute_root_hash(merkle_tree, path, hashes, child_hash, &parent);
	if (memcmp(parent, merkle_tree->root, sizeof(parent)) != 0)
		return WNG_ERR_PATH_AUTH_FAILED;
	return EC_SUCCESS;
}

int encrypt_leaf_data(const merkle_tree_t *merkle_tree,
		      const leaf_data_t *leaf_data,
		      wrapped_leaf_data_t *wrapped_leaf_data)
{
	/* Generate a random IV. */
	rand_bytes(wrapped_leaf_data->iv, sizeof(wrapped_leaf_data->iv));
	if (DCRYPTO_aes_ctr(wrapped_leaf_data->cipher_text,
			     merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv, (uint8_t *)leaf_data,
			     sizeof(*leaf_data)) != EC_SUCCESS) {
		return WNG_ERR_CRYPTO_FAILURE;
	}
	return EC_SUCCESS;
}

int decrypt_leaf_data(const merkle_tree_t *merkle_tree,
		      const wrapped_leaf_data_t *wrapped_leaf_data,
		      leaf_data_t *leaf_data)
{
	if (DCRYPTO_aes_ctr((uint8_t *)leaf_data, merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv,
			     wrapped_leaf_data->cipher_text,
			     sizeof(*leaf_data)) != EC_SUCCESS) {
		return WNG_ERR_CRYPTO_FAILURE;
	}
	return EC_SUCCESS;
}

/* Verifies that merkle_tree has been initialized. */
static int validate_tree(const merkle_tree_t *merkle_tree)
{
	if (merkle_tree->bits_per_level < BITS_PER_LEVEL_MIN ||
			merkle_tree->bits_per_level > BITS_PER_LEVEL_MAX ||
			merkle_tree->height < HEIGHT_MIN ||
			merkle_tree->height >
					HEIGHT_MAX(merkle_tree->bits_per_level))
		return WNG_ERR_TREE_INVALID;
	return EC_SUCCESS;
}

/* Checks the following conditions:
 * Extra index fields should be all zero.
 */
static int validate_label(const merkle_tree_t *merkle_tree, label_t path)
{
	uint8_t shift_by = merkle_tree->bits_per_level * merkle_tree->height;

	if ((path & (~((label_t)0)) >> shift_by) == 0)
		return EC_SUCCESS;
	return WNG_ERR_LABEL_INVALID;
}

/* Checks the following conditions:
 * Columns should be strictly increasing.
 * Zeroes for filler at the end of the delay_schedule are permitted.
 */
static int validate_delay_schedule(const delay_schedule_t *delay_schedule)
{
	size_t x;

	/* The first entry should not be useless. */
	if ((*delay_schedule)[0].time_diff == 0)
		return WNG_ERR_DELAY_SCHEDULE_INVALID;

	for (x = ARRAY_SIZE(*delay_schedule) - 1; x > 0; --x) {
		if ((*delay_schedule)[x].attempt_count == 0) {
			if ((*delay_schedule)[x].time_diff != 0)
				return WNG_ERR_DELAY_SCHEDULE_INVALID;
		} else if ((*delay_schedule)[x].attempt_count <=
				(*delay_schedule)[x - 1].attempt_count ||
				(*delay_schedule)[x].time_diff <=
				(*delay_schedule)[x - 1].time_diff) {
			return WNG_ERR_DELAY_SCHEDULE_INVALID;
		}
	}
	return EC_SUCCESS;
}

wng_timestamp_t MOCK_update_timestamp;
/* Sets the value of ts to the current notion of time. */
static void update_timestamp(wng_timestamp_t *ts)
{
#ifdef CHIP_HOST
	ts->timer_value = MOCK_update_timestamp.timer_value;
	ts->boot_count = MOCK_update_timestamp.boot_count;
#else
	ts->timer_value = get_time().val;
	ts->boot_count = get_restart_count();
#endif
}

/* Checks if an auth attempt can be made or not based on the delay schedule.
 * EC_SUCCESS is returned when a new attempt can be made.
 */
static int test_rate_limit(leaf_data_t *leaf_data)
{
	uint64_t ready_time;
	uint8_t x;
	wng_timestamp_t current_time;
	time_diff_t delay = 0;

	/* This loop ends when x is one greater than the index that applies. */
	for (x = 0; x < ARRAY_SIZE(leaf_data->idat.delay_schedule) &&
			leaf_data->idat.delay_schedule[x].attempt_count != 0 &&
			leaf_data->attempt_count >=
			leaf_data->idat.delay_schedule[x].attempt_count;
	     ++x) {
	}

	if (x > 1)
		delay = leaf_data->idat.delay_schedule[x - 1].time_diff;

	if (delay == 0)
		return EC_SUCCESS;

	if (delay == WNG_BLOCK_ATTEMPTS)
		return WNG_ERR_RATE_LIMIT_REACHED;

	update_timestamp(&current_time);

	/* TODO(allenwebb) verify the timer starts at zero on reboot. */
	if (leaf_data->timestamp.boot_count == current_time.boot_count)
		ready_time = delay * SECOND + leaf_data->timestamp.timer_value;
	else
		ready_time = delay * SECOND;

	return current_time.timer_value > ready_time ? EC_SUCCESS :
	       WNG_ERR_RATE_LIMIT_REACHED;
}

int wng_handle_request(merkle_tree_t *merkle_tree, const wng_request_t *request,
		       wng_response_t *response)
{
	int32_t ret;

	response->header.version = WNG_PROTOCOL_VERSION;
	response->header.type = WNG_MT_ERROR_MSG;
	response->header.data_length = 0;
	/* Initialize new_root to the current root. */
	memcpy(response->header.root, merkle_tree->root,
	       sizeof(merkle_tree->root));

	if (request->header.version != WNG_PROTOCOL_VERSION) {
		response->header.result_code = WNG_ERR_VERSION_MISMATCH;
		return WNG_ERR_VERSION_MISMATCH;
	}

	switch (request->header.type) {
	case WNG_MTQ_RESET_TREE:
		if (request->header.data_length !=
				sizeof(request->data.reset_tree)) {
			ret = WNG_ERR_LENGTH_INVALID;
			break;
		}
		ret = wng_handle_reset_tree(merkle_tree,
					    &request->data.reset_tree,
					    &response->header.root);
		if (ret == EC_SUCCESS)
			response->header.type = WNG_MTA_RESET_TREE;
		break;
	case WNG_MTQ_INSERT_LEAF:
		if (request->header.data_length !=
				sizeof(request->data.insert_leaf) +
				get_path_length(merkle_tree)) {
			ret = WNG_ERR_LENGTH_INVALID;
			break;
		}
		ret = wng_handle_insert_leaf(merkle_tree,
					     &request->data.insert_leaf,
					     &response->data.insert_leaf,
					     &response->header.root);
		if (ret == EC_SUCCESS) {
			response->header.type = WNG_MTA_INSERT_LEAF;
			response->header.data_length =
					sizeof(response->data.insert_leaf);
		}
		break;
	case WNG_MTQ_REMOVE_LEAF:
		if (request->header.data_length !=
				sizeof(request->data.remove_leaf) +
				get_path_length(merkle_tree)) {
			ret = WNG_ERR_LENGTH_INVALID;
			break;
		}
		ret = wng_handle_remove_leaf(merkle_tree,
					     &request->data.remove_leaf,
					     &response->header.root);
		if (ret == EC_SUCCESS)
			response->header.type = WNG_MTA_REMOVE_LEAF;
		break;
	case WNG_MTQ_TRY_AUTH:
		if (request->header.data_length !=
				sizeof(request->data.try_auth) +
				get_path_length(merkle_tree)) {
			ret = WNG_ERR_LENGTH_INVALID;
			break;
		}
		ret = wng_handle_try_auth(merkle_tree,
					  &request->data.try_auth,
					  &response->data.try_auth,
					  &response->header.root);
		/* Constant time OR operation to avoid timing side channels. */
		if ((ret == EC_SUCCESS) + (ret == (WNG_ERR_LOWENT_AUTH_FAILED))
						 > 0) {
			response->header.type = WNG_MTA_TRY_AUTH;
			response->header.data_length =
					sizeof(response->data.try_auth);
		}
		break;
	case WNG_MTQ_RESET_AUTH:
		if (request->header.data_length !=
				sizeof(request->data.reset_auth) +
				get_path_length(merkle_tree)) {
			ret = WNG_ERR_LENGTH_INVALID;
			break;
		}
		ret = wng_handle_reset_auth(merkle_tree,
					    &request->data.reset_auth,
					    &response->data.reset_auth,
					    &response->header.root);
		if (ret == EC_SUCCESS) {
			response->header.type = WNG_MTA_RESET_AUTH;
			response->header.data_length =
					sizeof(response->data.reset_auth);
		}
		break;
	default:
		ret = WNG_ERR_TYPE_INVALID;
		break;
	}
	response->header.result_code = ret;
	return ret;
};

int wng_handle_reset_tree(merkle_tree_t *merkle_tree,
			  const wng_request_reset_tree_t *request,
			  hash_t *new_root)
{
	int ret = create_merkle_tree(request->bits_per_level, request->height,
				     merkle_tree);

	if (ret == EC_SUCCESS)
		memcpy(*new_root, merkle_tree->root, sizeof(*new_root));
	return ret;
}

int wng_handle_insert_leaf(merkle_tree_t *merkle_tree,
			   const wng_request_insert_leaf_t *request,
			   wng_response_insert_leaf_t *response,
			   hash_t *new_root)
{
	int ret = EC_SUCCESS;
	leaf_data_t leaf_data = {};
	const hash_t empty_hash = {};

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->idat.label);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_delay_schedule(&request->idat.delay_schedule);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->idat.label,
				&request->path_hashes, &empty_hash);
	if (ret != EC_SUCCESS)
		return ret;

	memset(&leaf_data, 0, sizeof(leaf_data));
	leaf_data.version = WNG_STORAGE_VERSION;

	memcpy(&leaf_data.idat, &request->idat, sizeof(leaf_data.idat));

	ret = encrypt_leaf_data(merkle_tree, &leaf_data,
				&response->wrapped_leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &response->wrapped_leaf_data,
		     &response->wrapped_leaf_data.hmac);

	compute_root_hash(merkle_tree, leaf_data.idat.label,
			  &request->path_hashes,
			  (const hash_t *)&response->wrapped_leaf_data.hmac,
			  new_root);

	return ret;
}

int wng_handle_remove_leaf(merkle_tree_t *merkle_tree,
			   const wng_request_remove_leaf_t *request,
			   hash_t *new_root)
{
	int ret = EC_SUCCESS;
	const hash_t empty_hash = {};

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				&request->path_hashes, &request->leaf_hmac);
	if (ret != EC_SUCCESS)
		return ret;

	compute_root_hash(merkle_tree, request->leaf_location,
			  &request->path_hashes, &empty_hash, new_root);

	return ret;
}

int wng_handle_try_auth(merkle_tree_t *merkle_tree,
			const wng_request_try_auth_t *request,
			wng_response_try_auth_t *response,
			hash_t *new_root)
{
	int ret = EC_SUCCESS;
	attempt_count_t dummy_ac = 0;
	leaf_data_t leaf_data = {};
	hash_t hmac;

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				&request->path_hashes,
				&request->wrapped_leaf_data.hmac);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &request->wrapped_leaf_data, &hmac);
	if (memcmp(hmac, request->wrapped_leaf_data.hmac, sizeof(hmac)) != 0)
		return WNG_ERR_HMAC_AUTH_FAILED;

	ret = decrypt_leaf_data(merkle_tree, &request->wrapped_leaf_data,
				&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	if (leaf_data.idat.label != request->leaf_location)
		return WNG_ERR_LABEL_INVALID;

	ret = test_rate_limit(&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	/* ret must not be overwritten after this. */
	if (safe_memcmp(request->low_entropy_secret,
			leaf_data.idat.low_entropy_secret,
			sizeof(request->low_entropy_secret)) != 0) {
		++leaf_data.attempt_count;
		dummy_ac = 0;
		ret = WNG_ERR_LOWENT_AUTH_FAILED;
	} else {
		++dummy_ac;
		leaf_data.attempt_count = 0;
		ret = EC_SUCCESS;
	}
	update_timestamp(&leaf_data.timestamp);

	if (encrypt_leaf_data(merkle_tree, &leaf_data,
			      &response->wrapped_leaf_data) != EC_SUCCESS)
		return WNG_ERR_CRYPTO_FAILURE;

	compute_hmac(merkle_tree, &response->wrapped_leaf_data,
		     &response->wrapped_leaf_data.hmac);

	compute_root_hash(merkle_tree, leaf_data.idat.label,
			  &request->path_hashes,
			  (const hash_t *)&response->wrapped_leaf_data.hmac,
			  new_root);

	return ret;
}

int wng_handle_reset_auth(merkle_tree_t *merkle_tree,
			  const wng_request_reset_auth_t *request,
			  wng_response_reset_auth_t *response,
			  hash_t *new_root)
{
	int ret = EC_SUCCESS;
	leaf_data_t leaf_data = {};
	hash_t hmac;

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				&request->path_hashes,
				&request->wrapped_leaf_data.hmac);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &request->wrapped_leaf_data, &hmac);
	if (memcmp(hmac, request->wrapped_leaf_data.hmac, sizeof(hmac)) != 0)
		return WNG_ERR_HMAC_AUTH_FAILED;

	ret = decrypt_leaf_data(merkle_tree, &request->wrapped_leaf_data,
				&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	if (leaf_data.idat.label != request->leaf_location)
		return WNG_ERR_LABEL_INVALID;

	if (safe_memcmp(request->reset_secret,
			leaf_data.idat.reset_secret,
			sizeof(request->reset_secret)) != 0)
		return WNG_ERR_RESET_AUTH_FAILED;

	leaf_data.attempt_count = 0;

	ret = encrypt_leaf_data(merkle_tree, &leaf_data,
				&response->wrapped_leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &response->wrapped_leaf_data,
		     &response->wrapped_leaf_data.hmac);

	compute_root_hash(merkle_tree, leaf_data.idat.label,
			  &request->path_hashes,
			  (const hash_t *)&response->wrapped_leaf_data.hmac,
			  new_root);

	return ret;
}
