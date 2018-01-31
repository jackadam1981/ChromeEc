/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pinweaver.h>

#include <common.h>
#include <console.h>
#include <pinweaver_tpm_imports.h>
#include <pinweaver_types.h>
#include <timer.h>
#include <trng.h>
#include <util.h>

/* Compile time sanity checks. */
/* Make sure the hash size is consistent with dcrypto. */
BUILD_ASSERT(PW_HASH_SIZE >= SHA256_DIGEST_SIZE);

/* sizeof(struct leaf_data_t) % 16 should be zero */
BUILD_ASSERT(sizeof(struct leaf_data_t) % PW_WRAP_BLOCK_SIZE == 0);

/* pw_request_t.data.raw should be the largest member of the union. */
BUILD_ASSERT(sizeof(((struct pw_request_t *)0)->data) ==
			     sizeof(((struct pw_request_t *)0)->data.raw));

/* pw_response_t.data.raw should be the largest member of the union */
BUILD_ASSERT(sizeof(((struct pw_response_t *)0)->data) ==
			     sizeof(((struct pw_response_t *)0)->data.raw));

int create_merkle_tree(bits_per_level_t bits_per_level, height_t height,
		       struct merkle_tree_t *merkle_tree)
{
	uint16_t fan_out = 1 << bits_per_level;
	uint8_t child_hashes[fan_out][PW_HASH_SIZE];
	height_t hx;
	uint16_t kx;

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

int store_merkle_tree(uint8_t slot, const struct merkle_tree_t *merkle_tree)
{
	/* TODO(allenwebb)
	 * 1) Find some flash that can be dedicated to this feature
	 * 2) Implement this function
	 */
	return EC_ERROR_UNIMPLEMENTED;
}

int load_merkle_tree(uint8_t slot, struct merkle_tree_t *merkle_tree)
{
	/* TODO(allenwebb) Implement this function. */
	return EC_ERROR_UNIMPLEMENTED;
}

index_t get_index(const struct merkle_tree_t *merkle_tree, label_t label,
		  height_t level)
{
	index_t mask = ~((~(index_t)0) << merkle_tree->bits_per_level);
	uint8_t shift_by = (sizeof(label) << 3) -
			merkle_tree->bits_per_level * (level + 1);
	return (label >> shift_by) & mask;
}

int get_path_length(const struct merkle_tree_t *merkle_tree)
{
	return ((1 << merkle_tree->bits_per_level) - 1) * merkle_tree->height;
}

void compute_hmac(const struct merkle_tree_t *merkle_tree,
		  const struct wrapped_leaf_data_t *wrapped_leaf_data,
		  uint8_t result[PW_HASH_SIZE])
{
	LITE_HMAC_CTX hmac;

	DCRYPTO_HMAC_SHA256_init(&hmac, merkle_tree->hmac_key,
				 sizeof(merkle_tree->hmac_key));
	HASH_update(&hmac.hash, wrapped_leaf_data->cipher_text,
		    sizeof(wrapped_leaf_data->cipher_text));
	memcpy(result, DCRYPTO_HMAC_final(&hmac), PW_HASH_SIZE);
}

void compute_hash(const uint8_t hashes[][PW_HASH_SIZE], uint16_t num_hashes,
		  index_t location, const uint8_t child_hash[PW_HASH_SIZE],
		  uint8_t result[PW_HASH_SIZE])
{
	uint8_t buffer[num_hashes + 1][PW_HASH_SIZE];

	if (location > 0)
		memcpy(buffer[0], hashes[0], PW_HASH_SIZE * location);
	memcpy(buffer[location], child_hash, PW_HASH_SIZE);
	if (location < num_hashes)
		memcpy(buffer[location + 1], hashes[location],
		       PW_HASH_SIZE * (num_hashes - location));
	DCRYPTO_SHA256_hash(buffer[0], sizeof(buffer), result);
}

void compute_root_hash(const struct merkle_tree_t *merkle_tree, label_t path,
		       const uint8_t hashes[][PW_HASH_SIZE],
		       const uint8_t child_hash[PW_HASH_SIZE],
		       uint8_t new_root[PW_HASH_SIZE])
{
	uint16_t fan_out = 1 << merkle_tree->bits_per_level;
	/* Ugly way to convert a 1D array to a 2D one. */
	const uint8_t (*view)[merkle_tree->height][fan_out - 1][PW_HASH_SIZE] =
			(void *)hashes;
	uint8_t temp_a[PW_HASH_SIZE];
	uint8_t temp_b[PW_HASH_SIZE];
	height_t hx = 0;
	index_t index = get_index(merkle_tree, path,
				  merkle_tree->height - hx - 1);

	/* Case child_hash -> new_root */
	if (merkle_tree->height == 1) {
		compute_hash((*view)[hx], fan_out - 1, index, child_hash,
			     new_root);
		return;
	}

	/* Case child_hash -> temp_a */
	compute_hash((*view)[hx], fan_out - 1, index, child_hash, temp_a);
	for (hx = 1; hx < merkle_tree->height - 1; ++hx) {
		/* Case temp_a -> temp_b */
		index = get_index(merkle_tree, path,
				  merkle_tree->height - hx - 1);
		compute_hash((*view)[hx], fan_out - 1, index, temp_a, temp_b);

		/* Unroll loop to alternate buffers. */
		++hx;
		if (hx >= merkle_tree->height - 1)
			break;

		/* Case temp_b -> temp_a */
		index = get_index(merkle_tree, path,
				  merkle_tree->height - hx - 1);
		compute_hash((*view)[hx], fan_out - 1, index, temp_b, temp_a);
	}

	/* Handle last case temp_? -> new_root. */
	index = get_index(merkle_tree, path, 0);
	compute_hash((*view)[hx], fan_out - 1, index,
		     ((hx & 0x1) == 0 ? temp_b : temp_a),
		     new_root);
}

int authenticate_path(const struct merkle_tree_t *merkle_tree, label_t path,
		      const uint8_t hashes[][PW_HASH_SIZE],
		      const uint8_t child_hash[PW_HASH_SIZE])
{
	uint8_t parent[PW_HASH_SIZE];

	compute_root_hash(merkle_tree, path, hashes, child_hash, parent);
	if (memcmp(parent, merkle_tree->root, sizeof(parent)) != 0)
		return PW_ERR_PATH_AUTH_FAILED;
	return EC_SUCCESS;
}

int encrypt_leaf_data(const struct merkle_tree_t *merkle_tree,
		      const struct leaf_data_t *leaf_data,
		      struct wrapped_leaf_data_t *wrapped_leaf_data)
{
	/* Generate a random IV. */
	rand_bytes(wrapped_leaf_data->iv, sizeof(wrapped_leaf_data->iv));
	if (DCRYPTO_aes_ctr(wrapped_leaf_data->cipher_text,
			     merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv, (uint8_t *)leaf_data,
			     sizeof(*leaf_data)) != EC_SUCCESS) {
		return PW_ERR_CRYPTO_FAILURE;
	}
	return EC_SUCCESS;
}

int decrypt_leaf_data(const struct merkle_tree_t *merkle_tree,
		      const struct wrapped_leaf_data_t *wrapped_leaf_data,
		      struct leaf_data_t *leaf_data)
{
	if (DCRYPTO_aes_ctr((uint8_t *)leaf_data, merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv,
			     wrapped_leaf_data->cipher_text,
			     sizeof(*leaf_data)) != EC_SUCCESS) {
		return PW_ERR_CRYPTO_FAILURE;
	}
	return EC_SUCCESS;
}

static int validate_tree_parameters(bits_per_level_t bits_per_level,
				    height_t height)
{
	uint8_t fan_out = 1 << bits_per_level;

	if (bits_per_level < BITS_PER_LEVEL_MIN ||
	    bits_per_level > BITS_PER_LEVEL_MAX)
		return PW_ERR_BITS_PER_LEVEL_INVALID;
	if (height < HEIGHT_MIN ||
	    height > HEIGHT_MAX(bits_per_level) ||
	    ((fan_out - 1) * height) * PW_HASH_SIZE > PW_MAX_PATH_SIZE)
		return PW_ERR_HEIGHT_INVALID;
	return EC_SUCCESS;
}

/* Verifies that merkle_tree has been initialized. */
static int validate_tree(const struct merkle_tree_t *merkle_tree)
{
	if (validate_tree_parameters(merkle_tree->bits_per_level,
				     merkle_tree->height) != EC_SUCCESS)
		return PW_ERR_TREE_INVALID;
	return EC_SUCCESS;
}

/* Checks the following conditions:
 * Extra index fields should be all zero.
 */
static int validate_label(const struct merkle_tree_t *merkle_tree, label_t path)
{
	uint8_t shift_by = merkle_tree->bits_per_level * merkle_tree->height;

	if ((path & (~((label_t)0)) >> shift_by) == 0)
		return EC_SUCCESS;
	return PW_ERR_LABEL_INVALID;
}

/* Checks the following conditions:
 * Columns should be strictly increasing.
 * Zeroes for filler at the end of the delay_schedule are permitted.
 */
static int validate_delay_schedule(const struct delay_schedule_entry_t
				   delay_schedule[PW_SCHED_COUNT])
{
	size_t x;

	/* The first entry should not be useless. */
	if (delay_schedule[0].time_diff == 0)
		return PW_ERR_DELAY_SCHEDULE_INVALID;

	for (x = PW_SCHED_COUNT - 1; x > 0; --x) {
		if (delay_schedule[x].attempt_count == 0) {
			if (delay_schedule[x].time_diff != 0)
				return PW_ERR_DELAY_SCHEDULE_INVALID;
		} else if (delay_schedule[x].attempt_count <=
				delay_schedule[x - 1].attempt_count ||
				delay_schedule[x].time_diff <=
				delay_schedule[x - 1].time_diff) {
			return PW_ERR_DELAY_SCHEDULE_INVALID;
		}
	}
	return EC_SUCCESS;
}

#ifdef CHIP_HOST
struct pw_timestamp_t MOCK_update_timestamp;
#endif
/* Sets the value of ts to the current notion of time. */
static void update_timestamp(struct pw_timestamp_t *ts)
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
static int test_rate_limit(struct leaf_data_t *leaf_data)
{
	uint64_t ready_time;
	uint8_t x;
	struct pw_timestamp_t current_time;
	time_diff_t delay = 0;

	/* This loop ends when x is one greater than the index that applies. */
	for (x = 0; x < ARRAY_SIZE(leaf_data->idat.delay_schedule) &&
			leaf_data->idat.delay_schedule[x].attempt_count != 0 &&
			leaf_data->attempt_count >=
			leaf_data->idat.delay_schedule[x].attempt_count; ++x) {
	}

	if (x > 1)
		delay = leaf_data->idat.delay_schedule[x - 1].time_diff;

	if (delay == 0)
		return EC_SUCCESS;

	if (delay == PW_BLOCK_ATTEMPTS)
		return PW_ERR_RATE_LIMIT_REACHED;

	update_timestamp(&current_time);

	/* TODO(allenwebb) verify the timer starts at zero on reboot. */
	if (leaf_data->timestamp.boot_count == current_time.boot_count)
		ready_time = delay * SECOND + leaf_data->timestamp.timer_value;
	else
		ready_time = delay * SECOND;

	return current_time.timer_value > ready_time ? EC_SUCCESS :
	       PW_ERR_RATE_LIMIT_REACHED;
}

int pw_handle_request(struct merkle_tree_t *merkle_tree,
		      const struct pw_request_t *request,
		      struct pw_response_t *response)
{
	int32_t ret;
	/* This function needs to support the response and request being in the
	 * same buffer.
	 */
	struct pw_response_header_t header;

