/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "common.h"
#include "cryptoc/p256.h"
#include "cryptoc/util.h"
#include "ec_commands.h"
#include "fpsensor.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state.h"
#include "fpsensor_utils.h"
#include "host_command.h"
#include "openssl/aes.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "util.h"

/* These must be included after the "openssl/aes.h" */
#include "crypto/fipsmodule/aes/internal.h"
#include "crypto/fipsmodule/modes/internal.h"

/* Last acquired frame (aligned as it is used by arbitrary binary libraries) */
uint8_t fp_buffer[FP_SENSOR_IMAGE_SIZE] FP_FRAME_SECTION __aligned(4);
/* Fingers templates for the current user */
uint8_t fp_template[FP_MAX_FINGER_COUNT]
		   [FP_ALGORITHM_TEMPLATE_SIZE] FP_TEMPLATE_SECTION;
/* Encryption/decryption buffer */
/* TODO: On-the-fly encryption/decryption without a dedicated buffer */
/*
 * Store the encryption metadata at the beginning of the buffer containing the
 * ciphered data.
 */
uint8_t fp_enc_buffer[FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE] FP_TEMPLATE_SECTION;
/* Salt used in derivation of positive match secret. */
uint8_t fp_positive_match_salt[FP_MAX_FINGER_COUNT]
			      [FP_POSITIVE_MATCH_SALT_BYTES];

/* Store the intermediate encrypted data for transfer & reuse purpose.*/
/* The data will be copied into fp_enc_buffer after commit. */
static uint8_t fp_xfer_buffer[FP_MAX_FINGER_COUNT]
			     [FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE];

struct positive_match_secret_state
	positive_match_secret_state = { .template_matched = FP_NO_SUCH_TEMPLATE,
					.readable = false,
					.deadline = {
						.val = 0,
					} };

/* Index of the last enrolled but not retrieved template. */
int8_t template_newly_enrolled = FP_NO_SUCH_TEMPLATE;
/* Number of used templates */
uint32_t templ_valid;
/* Bitmap of the templates with local modifications */
uint32_t templ_dirty;
/* Current user ID */
uint32_t user_id[FP_CONTEXT_USERID_WORDS];
/* Part of the IKM used to derive encryption keys received from the TPM. */
uint8_t tpm_seed[FP_CONTEXT_TPM_BYTES];
/* The GSC paring key. */
uint8_t pairing_key[FP_PK_LEN];
/* The auth nonce for CK. */
uint8_t auth_nonce[FP_CK_AUTH_NONCE_LEN];
/* Status of the FP context. */
uint32_t fp_context_status;
/* Status of the FP encryption engine. */
static uint32_t fp_encryption_status;

atomic_t fp_events;

uint32_t sensor_mode;

void fp_task_simulate(void)
{
	int timeout_us = -1;

	while (1)
		task_wait_event(timeout_us);
}

void fp_clear_finger_context(int idx)
{
	always_memset(fp_template[idx], 0, sizeof(fp_template[0]));
	always_memset(fp_positive_match_salt[idx], 0,
		      sizeof(fp_positive_match_salt[0]));
}

/**
 * @warning |fp_buffer| contains data used by the matching algorithm that must
 * be released by calling fp_sensor_deinit() first. Call
 * fp_reset_and_clear_context instead of calling this directly.
 */
static void _fp_clear_context(void)
{
	int idx;

	templ_valid = 0;
	templ_dirty = 0;
	fp_context_status = 0;
	template_newly_enrolled = FP_NO_SUCH_TEMPLATE;
	always_memset(fp_buffer, 0, sizeof(fp_buffer));
	always_memset(fp_enc_buffer, 0, sizeof(fp_enc_buffer));
	always_memset(user_id, 0, sizeof(user_id));
	always_memset(auth_nonce, 0, sizeof(auth_nonce));
	fp_disable_positive_match_secret(&positive_match_secret_state);
	for (idx = 0; idx < FP_MAX_FINGER_COUNT; idx++)
		fp_clear_finger_context(idx);
}

