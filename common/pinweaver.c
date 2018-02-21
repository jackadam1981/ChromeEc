/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pinweaver.h>

#include <common.h>
#include <console.h>
#include <extension.h>
#include <hooks.h>
#include <pinweaver_tpm_imports.h>
#include <pinweaver_types.h>
#include <timer.h>
#include <tpm_vendor_cmds.h>
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

/******************************************************************************/
/* Basic operations required by the Merkle tree.
 */

/* Creates an empty Merkle_tree with the given parameters. */
static int create_merkle_tree(struct bits_per_level_t bits_per_level,
			      struct height_t height,
			      struct merkle_tree_t *merkle_tree)
{
	uint16_t fan_out = 1 << bits_per_level.v;
	uint8_t temp_a[PW_HASH_SIZE] = {};
	uint8_t temp_b[PW_HASH_SIZE];
	struct height_t hx;
	uint16_t kx;
	LITE_SHA256_CTX ctx;

	merkle_tree->bits_per_level = bits_per_level;
	merkle_tree->height = height;

	/* Initialize the root hash. */
	for (hx.v = 0; hx.v < height.v; ++hx.v) {
		uint8_t *src, *dst;

		if ((hx.v & 1) == 0) {
			src = temp_a;
			dst = temp_b;
		} else {
			src = temp_b;
			dst = temp_a;
		}
		if (hx.v == height.v - 1)
			dst = merkle_tree->root;

		DCRYPTO_SHA256_init(&ctx, 0);
		for (kx = 0; kx < fan_out; ++kx)
			HASH_update(&ctx, src, PW_HASH_SIZE);
		memcpy(dst, HASH_final(&ctx), PW_HASH_SIZE);
	}

	rand_bytes(merkle_tree->hmac_key, sizeof(merkle_tree->hmac_key));

	rand_bytes(merkle_tree->wrap_key, sizeof(merkle_tree->wrap_key));

	/* TODO(allenwebb) generate public private key pair */
	return EC_SUCCESS;
}