	header.version = PW_PROTOCOL_VERSION;
	header.type = PW_MT_ERROR_MSG;
	header.data_length = 0;
	/* Initialize new_root to the current root. */
	memcpy(header.root, merkle_tree->root, sizeof(merkle_tree->root));

	if (request->header.version != PW_PROTOCOL_VERSION) {
		ret = PW_ERR_VERSION_MISMATCH;
		goto cleanup;
	}

	switch (request->header.type) {
	case PW_MTQ_RESET_TREE:
		if (request->header.data_length !=
				sizeof(request->data.reset_tree)) {
			ret = PW_ERR_LENGTH_INVALID;
			break;
		}
		ret = pw_handle_reset_tree(merkle_tree,
					    &request->data.reset_tree,
					    header.root);
		if (ret == EC_SUCCESS)
			header.type = PW_MTA_RESET_TREE;
		break;
	case PW_MTQ_INSERT_LEAF:
		if (request->header.data_length !=
				sizeof(request->data.insert_leaf) +
				get_path_length(merkle_tree)) {
			ret = PW_ERR_LENGTH_INVALID;
			break;
		}
		ret = pw_handle_insert_leaf(merkle_tree,
					     &request->data.insert_leaf,
					     &response->data.insert_leaf,
					     header.root);
		if (ret == EC_SUCCESS) {
			header.type = PW_MTA_INSERT_LEAF;
			header.data_length =
					sizeof(response->data.insert_leaf);
		}
		break;
	case PW_MTQ_REMOVE_LEAF:
		if (request->header.data_length !=
				sizeof(request->data.remove_leaf) +
				get_path_length(merkle_tree)) {
			ret = PW_ERR_LENGTH_INVALID;
			break;
		}
		ret = pw_handle_remove_leaf(merkle_tree,
					     &request->data.remove_leaf,
					     header.root);
		if (ret == EC_SUCCESS)
			header.type = PW_MTA_REMOVE_LEAF;
		break;
	case PW_MTQ_TRY_AUTH:
		if (request->header.data_length !=
				sizeof(request->data.try_auth) +
				get_path_length(merkle_tree)) {
			ret = PW_ERR_LENGTH_INVALID;
			break;
		}
		ret = pw_handle_try_auth(merkle_tree,
					  &request->data.try_auth,
					  &response->data.try_auth,
					  header.root);
		/* Constant time OR operation to avoid timing side channels. */
		if ((ret == EC_SUCCESS) + (ret == (PW_ERR_LOWENT_AUTH_FAILED))
						 > 0) {
			header.type = PW_MTA_TRY_AUTH;
			header.data_length =
					sizeof(response->data.try_auth);
		}
		break;
	case PW_MTQ_RESET_AUTH:
		if (request->header.data_length !=
				sizeof(request->data.reset_auth) +
				get_path_length(merkle_tree)) {
			ret = PW_ERR_LENGTH_INVALID;
			break;
		}
		ret = pw_handle_reset_auth(merkle_tree,
					    &request->data.reset_auth,
					    &response->data.reset_auth,
					    header.root);
		if (ret == EC_SUCCESS) {
			header.type = PW_MTA_RESET_AUTH;
			header.data_length =
					sizeof(response->data.reset_auth);
		}
		break;
	default:
		ret = PW_ERR_TYPE_INVALID;
		break;
	}
cleanup:
	memcpy(&response->header, &header, sizeof(header));
	response->header.result_code = ret;
	return ret;
};

int pw_handle_reset_tree(struct merkle_tree_t *merkle_tree,
			 const struct pw_request_reset_tree_t *request,
			 uint8_t new_root[PW_HASH_SIZE])
{
	int ret;

	ret = validate_tree_parameters(request->bits_per_level,
				       request->height);
	if (ret != EC_SUCCESS)
		return ret;

	ret = create_merkle_tree(request->bits_per_level, request->height,
				     merkle_tree);

	if (ret == EC_SUCCESS)
		memcpy(new_root, merkle_tree->root, sizeof(new_root));
	return ret;
}

int pw_handle_insert_leaf(struct merkle_tree_t *merkle_tree,
			  const struct pw_request_insert_leaf_t *request,
			  struct pw_response_insert_leaf_t *response,
			  uint8_t new_root[PW_HASH_SIZE])
{
	int ret = EC_SUCCESS;
	struct leaf_data_t leaf_data = {};
	struct wrapped_leaf_data_t wrapped_leaf_data;
	const uint8_t empty_hash[PW_HASH_SIZE] = {};

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->idat.label);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_delay_schedule(request->idat.delay_schedule);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->idat.label,
				request->path_hashes, empty_hash);
	if (ret != EC_SUCCESS)
		return ret;

	memset(&leaf_data, 0, sizeof(leaf_data));
	leaf_data.version = PW_STORAGE_VERSION;

	memcpy(&leaf_data.idat, &request->idat, sizeof(leaf_data.idat));

	ret = encrypt_leaf_data(merkle_tree, &leaf_data,
				&wrapped_leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &wrapped_leaf_data, wrapped_leaf_data.hmac);

	compute_root_hash(merkle_tree, leaf_data.idat.label,
			  request->path_hashes, wrapped_leaf_data.hmac,
			  new_root);

	memcpy(&response->wrapped_leaf_data, &wrapped_leaf_data,
	       sizeof(wrapped_leaf_data));

	return ret;
}