void fp_reset_and_clear_context(void)
{
	if (fp_sensor_deinit() != EC_SUCCESS)
		CPRINTS("Failed to deinit sensor");
	_fp_clear_context();
	if (fp_sensor_init() != EC_SUCCESS)
		CPRINTS("Failed to init sensor");
}

int fp_get_next_event(uint8_t *out)
{
	uint32_t event_out = atomic_clear(&fp_events);

	memcpy(out, &event_out, sizeof(event_out));

	return sizeof(event_out);
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_FINGERPRINT, fp_get_next_event);

static enum ec_status fp_command_tpm_seed(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_seed *params = args->params;

	if (params->struct_version != FP_TEMPLATE_FORMAT_VERSION) {
		CPRINTS("Invalid seed format %d", params->struct_version);
		return EC_RES_INVALID_PARAM;
	}

	if (fp_encryption_status & FP_ENC_STATUS_SEED_SET) {
		CPRINTS("Seed has already been set.");
		return EC_RES_ACCESS_DENIED;
	}
	memcpy(tpm_seed, params->seed, sizeof(tpm_seed));
	fp_encryption_status |= FP_ENC_STATUS_SEED_SET;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_SEED, fp_command_tpm_seed, EC_VER_MASK(0));

int fp_tpm_seed_is_set(void)
{
	return fp_encryption_status & FP_ENC_STATUS_SEED_SET;
}

static enum ec_status
fp_command_encryption_status(struct host_cmd_handler_args *args)
{
	struct ec_response_fp_encryption_status *r = args->response;

	r->valid_flags = FP_ENC_STATUS_SEED_SET;
	r->status = fp_encryption_status;
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ENC_STATUS, fp_command_encryption_status,
		     EC_VER_MASK(0));