/* Computes the HMAC for an encrypted leaf using the key in the merkle_tree. */
static void compute_hmac(const struct merkle_tree_t *merkle_tree,
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

/* Computes the root hash for the specified path and child hash. */
static void compute_root_hash(const struct merkle_tree_t *merkle_tree,
			      struct label_t path,
			      const uint8_t hashes[][PW_HASH_SIZE],
			      const uint8_t child_hash[PW_HASH_SIZE],
			      uint8_t new_root[PW_HASH_SIZE])
{
	uint16_t fan_out = 1 << merkle_tree->bits_per_level.v;
	/* Ugly way to convert a 1D array to a 2D one. */
	const uint8_t (*view)[merkle_tree->height.v][fan_out - 1]
			[PW_HASH_SIZE] = (void *)hashes;
	uint8_t temp_a[PW_HASH_SIZE];
	uint8_t temp_b[PW_HASH_SIZE];
	struct height_t hx = {0};
	struct index_t index = get_index(
			merkle_tree, path,
			(struct height_t){merkle_tree->height.v - hx.v - 1});

	/* Case child_hash -> new_root */
	if (merkle_tree->height.v == 1) {
		compute_hash((*view)[hx.v], fan_out - 1, index, child_hash,
			     new_root);
		return;
	}

	/* Case child_hash -> temp_a */
	compute_hash((*view)[hx.v], fan_out - 1, index, child_hash, temp_a);
	for (hx.v = 1; hx.v < merkle_tree->height.v - 1; ++hx.v) {
		/* Case temp_a -> temp_b */
		index = get_index(
				merkle_tree, path,
				(struct height_t){merkle_tree->height.v -
						  hx.v - 1});
		compute_hash((*view)[hx.v], fan_out - 1, index, temp_a, temp_b);

		/* Unroll loop to alternate buffers. */
		++hx.v;
		if (hx.v >= merkle_tree->height.v - 1)
			break;

		/* Case temp_b -> temp_a */
		index = get_index(
				merkle_tree, path,
				(struct height_t){merkle_tree->height.v -
						  hx.v - 1});
		compute_hash((*view)[hx.v], fan_out - 1, index, temp_b, temp_a);
	}

	/* Handle last case temp_? -> new_root. */
	index = get_index(merkle_tree, path, (struct height_t){0});
	compute_hash((*view)[hx.v], fan_out - 1, index,
		     ((hx.v & 0x1) == 0 ? temp_b : temp_a),
		     new_root);
}

/* Checks to see the specified path is valid. The length of the path should be
 * validated prior to calling this function.
 *
 * Returns 0 on success or an error code otherwise.
 */
static int authenticate_path(const struct merkle_tree_t *merkle_tree,
			     struct label_t path,
			     const uint8_t hashes[][PW_HASH_SIZE],
			     const uint8_t child_hash[PW_HASH_SIZE])
{
	uint8_t parent[PW_HASH_SIZE];

	compute_root_hash(merkle_tree, path, hashes, child_hash, parent);
	if (memcmp(parent, merkle_tree->root, sizeof(parent)) != 0)
		return PW_ERR_PATH_AUTH_FAILED;
	return EC_SUCCESS;
}

/* Encrypts the leaf meta data. */
static int encrypt_leaf_data(const struct merkle_tree_t *merkle_tree,
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

/* Decrypts the leaf meta data. */
static int decrypt_leaf_data(
		const struct merkle_tree_t *merkle_tree,
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

/******************************************************************************/
/* Parameter and state validation functions.
 */

static int validate_tree_parameters(struct bits_per_level_t bits_per_level,
				    struct height_t height)
{
	uint8_t fan_out = 1 << bits_per_level.v;

	if (bits_per_level.v < BITS_PER_LEVEL_MIN ||
	    bits_per_level.v > BITS_PER_LEVEL_MAX)
		return PW_ERR_BITS_PER_LEVEL_INVALID;
	if (height.v < HEIGHT_MIN ||
	    height.v > HEIGHT_MAX(bits_per_level.v) ||
	    ((fan_out - 1) * height.v) * PW_HASH_SIZE > PW_MAX_PATH_SIZE)
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
static int validate_label(const struct merkle_tree_t *merkle_tree,
			  struct label_t path)
{
	uint8_t shift_by = merkle_tree->bits_per_level.v *
			   merkle_tree->height.v;

	if ((path.v & (~((struct label_t){0}).v) >> shift_by) == 0)
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
	if (delay_schedule[0].time_diff.v == 0)
		return PW_ERR_DELAY_SCHEDULE_INVALID;

	for (x = PW_SCHED_COUNT - 1; x > 0; --x) {
		if (delay_schedule[x].attempt_count.v == 0) {
			if (delay_schedule[x].time_diff.v != 0)
				return PW_ERR_DELAY_SCHEDULE_INVALID;
		} else if (delay_schedule[x].attempt_count.v <=
				delay_schedule[x - 1].attempt_count.v ||
				delay_schedule[x].time_diff.v <=
				delay_schedule[x - 1].time_diff.v) {
			return PW_ERR_DELAY_SCHEDULE_INVALID;
		}
	}
	return EC_SUCCESS;
}

/* Sets the value of ts to the current notion of time. */
static void update_timestamp(struct pw_timestamp_t *ts)
{
	ts->timer_value = get_time().val;
	ts->boot_count = get_restart_count();
}

/* Checks if an auth attempt can be made or not based on the delay schedule.
 * EC_SUCCESS is returned when a new attempt can be made.
 */
static int test_rate_limit(struct leaf_data_t *leaf_data)
{
	uint64_t ready_time;
	uint8_t x;
	struct pw_timestamp_t current_time;
	struct time_diff_t delay = {0};

	/* This loop ends when x is one greater than the index that applies. */
	for (x = 0; x < ARRAY_SIZE(leaf_data->idat.delay_schedule) &&
			leaf_data->idat.delay_schedule[x]
					.attempt_count.v != 0 &&
			leaf_data->attempt_count.v >=
			leaf_data->idat.delay_schedule[x]
					.attempt_count.v; ++x) {
	}

	if (x > 1)
		delay = leaf_data->idat.delay_schedule[x - 1].time_diff;

	if (delay.v == 0)
		return EC_SUCCESS;

	if (delay.v == PW_BLOCK_ATTEMPTS)
		return PW_ERR_RATE_LIMIT_REACHED;

	update_timestamp(&current_time);

	/* TODO(allenwebb) verify the timer starts at zero on reboot. */
	if (leaf_data->timestamp.boot_count == current_time.boot_count)
		ready_time = delay.v * SECOND +
				leaf_data->timestamp.timer_value;
	else
		ready_time = delay.v * SECOND;

	return current_time.timer_value > ready_time ? EC_SUCCESS :
	       PW_ERR_RATE_LIMIT_REACHED;
}

/******************************************************************************/
/* Per-request-type handler implementations.
 */

static int pw_handle_reset_tree(struct merkle_tree_t *merkle_tree,
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

static int pw_handle_insert_leaf(struct merkle_tree_t *merkle_tree,
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

static int pw_handle_remove_leaf(struct merkle_tree_t *merkle_tree,
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

static int pw_handle_try_auth(struct merkle_tree_t *merkle_tree,
			      const struct pw_request_try_auth_t *request,
			      struct pw_response_try_auth_t *response,
			      uint8_t new_root[PW_HASH_SIZE])
{
	int ret = EC_SUCCESS;
	struct attempt_count_t dummy_ac = {0};
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

	if (leaf_data.idat.label.v != request->leaf_location.v)
		return PW_ERR_LABEL_INVALID;

	ret = test_rate_limit(&leaf_data);
	if (ret != EC_SUCCESS)
		return ret;

	/* ret must not be overwritten after this. */
	if (safe_memcmp(request->low_entropy_secret,
			leaf_data.idat.low_entropy_secret,
			sizeof(request->low_entropy_secret)) != 0) {
		++leaf_data.attempt_count.v;
		dummy_ac.v = 0;
		ret = PW_ERR_LOWENT_AUTH_FAILED;
	} else {
		++dummy_ac.v;
		leaf_data.attempt_count.v = 0;
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

static int pw_handle_reset_auth(struct merkle_tree_t *merkle_tree,
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

	if (leaf_data.idat.label.v != request->leaf_location.v)
		return PW_ERR_LABEL_INVALID;

	if (safe_memcmp(request->reset_secret,
			leaf_data.idat.reset_secret,
			sizeof(request->reset_secret)) != 0)
		return PW_ERR_RESET_AUTH_FAILED;

	leaf_data.attempt_count.v = 0;

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

struct merkle_tree_t pw_merkle_tree;

/*
 * Handle the VENDOR_CC_WEAVER_NG command.
 */
static enum vendor_cmd_rc pw_vendor_specific_command(enum vendor_cmd_cc code,
						      void *buf,
						      size_t input_size,
						      size_t *response_size)
{
	const struct pw_request_t *request = buf;
	struct pw_response_t *response = buf;
	int ret;

	if (code != VENDOR_CC_PINWEAVER)
		return VENDOR_RC_BOGUS_ARGS;

	if (input_size != request->header.data_length + sizeof(request->header))
		return VENDOR_RC_REQUEST_TOO_BIG;

	ret = pw_handle_request(&pw_merkle_tree, request, response);

	/* TODO(allenwebb) store merkle_tree log update to flash here. */

	*response_size = response->header.data_length +
			 sizeof(response->header);

	return ret == EC_SUCCESS ? VENDOR_RC_SUCCESS : VENDOR_RC_INTERNAL_ERROR;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_PINWEAVER,
		pw_vendor_specific_command);

static void pinweaver_init(void)
{
	/* TODO(allenwebb) load merkle_tree from flash here. */
}
DECLARE_HOOK(HOOK_INIT, pinweaver_init, HOOK_PRIO_LAST);

/******************************************************************************/
/* Non-static functions.
 */

struct index_t get_index(const struct merkle_tree_t *merkle_tree,
			 struct label_t label, struct height_t level)
{
	struct index_t mask = {~((~((struct index_t){0}).v) <<
					       merkle_tree->bits_per_level.v)};
	uint8_t shift_by = (sizeof(label) << 3) -
			   merkle_tree->bits_per_level.v * (level.v + 1);
	return (struct index_t){((label.v >> shift_by) & mask.v)};
}

int get_path_length(const struct merkle_tree_t *merkle_tree)
{
	return ((1 << merkle_tree->bits_per_level.v) - 1) *
			merkle_tree->height.v;
}

void compute_hash(const uint8_t hashes[][PW_HASH_SIZE], uint16_t num_hashes,
		  struct index_t location,
		  const uint8_t child_hash[PW_HASH_SIZE],
		  uint8_t result[PW_HASH_SIZE])
{
	LITE_SHA256_CTX ctx;

	DCRYPTO_SHA256_init(&ctx, 0);
	if (location.v > 0)
		HASH_update(&ctx, hashes[0], PW_HASH_SIZE * location.v);
	HASH_update(&ctx, child_hash, PW_HASH_SIZE);
	if (location.v < num_hashes)
		SHA256_update(&ctx, hashes[location.v],
			      PW_HASH_SIZE * (num_hashes - location.v));
	memcpy(result, HASH_final(&ctx), PW_HASH_SIZE);
}

/* Handles the message in request using the context in merkle_tree and writes
 * the results to response. The return value captures any error conditions that
 * occurred or EC_SUCCESS if there were no errors.
 *
 * This implementation is written to handle the case where request and response
 * exist at the same memory location---are backed by the same buffer. This means
 * the implementation requires that no reads are made to request after response
 * has been written to.
 */
int pw_handle_request(struct merkle_tree_t *merkle_tree,
		      const struct pw_request_t *request,
		      struct pw_response_t *response)
{
	int32_t ret;
	/* Store state needed for the response until the contents of request are
	 * no longer needed.
	 */
	struct pw_response_header_t header;
	/* Store the message type of the request since it may be overwritten
	 * inside the switch whenever response and request overlap in memory.
	 */
	struct pw_message_type_t type = request->header.type;

	header.version = PW_PROTOCOL_VERSION;
	header.type.v = PW_MT_ERROR_MSG;
	header.data_length = 0;
	/* Initialize new_root to the current root. */
	memcpy(header.root, merkle_tree->root, sizeof(merkle_tree->root));

	if (request->header.version != PW_PROTOCOL_VERSION) {
		ret = PW_ERR_VERSION_MISMATCH;
		goto cleanup;
	}

	switch (type.v) {
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
			header.type.v = PW_MTA_RESET_TREE;
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
			header.type.v = PW_MTA_INSERT_LEAF;
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
			header.type.v = PW_MTA_REMOVE_LEAF;
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
			header.type.v = PW_MTA_TRY_AUTH;
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
			header.type.v = PW_MTA_RESET_AUTH;
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