int pw_handle_remove_leaf(struct merkle_tree_t *merkle_tree,
			   const struct pw_request_remove_leaf_t *request,
			   uint8_t new_root[PW_HASH_SIZE])
{
	int ret = EC_SUCCESS;
	const uint8_t empty_hash[PW_HASH_SIZE] = {};

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				request->path_hashes, request->leaf_hmac);
	if (ret != EC_SUCCESS)
		return ret;

	compute_root_hash(merkle_tree, request->leaf_location,
			  request->path_hashes, empty_hash, new_root);

	return ret;
}

int pw_handle_try_auth(struct merkle_tree_t *merkle_tree,
			const struct pw_request_try_auth_t *request,
			struct pw_response_try_auth_t *response,
			uint8_t new_root[PW_HASH_SIZE])
{
	int ret = EC_SUCCESS;
	attempt_count_t dummy_ac = 0;
	struct leaf_data_t leaf_data = {};
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t hmac[PW_HASH_SIZE];

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				request->path_hashes,
				request->wrapped_leaf_data.hmac);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &request->wrapped_leaf_data, hmac);
	if (memcmp(hmac, request->wrapped_leaf_data.hmac, sizeof(hmac)) != 0)
		return PW_ERR_HMAC_AUTH_FAILED;

	ret = decrypt_leaf_data(merkle_tree, &request->wrapped_leaf_data,
				&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	if (leaf_data.idat.label != request->leaf_location)
		return PW_ERR_LABEL_INVALID;

	ret = test_rate_limit(&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	/* ret must not be overwritten after this. */
	if (safe_memcmp(request->low_entropy_secret,
			leaf_data.idat.low_entropy_secret,
			sizeof(request->low_entropy_secret)) != 0) {
		++leaf_data.attempt_count;
		dummy_ac = 0;
		ret = PW_ERR_LOWENT_AUTH_FAILED;
	} else {
		++dummy_ac;
		leaf_data.attempt_count = 0;
		ret = EC_SUCCESS;
	}
	update_timestamp(&leaf_data.timestamp);

	if (encrypt_leaf_data(merkle_tree, &leaf_data,
			      &wrapped_leaf_data) != EC_SUCCESS)
		return PW_ERR_CRYPTO_FAILURE;

	compute_hmac(merkle_tree, &wrapped_leaf_data, wrapped_leaf_data.hmac);

	compute_root_hash(merkle_tree, leaf_data.idat.label,
			  request->path_hashes, wrapped_leaf_data.hmac,
			  new_root);

	memcpy(&response->wrapped_leaf_data, &wrapped_leaf_data,
	       sizeof(wrapped_leaf_data));

	return ret;
}

int pw_handle_reset_auth(struct merkle_tree_t *merkle_tree,
			  const struct pw_request_reset_auth_t *request,
			  struct pw_response_reset_auth_t *response,
			  uint8_t new_root[PW_HASH_SIZE])
{
	int ret = EC_SUCCESS;
	struct leaf_data_t leaf_data = {};
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t hmac[PW_HASH_SIZE];

	ret = validate_tree(merkle_tree);
	if (ret != EC_SUCCESS)
		return ret;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		return ret;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				request->path_hashes,
				request->wrapped_leaf_data.hmac);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &request->wrapped_leaf_data, hmac);
	if (memcmp(hmac, request->wrapped_leaf_data.hmac, sizeof(hmac)) != 0)
		return PW_ERR_HMAC_AUTH_FAILED;

	ret = decrypt_leaf_data(merkle_tree, &request->wrapped_leaf_data,
				&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	if (leaf_data.idat.label != request->leaf_location)
		return PW_ERR_LABEL_INVALID;

	if (safe_memcmp(request->reset_secret,
			leaf_data.idat.reset_secret,
			sizeof(request->reset_secret)) != 0)
		return PW_ERR_RESET_AUTH_FAILED;

	leaf_data.attempt_count = 0;

	ret = encrypt_leaf_data(merkle_tree, &leaf_data,
				&wrapped_leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	compute_hmac(merkle_tree, &wrapped_leaf_data, wrapped_leaf_data.hmac);

	compute_root_hash(merkle_tree, leaf_data.idat.label,
			  request->path_hashes, wrapped_leaf_data.hmac,
			  new_root);

	memcpy(&response->wrapped_leaf_data, &wrapped_leaf_data,
	       sizeof(wrapped_leaf_data));

	return ret;
}