static int validate_fp_mode(const uint32_t mode)
{
	uint32_t capture_type = FP_CAPTURE_TYPE(mode);
	uint32_t algo_mode = mode & ~FP_MODE_CAPTURE_TYPE_MASK;
	uint32_t cur_mode = sensor_mode;

	if (capture_type >= FP_CAPTURE_TYPE_MAX)
		return EC_ERROR_INVAL;

	if (algo_mode & ~FP_VALID_MODES)
		return EC_ERROR_INVAL;

	if ((mode & FP_MODE_ENROLL_SESSION) &&
	    templ_valid >= FP_MAX_FINGER_COUNT) {
		CPRINTS("Maximum number of fingers already enrolled: %d",
			FP_MAX_FINGER_COUNT);
		return EC_ERROR_INVAL;
	}

	/* Don't allow sensor reset if any other mode is
	 * set (including FP_MODE_RESET_SENSOR itself).
	 */
	if (mode & FP_MODE_RESET_SENSOR) {
		if (cur_mode & FP_VALID_MODES)
			return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

enum ec_status fp_set_sensor_mode(uint32_t mode, uint32_t *mode_output)
{
	int ret;

	if (mode_output == NULL)
		return EC_RES_INVALID_PARAM;

	ret = validate_fp_mode(mode);
	if (ret != EC_SUCCESS) {
		CPRINTS("Invalid FP mode 0x%x", mode);
		return EC_RES_INVALID_PARAM;
	}

	if (!(mode & FP_MODE_DONT_CHANGE)) {
		sensor_mode = mode;
		task_set_event(TASK_ID_FPSENSOR, TASK_EVENT_UPDATE_CONFIG);
	}

	*mode_output = sensor_mode;
	return EC_RES_SUCCESS;
}

static enum ec_status fp_command_mode(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_mode *p = args->params;
	struct ec_response_fp_mode *r = args->response;

	enum ec_status ret = fp_set_sensor_mode(p->mode, &r->mode);

	if (ret == EC_RES_SUCCESS)
		args->response_size = sizeof(*r);

	return ret;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_MODE, fp_command_mode, EC_VER_MASK(0));

static enum ec_status fp_command_context(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_context_v1 *p = args->params;
	uint32_t mode_output;

	switch (p->action) {
	case FP_CONTEXT_ASYNC:
		if (sensor_mode & FP_MODE_RESET_SENSOR)
			return EC_RES_BUSY;

		/**
		 * Trigger a call to fp_reset_and_clear_context() by
		 * requesting a reset. Since that function triggers a call to
		 * fp_sensor_open(), this must be asynchronous because
		 * fp_sensor_open() can take ~175 ms. See http://b/137288498.
		 */
		return fp_set_sensor_mode(FP_MODE_RESET_SENSOR, &mode_output);

	case FP_CONTEXT_GET_RESULT:
		if (sensor_mode & FP_MODE_RESET_SENSOR)
			return EC_RES_BUSY;

		if (fp_context_status & FP_CONTEXT_STATUS_NONCE_CONTEXT) {
			/* Clear the context to prevent downgrade attack. */
			_fp_clear_context();
		}

		memcpy(user_id, p->userid, sizeof(user_id));

		return EC_RES_SUCCESS;
	}

	return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_CONTEXT, fp_command_context, EC_VER_MASK(1));

int fp_enable_positive_match_secret(uint32_t fgr,
				    struct positive_match_secret_state *state)
{
	timestamp_t now;

	if (state->readable) {
		CPRINTS("Error: positive match secret already readable.");
		fp_disable_positive_match_secret(state);
		return EC_ERROR_UNKNOWN;
	}

	now = get_time();
	state->template_matched = fgr;
	state->readable = true;
	state->deadline.val = now.val + (5 * SECOND);
	return EC_SUCCESS;
}

void fp_disable_positive_match_secret(struct positive_match_secret_state *state)
{
	state->template_matched = FP_NO_SUCH_TEMPLATE;
	state->readable = false;
	state->deadline.val = 0;
}

static enum ec_status fp_read_match_secret(
	int8_t fgr,
	uint8_t positive_match_secret[FP_POSITIVE_MATCH_SECRET_BYTES])
{
	timestamp_t now = get_time();
	struct positive_match_secret_state state_copy =
		positive_match_secret_state;

	fp_disable_positive_match_secret(&positive_match_secret_state);

	if (fgr < 0 || fgr >= FP_MAX_FINGER_COUNT) {
		CPRINTS("Invalid finger number %d", fgr);
		return EC_RES_INVALID_PARAM;
	}
	if (timestamp_expired(state_copy.deadline, &now)) {
		CPRINTS("Reading positive match secret disallowed: "
			"deadline has passed.");
		return EC_RES_TIMEOUT;
	}
	if (fgr != state_copy.template_matched || !state_copy.readable) {
		CPRINTS("Positive match secret for finger %d is not meant to "
			"be read now.",
			fgr);
		return EC_RES_ACCESS_DENIED;
	}

	if (derive_positive_match_secret(positive_match_secret,
					 fp_positive_match_salt[fgr]) !=
	    EC_SUCCESS) {
		CPRINTS("Failed to derive positive match secret for finger %d",
			fgr);
		/* Keep the template and encryption salt. */
		return EC_RES_ERROR;
	}
	CPRINTS("Derived positive match secret for finger %d", fgr);

	return EC_RES_SUCCESS;
}

static enum ec_status
fp_command_read_match_secret(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_read_match_secret *params = args->params;
	struct ec_response_fp_read_match_secret *response = args->response;
	int8_t fgr = params->fgr;

	int ret = fp_read_match_secret(fgr, response->positive_match_secret);
	if (ret != EC_RES_SUCCESS) {
		return ret;
	}

	args->response_size = sizeof(*response);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_READ_MATCH_SECRET, fp_command_read_match_secret,
		     EC_VER_MASK(0));

BUILD_ASSERT(FP_PK_LEN == SHA256_DIGEST_SIZE);
BUILD_ASSERT(FP_PK_EC_PUBLIC_KEY_LEN == P256_BITSPERDIGIT);
BUILD_ASSERT(FP_PK_EC_PRIVATE_KEY_LEN == P256_BITSPERDIGIT);

static enum ec_status
fp_command_establish_pk_keygen(struct host_cmd_handler_args *args)
{
	struct ec_response_fp_establish_pk_keygen *r = args->response;

	r->enc_privkey_info.struct_version = FP_PK_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_rand_bytes(r->enc_privkey_info.nonce, FP_PK_NONCE_BYTES);
	trng_rand_bytes(r->enc_privkey_info.encryption_salt,
			FP_PK_ENCRYPTION_SALT_BYTES);
	trng_exit();

	p256_int p256_n, p256_x, p256_y;
	p256_from_bin(r->enc_privkey, &p256_n);
	p256_base_point_mul(&p256_n, &p256_x, &p256_y);
	p256_to_bin(&p256_x, r->pubkey_x);
	p256_to_bin(&p256_y, r->pubkey_y);

	/* Clear the private key. */
	always_memset(&p256_n, 0, sizeof(p256_n));

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret =
		derive_encryption_key(key, r->enc_privkey_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_keygen: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(key, SBP_ENC_KEY_LEN, r->enc_privkey,
			      r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN,
			      r->enc_privkey_info.nonce, FP_PK_NONCE_BYTES,
			      r->enc_privkey_info.tag, FP_PK_TAG_BYTES);
	always_memset(key, 0, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_keygen: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PK_KEYGEN,
		     fp_command_establish_pk_keygen, EC_VER_MASK(0));

static enum ec_status
fp_command_establish_pk_wrap(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_establish_pk_wrap *params = args->params;
	struct ec_response_fp_establish_pk_wrap *r = args->response;

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret = derive_encryption_key(
		key, params->enc_privkey_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];
	memcpy(privkey, params->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN);

	/* Decrypt the secret blob in-place. */
	ret = aes_gcm_decrypt(key, SBP_ENC_KEY_LEN, privkey, privkey,
			      FP_PK_EC_PRIVATE_KEY_LEN,
			      params->enc_privkey_info.nonce, FP_PK_NONCE_BYTES,
			      params->enc_privkey_info.tag, FP_PK_TAG_BYTES);
	always_memset(key, 0, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to decipher template");
		return EC_RES_UNAVAILABLE;
	}

	p256_int p256_n, p256_x, p256_y;
	p256_from_bin(privkey, &p256_n);
	p256_from_bin(params->peers_pubkey_x, &p256_x);
	p256_from_bin(params->peers_pubkey_y, &p256_y);
	/* Doing the calculation in-place. */
	p256_point_mul(&p256_n, &p256_x, &p256_y, &p256_x, &p256_y);
	p256_to_bin(&p256_x, r->enc_pk);

	/* Clear the private key and pk material. */
	always_memset(privkey, 0, sizeof(privkey));
	always_memset(&p256_n, 0, sizeof(p256_n));
	always_memset(&p256_x, 0, sizeof(p256_x));
	always_memset(&p256_y, 0, sizeof(p256_y));

	struct sha256_ctx ctx;
	SHA256_init(&ctx);
	SHA256_update(&ctx, r->enc_pk, FP_PK_LEN);
	uint8_t *pk = SHA256_final(&ctx);
	memcpy(r->enc_pk, pk, FP_PK_LEN);

	/* Clear the context that contain pk. */
	always_memset(&ctx, 0, sizeof(ctx));

	r->enc_pk_info.struct_version = FP_PK_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->enc_pk_info.nonce, FP_PK_NONCE_BYTES);
	trng_rand_bytes(r->enc_pk_info.encryption_salt,
			FP_PK_ENCRYPTION_SALT_BYTES);
	trng_exit();

	ret = derive_encryption_key(key, r->enc_pk_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(key, SBP_ENC_KEY_LEN, r->enc_pk, r->enc_pk,
			      FP_PK_LEN, r->enc_pk_info.nonce,
			      FP_PK_NONCE_BYTES, r->enc_pk_info.tag,
			      FP_PK_TAG_BYTES);
	always_memset(key, 0, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PK_WRAP, fp_command_establish_pk_wrap,
		     EC_VER_MASK(0));

static enum ec_status fp_command_load_pk(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_load_pk *params = args->params;

	/* Clear the context to prevent leaking the existing template. */
	_fp_clear_context();

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret =
		derive_encryption_key(key, params->enc_pk_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_load: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	memcpy(pairing_key, params->enc_pk, FP_PK_LEN);

	/* Decrypt the secret blob in-place. */
	ret = aes_gcm_decrypt(key, SBP_ENC_KEY_LEN, pairing_key, pairing_key,
			      FP_PK_EC_PRIVATE_KEY_LEN,
			      params->enc_pk_info.nonce, FP_PK_NONCE_BYTES,
			      params->enc_pk_info.tag, FP_PK_TAG_BYTES);
	always_memset(key, 0, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_load: Failed to decipher pk");
		return EC_RES_UNAVAILABLE;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_LOAD_PK, fp_command_load_pk, EC_VER_MASK(0));

static enum ec_status
fp_command_generate_nonce(struct host_cmd_handler_args *args)
{
	struct ec_response_fp_generate_nonce *r = args->response;

	if (fp_context_status & FP_CONTEXT_STATUS_NONCE_CONTEXT) {
		/* Clear the context to prevent leaking the data from previous
		 * nonce context.
		 */
		_fp_clear_context();
	}

	trng_init();
	trng_rand_bytes(auth_nonce, FP_CK_AUTH_NONCE_LEN);
	trng_exit();

	memcpy(r->nonce, auth_nonce, FP_CK_AUTH_NONCE_LEN);

	fp_context_status |= FP_CONTEXT_AUTH_NONCE_SET;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_GENERATE_NONCE, fp_command_generate_nonce,
		     EC_VER_MASK(0));

BUILD_ASSERT(FP_CONTEXT_KEY_LEN == FP_CONTEXT_USERID_LEN);

static enum ec_status
fp_command_nonce_context(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_nonce_context *p = args->params;

	if (!(fp_context_status & FP_CONTEXT_AUTH_NONCE_SET)) {
		CPRINTS("No existing auth nonce");
		return EC_RES_ACCESS_DENIED;
	}

	struct sha256_ctx ctx;
	SHA256_init(&ctx);
	SHA256_update(&ctx, auth_nonce, FP_CK_AUTH_NONCE_LEN);
	SHA256_update(&ctx, p->gsc_nonce, FP_CK_AUTH_NONCE_LEN);
	SHA256_update(&ctx, pairing_key, FP_PK_LEN);
	uint8_t *ck = SHA256_final(&ctx);

	AES_KEY aes_key;
	int res = AES_set_encrypt_key(ck, 256, &aes_key);
	if (res) {
		CPRINTS("Failed to set encryption key: %d", res);
		return EC_RES_UNAVAILABLE;
	}

	uint8_t aes_iv[FP_CONTEXT_USERID_IV_LEN];
	memcpy(aes_iv, p->enc_user_id_iv, FP_CONTEXT_USERID_IV_LEN);

	unsigned int block_num = 0;
	uint8_t raw_user_id[FP_CONTEXT_USERID_LEN];
	uint8_t ecount_buf[16];
	/* The AES CTR used the same function for encryption & decryption. */
	CRYPTO_ctr128_encrypt(p->enc_user_id, raw_user_id,
			      FP_CONTEXT_USERID_LEN, &aes_key, aes_iv,
			      ecount_buf, &block_num, (block128_f)AES_encrypt);

	/* Clear the key material. */
	always_memset(&aes_key, 0, sizeof(aes_key));
	always_memset(&ctx, 0, sizeof(ctx));

	if (p->clear_context) {
		/* Clear the previous context. */
		_fp_clear_context();
	}

	/* Set the user_id. */
	memcpy(user_id, raw_user_id, FP_CONTEXT_USERID_LEN);

	fp_context_status = FP_CONTEXT_STATUS_NONCE_CONTEXT;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_NONCE_CONTEXT, fp_command_nonce_context,
		     EC_VER_MASK(0));

BUILD_ASSERT(FP_POSITIVE_MATCH_SECRET_BYTES == SHA256_DIGEST_SIZE);

static enum ec_status
fp_command_read_match_secret_with_pubkey(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_read_match_secret_with_pubkey *params =
		args->params;
	struct ec_response_fp_read_match_secret_with_pubkey *response =
		args->response;
	int8_t fgr = params->fgr;

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];
	trng_init();
	trng_rand_bytes(privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_rand_bytes(response->iv, FP_EC_PUBLIC_KEY_IV_LEN);
	trng_exit();

	p256_int p256_n, p256_x, p256_y;
	p256_from_bin(privkey, &p256_n);

	p256_base_point_mul(&p256_n, &p256_x, &p256_y);

	p256_to_bin(&p256_x, response->pubkey_x);
	p256_to_bin(&p256_y, response->pubkey_y);

	p256_from_bin(params->pubkey_x, &p256_x);
	p256_from_bin(params->pubkey_y, &p256_y);
	p256_point_mul(&p256_n, &p256_x, &p256_y, &p256_x, &p256_y);
	p256_to_bin(&p256_x, share_secret);

	/* Clear the private key and share_secret material. */
	always_memset(privkey, 0, sizeof(privkey));
	always_memset(&p256_n, 0, sizeof(p256_n));
	always_memset(&p256_x, 0, sizeof(p256_x));
	always_memset(&p256_y, 0, sizeof(p256_y));

	/* Sha256 the share_secret. */
	uint8_t share_secret[FP_POSITIVE_MATCH_SECRET_BYTES];
	struct sha256_ctx ctx;
	SHA256_init(&ctx);
	SHA256_update(&ctx, share_secret, FP_POSITIVE_MATCH_SECRET_BYTES);
	uint8_t *tmp = SHA256_final(&ctx);

	AES_KEY aes_key;
	int res = AES_set_encrypt_key(tmp, 256, &aes_key);
	if (res) {
		CPRINTS("Failed to set encryption key: %d", res);
		return EC_RES_UNAVAILABLE;
	}

	uint8_t aes_iv[FP_CONTEXT_USERID_IV_LEN];
	memcpy(aes_iv, response->iv, FP_EC_PUBLIC_KEY_IV_LEN);

	res = fp_read_match_secret(fgr, share_secret);
	if (res != EC_RES_SUCCESS) {
		return res;
	}

	/* The AES CTR used the same function for encryption & decryption. */
	unsigned int block_num = 0;
	uint8_t ecount_buf[16];
	CRYPTO_ctr128_encrypt(share_secret, response->enc_secret,
			      FP_CONTEXT_USERID_LEN, &aes_key, aes_iv,
			      ecount_buf, &block_num, (block128_f)AES_encrypt);

	/* Clear the key material. */
	always_memset(&aes_key, 0, sizeof(aes_key));
	always_memset(&ctx, 0, sizeof(ctx));

	args->response_size = sizeof(*response);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_READ_MATCH_SECRET_WITH_PUBKEY,
		     fp_command_read_match_secret_with_pubkey, EC_VER_MASK(0));

static enum ec_status
fp_command_preload_template(struct host_cmd_handler_args *args)
{
	const struct ec_params_fp_preload_template *params = args->params;
	uint32_t size = params->size & ~FP_TEMPLATE_COMMIT;
	int xfer_complete = params->size & FP_TEMPLATE_COMMIT;
	uint32_t offset = params->offset;
	uint16_t idx = params->fgr;

	/* Can we store one more template ? */
	if (idx >= FP_MAX_FINGER_COUNT)
		return EC_RES_OVERFLOW;

	if (args->params_size !=
	    size + offsetof(struct ec_params_fp_preload_template, data))
		return EC_RES_INVALID_PARAM;

	int ret = validate_fp_buffer_offset(sizeof(fp_xfer_buffer[idx]), offset,
					    size);

	if (ret != EC_SUCCESS)
		return ret;

	memcpy(&fp_xfer_buffer[idx][offset], params->data, size);

	if (xfer_complete) {
		memcpy(fp_enc_buffer, fp_xfer_buffer[idx],
		       FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE);
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_PRELOAD_TEMPLATE, fp_command_preload_template,
		     EC_VER_MASK(0));
