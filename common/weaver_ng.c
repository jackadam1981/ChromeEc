/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <common.h>
#include <trng.h>
#include <weaver_ng.h>

/* Compile time sanity checks. */
_Static_assert(sizeof(hash_t) >= SHA256_DIGEST_SIZE,
	       "hash_t is too small for a SHA256 digest/");

_Static_assert(sizeof(leaf_data_t) == ((sizeof(leaf_data_t) + 15) & ~15),
	       "sizeof(leaf_data_t) % 16 should be zero");

_Static_assert(sizeof(((wng_message_t *)0)->data) ==
			       sizeof(((wng_message_t *)0)->data.raw),
	       "wng_message_t.data.raw should be the largest member of the "
			       "union");

int create_merkle_tree(param_logk_t param_logk, param_h_t param_h,
			merkle_tree_t *merkle_tree)
{
	uint16_t param_k = 1 << param_logk;
	hash_t child_hashes[param_k];
	param_h_t hx;
	uint16_t kx;

	if (param_logk > 8 || param_k < 1)
		return EC_ERROR_UNKNOWN;
	if (param_h > 16 || param_h < 1)
		return EC_ERROR_UNKNOWN;

	merkle_tree->param_logk = param_logk;
	merkle_tree->param_h = param_h;

	/* Initialize the root hash. */
	memset(child_hashes, 0, sizeof(child_hashes));
	memset(merkle_tree->root, 0, sizeof(merkle_tree->root));
	DCRYPTO_SHA256_hash(child_hashes[0], sizeof(child_hashes),
			    merkle_tree->root);
	for (hx = 1; hx < param_h; ++hx) {
		for (kx = 0; kx < param_k; ++kx) {
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

int store_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree)
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

index_t get_index(merkle_tree_t *merkle_tree, label_t label, param_h_t level) {
	index_t mask = ~((~(index_t)0) << merkle_tree->param_logk);
	uint8_t shift_by = (sizeof(label) << 3) -
			merkle_tree->param_logk * (level + 1);
	return (label >> shift_by) & mask;
}

int get_path_length(merkle_tree_t *merkle_tree)
{
	return (1 << merkle_tree->param_logk) * merkle_tree->param_h;
}

void compute_hmac(merkle_tree_t *merkle_tree,
		  wrapped_leaf_data_t *wrapped_leaf_data, hash_t *result)
{
	LITE_HMAC_CTX hmac;

	DCRYPTO_HMAC_SHA256_init(&hmac, merkle_tree->hmac_key,
				 sizeof(merkle_tree->hmac_key));
	HASH_update(&hmac.hash, wrapped_leaf_data->cipher_text,
		    sizeof(wrapped_leaf_data->cipher_text));
	memcpy(*result, DCRYPTO_HMAC_final(&hmac), sizeof(*result));
}

void compute_hash(hash_t (*hashes)[], uint16_t num_hashes, index_t location,
		  hash_t *child_hash, hash_t *result)
{
	memset(*result, 0, sizeof(*result));
	if (location > 0) {
		DCRYPTO_SHA256_hash((*hashes)[0],
				    sizeof((*hashes)[0]) * location, *result);
	}
	DCRYPTO_SHA256_hash(*child_hash, sizeof(*child_hash), *result);
	if (location < num_hashes) {
		DCRYPTO_SHA256_hash((*hashes)[location],
				    sizeof((*hashes)[0]) *
						    (num_hashes - location),
				    *result);
	}
}

void compute_root(merkle_tree_t *merkle_tree, label_t path,
		 hash_t (*hashes)[], hash_t *child_hash, hash_t *new_root)
{
	uint16_t param_k = 1 << merkle_tree->param_logk;
	/* Ugly way to convert a 1D array to a 2D one. */
	hash_t (*view)[merkle_tree->param_h][param_k] = (void *)hashes;
	hash_t temp_a;
	hash_t temp_b;
	param_h_t hx = 0;
	index_t index = get_index(merkle_tree, path,
				  merkle_tree->param_h - hx - 1);

	/* Case child_hash -> new_root */
	if (merkle_tree->param_h == 1) {
		compute_hash(&(*view)[hx], param_k - 1, index, child_hash,
			     new_root);
		return;
	}

	/* Case child_hash -> temp_a */
	compute_hash(&(*view)[hx], param_k - 1, index, child_hash, &temp_a);
	for (hx = 1; hx < merkle_tree->param_h - 1; ++hx) {
		/* Case temp_a -> temp_b */
		index = get_index(merkle_tree, path,
				  merkle_tree->param_h - hx - 1);
		compute_hash(&(*view)[hx], param_k - 1, index, &temp_a,
			     &temp_b);

		/* Unroll loop to alternate buffers. */
		++hx;
		if (hx < merkle_tree->param_h - 1)
			break;

		/* Case temp_b -> temp_a */
		index = get_index(merkle_tree, path,
				  merkle_tree->param_h - hx - 1);
		compute_hash(&(*view)[hx], param_k - 1, index, &temp_b,
			     &temp_a);
	}

	/* Handle last case temp_? -> new_root. */
	index = get_index(merkle_tree, path, 0);
	compute_hash(&(*view)[hx], param_k - 1, index,
		     ((hx & 0x1) == 0 ? &temp_b : &temp_a), new_root);
}

int authenticate_path(merkle_tree_t *merkle_tree, label_t path,
		      hash_t (*hashes)[], hash_t *child_hash)
{
	hash_t parent;

	compute_root(merkle_tree, path, hashes, child_hash, &parent);
	if (memcmp(parent, merkle_tree->root, sizeof(parent)) != 0)
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

int encrypt_leaf_data(merkle_tree_t *merkle_tree, leaf_data_t *leaf_data,
		      wrapped_leaf_data_t *wrapped_leaf_data)
{
	/* Generate a random IV. */
	rand_bytes(wrapped_leaf_data->iv, sizeof(wrapped_leaf_data->iv));
	if (!DCRYPTO_aes_ctr(wrapped_leaf_data->cipher_text,
			     merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv, (uint8_t *)leaf_data,
			     sizeof(leaf_data))) {
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int decrypt_leaf_data(merkle_tree_t *merkle_tree,
		      wrapped_leaf_data_t *wrapped_leaf_data,
		      leaf_data_t *leaf_data)
{
	if (!DCRYPTO_aes_ctr((uint8_t *)leaf_data, merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv,
			     wrapped_leaf_data->cipher_text,
			     sizeof(leaf_data))) {
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

/* Checks the following conditions:
 * Extra index fields should be all zero.
 */
static int validate_label(merkle_tree_t *merkle_tree, label_t path)
{
	uint8_t shift_by = merkle_tree->param_logk * merkle_tree->param_h;

	return (path & (~((label_t)0)) >> shift_by) == 0;
}

/* Checks the following conditions:
 * Columns should be strictly increasing.
 * Zeroes for filler at the end of the delay_schedule are permitted.
 */
static int validate_delay_schedule(delay_schedule_t *delay_schedule)
{
	size_t x;

	for (x = ARRAY_SIZE(*delay_schedule) - 1; x > 1; --x) {
		if ((*delay_schedule)[x].attempt_count == 0) {
			if ((*delay_schedule)[x].time_diff != 0)
				return EC_ERROR_UNKNOWN;
		} else if ((*delay_schedule)[x].attempt_count <=
				(*delay_schedule)[x - 1].attempt_count ||
				(*delay_schedule)[x].time_diff <=
				(*delay_schedule)[x - 1].time_diff) {
			return EC_ERROR_UNKNOWN;
		}
	}
	return EC_SUCCESS;
}

leaf_data_t leaf_data;

int wng_handle_message(merkle_tree_t *merkle_tree, wng_message_t *request,
		       wng_message_t *response,
		       hash_t *new_root)
{
	int32_t ret;

	if (request->header.version != WNG_PROTOCOL_VERSION)
		return EC_ERROR_UNKNOWN;

	/* Initialize new_root to the current root. */
	memcpy(*new_root, merkle_tree->root, sizeof(merkle_tree->root));

	response->header.version = WNG_PROTOCOL_VERSION;
	switch (request->header.type) {
	case WNG_MTQ_RESET_TREE:
		/* TODO(allenwebb) return a more meaningful error for the
		 * length checks.
		 */
		if (request->header.data_length !=
				sizeof(request->data.req_reset_tree))
			return EC_ERROR_UNKNOWN;
		response->header.type = WNG_MTA_RESET_TREE;
		response->header.data_length =
				sizeof(response->data.rsp_reset_tree);
		ret = wng_handle_reset_tree(merkle_tree,
					    &request->data.req_reset_tree,
					    &response->data.rsp_reset_tree,
					    new_root);
		break;
	case WNG_MTQ_INSERT_LEAF:
		if (request->header.data_length !=
				sizeof(request->data.req_insert_leaf) +
				get_path_length(merkle_tree))
			return EC_ERROR_UNKNOWN;
		response->header.type = WNG_MTA_INSERT_LEAF;
		response->header.data_length =
				sizeof(response->data.rsp_insert_leaf);
		ret = wng_handle_insert_leaf(merkle_tree,
					     &request->data.req_insert_leaf,
					     &response->data.rsp_insert_leaf,
					     new_root);
		if (ret == EC_SUCCESS)
			response->header.data_length +=
			    sizeof(*response->data.req_insert_leaf.path_hashes);
		break;
	case WNG_MTQ_REMOVE_LEAF:
		if (request->header.data_length !=
				sizeof(request->data.req_remove_leaf) +
				get_path_length(merkle_tree))
			return EC_ERROR_UNKNOWN;
		response->header.type = WNG_MTA_REMOVE_LEAF;
		response->header.data_length =
				sizeof(response->data.rsp_remove_leaf);
		ret = wng_handle_remove_leaf(merkle_tree,
					     &request->data.req_remove_leaf,
					     &response->data.rsp_remove_leaf,
					     new_root);
		break;
	case WNG_MTQ_TRY_AUTH:
		if (request->header.data_length !=
				sizeof(request->data.req_try_auth) +
				get_path_length(merkle_tree))
			return EC_ERROR_UNKNOWN;
		response->header.type = WNG_MTA_TRY_AUTH;
		response->header.data_length =
				sizeof(response->data.rsp_try_auth);
		ret = wng_handle_try_auth(merkle_tree,
					  &request->data.req_try_auth,
					  &response->data.rsp_try_auth,
					  new_root);
		break;
	case WNG_MTQ_RESET_AUTH:
		if (request->header.data_length !=
				sizeof(request->data.req_reset_auth) +
				get_path_length(merkle_tree))
			return EC_ERROR_UNKNOWN;
		response->header.type = WNG_MTA_RESET_AUTH;
		response->header.data_length =
				sizeof(response->data.rsp_reset_auth);
		ret = wng_handle_reset_auth(merkle_tree,
					    &request->data.req_reset_auth,
					    &response->data.rsp_reset_auth,
					    new_root);
		break;
	default:
		response->header.type = WNG_MT_ERROR_MSG;
		response->data.error.error_code = EC_ERROR_UNKNOWN;
		response->header.data_length = sizeof(response->data.error);
		ret = EC_ERROR_UNKNOWN;
		break;
	}
	return ret;
};

int wng_handle_reset_tree(merkle_tree_t *merkle_tree,
			  wng_request_reset_tree_t *request,
			  wng_response_reset_tree_t *response,
			  hash_t *new_root)
{
	int ret = create_merkle_tree(request->param_logk, request->param_h,
				     merkle_tree);
	if (ret == EC_SUCCESS)
		memcpy(*new_root, merkle_tree->root, sizeof(*new_root));
	response->result_code = ret;
	return ret;
}

int wng_handle_insert_leaf(merkle_tree_t *merkle_tree,
			   wng_request_insert_leaf_t *request,
			   wng_response_insert_leaf_t *response,
			   hash_t *new_root)
{
	int ret = EC_SUCCESS;
	hash_t leaf_hash = {};

	ret = validate_label(merkle_tree, request->idat.label);
	if (ret != EC_SUCCESS)
		goto cleanup;

	ret = validate_delay_schedule(&request->idat.delay_schedule);
	if (ret != EC_SUCCESS)
		goto cleanup;

	ret = authenticate_path(merkle_tree, request->idat.label,
				&request->path_hashes, &leaf_hash);
	if (ret != EC_SUCCESS)
		goto cleanup;

	memset(&leaf_data, 0, sizeof(leaf_data));
	leaf_data.version = WNG_STORAGE_VERSION;

	memcpy(&leaf_data.idat, &request->idat, sizeof(leaf_data.idat));

	ret = encrypt_leaf_data(merkle_tree, &leaf_data,
				response->wrapped_leaf_data);
	if (ret != EC_SUCCESS)
		goto cleanup;

	compute_hmac(merkle_tree, response->wrapped_leaf_data,
		     &response->wrapped_leaf_data->hmac);

	compute_root(merkle_tree, leaf_data.idat.label, &request->path_hashes,
		     &response->wrapped_leaf_data->hmac, new_root);

cleanup:
	response->result_code = ret;
	return ret;
}

int wng_handle_remove_leaf(merkle_tree_t *merkle_tree,
			   wng_request_remove_leaf_t *request,
			   wng_response_remove_leaf_t *response,
			   hash_t *new_root)
{
	int ret = EC_SUCCESS;
	hash_t empty_hash = {};

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		goto cleanup;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				&request->path_hashes, &request->leaf_hmac);
	if (ret != EC_SUCCESS)
		goto cleanup;

	compute_root(merkle_tree, request->leaf_location, &request->path_hashes,
		     &empty_hash, new_root);

cleanup:
	response->result_code = ret;
	return ret;
}

int wng_handle_try_auth(merkle_tree_t *merkle_tree,
			wng_request_try_auth_t *request,
			wng_response_try_auth_t *response,
			hash_t *new_root)
{
	int ret = EC_SUCCESS;
	hash_t hmac;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		goto cleanup;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				&request->path_hashes,
				&request->wrapped_leaf_data.hmac);
	if (ret != EC_SUCCESS)
		goto cleanup;

	compute_hmac(merkle_tree, &request->wrapped_leaf_data, &hmac);
	if (memcmp(hmac, response->wrapped_leaf_data.hmac, sizeof(hmac)) != 0) {
		ret = EC_ERROR_UNKNOWN;
		goto cleanup;
	}

	ret = decrypt_leaf_data(merkle_tree, &request->wrapped_leaf_data,
				&leaf_data);
	if (ret != EC_SUCCESS)
		goto cleanup;

cleanup:
	ret = EC_ERROR_UNIMPLEMENTED;
	response->result_code = ret;
	return ret;
}

int wng_handle_reset_auth(merkle_tree_t *merkle_tree,
			  wng_request_reset_auth_t *request,
			  wng_response_reset_auth_t *response,
			  hash_t *new_root)
{
	int ret = EC_SUCCESS;
	hash_t hmac;

	ret = validate_label(merkle_tree, request->leaf_location);
	if (ret != EC_SUCCESS)
		goto cleanup;

	ret = authenticate_path(merkle_tree, request->leaf_location,
				&request->path_hashes,
				&request->wrapped_leaf_data.hmac);
	if (ret != EC_SUCCESS)
		goto cleanup;

	compute_hmac(merkle_tree, &request->wrapped_leaf_data, &hmac);
	if (memcmp(hmac, response->wrapped_leaf_data.hmac, sizeof(hmac)) != 0) {
		ret = EC_ERROR_UNKNOWN;
		goto cleanup;
	}

	ret = decrypt_leaf_data(merkle_tree, &request->wrapped_leaf_data,
				&leaf_data);
	if (ret != EC_SUCCESS)
		goto cleanup;

cleanup:
	ret = EC_ERROR_UNIMPLEMENTED;
	response->result_code = ret;
	return ret;
}
