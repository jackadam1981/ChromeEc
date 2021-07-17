/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* APDU dispatcher and U2F command handlers. */

#include "console.h"
#include "cryptoc/p256.h"

#ifndef TEST_BUILD
#include "cryptoc/sha256.h"
#endif

#include "dcrypto.h"
#include "extension.h"
#include "nvmem_vars.h"
#include "system.h"
#include "tpm_nvmem_ops.h"
#include "tpm_vendor_cmds.h"
#include "u2f.h"
#include "u2f_impl.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ##args)

/* For test/u2f.c we provide a mock-up implementation of u2f_get_state() */
#ifndef U2F_TEST
static const uint8_t k_salt = NVMEM_VAR_G2F_SALT;
static const uint8_t k_salt_deprecated = NVMEM_VAR_U2F_SALT;

static int u2f_load_state(struct u2f_state *state)
{
	const struct tuple *t_salt = getvar(&k_salt, sizeof(k_salt));

	if (!t_salt) {
		/* Delete the old salt if present, no-op if not. */
		if (setvar(&k_salt_deprecated, sizeof(k_salt_deprecated), NULL,
			   0))
			return 0;

		/* create random salt */
		if (!DCRYPTO_ladder_random(state->salt))
			return 0;
		if (setvar(&k_salt, sizeof(k_salt),
			   (const uint8_t *)state->salt, sizeof(state->salt)))
			return 0;
	} else {
		memcpy(state->salt, tuple_val(t_salt), sizeof(state->salt));
		freevar(t_salt);
	}

	if (read_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KEK, sizeof(state->salt_kek),
				  state->salt_kek) == TPM_READ_NOT_FOUND) {
		/*
		 * Not found means that we have not used u2f before,
		 * or not used it with updated fw that resets kek seed
		 * on TPM clear.
		 */
		if (t_salt) { /* Note that memory has been freed already!. */
			/*
			 * We have previously used u2f, and may have
			 * existing registrations; we don't want to
			 * invalidate these, so preserve the existing
			 * seed as a one-off. It will be changed on
			 * next TPM clear.
			 */
			memcpy(state->salt_kek, state->salt,
			       sizeof(state->salt_kek));
		} else {
			/*
			 * We have never used u2f before - generate
			 * new seed.
			 */
			if (!DCRYPTO_ladder_random(state->salt_kek))
				return 0;
		}
		if (write_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KEK,
					   sizeof(state->salt_kek),
					   state->salt_kek,
					   1 /* commit */) != TPM_WRITE_CREATED)
			return 0;
	}

	if (read_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KH_SALT,
				  sizeof(state->salt_kh),
				  state->salt_kh) == TPM_READ_NOT_FOUND) {
		/*
		 * We have never used u2f before - generate
		 * new seed.
		 */
		if (!DCRYPTO_ladder_random(state->salt_kh))
			return 0;

		if (write_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KH_SALT,
					   sizeof(state->salt_kh),
					   state->salt_kh,
					   1 /* commit */) != TPM_WRITE_CREATED)
			return 0;
	}

	return 1;
}

/**
 * Get the current u2f state from the board.
 */
struct u2f_state *u2f_get_state(void)
{
	static int state_loaded;
	static struct u2f_state state;

	if (!state_loaded)
		state_loaded = u2f_load_state(&state);

	return state_loaded ? &state : NULL;
}

int u2f_gen_kek_seed(int commit)
{
	struct u2f_state *state = u2f_get_state();

	if (!state)
		return EC_ERROR_UNKNOWN;

	if (!u2f_generate_hmac_key(state))
		return EC_ERROR_HW_INTERNAL;

	if (write_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KEK, sizeof(state->salt_kek),
				   state->salt_kek, commit) == TPM_WRITE_FAIL)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

#endif /* U2F_TEST */

int g2f_attestation_cert(uint8_t *buf)
{
	uint8_t *serial;

	const struct u2f_state *state = u2f_get_state();

	if (!state)
		return 0;

	if (system_get_chip_unique_id(&serial) != P256_NBYTES)
		return 0;

	return g2f_attestation_cert_serial(state, serial, buf);
}

/* U2F GENERATE command  */
enum vendor_cmd_rc u2f_generate_cmd(enum vendor_cmd_cc code, void *buf,
				    size_t input_size, size_t *response_size)
{
	struct u2f_generate_req *req = buf;
	struct u2f_generate_resp *resp = buf;
	struct u2f_generate_versioned_resp *resp_versioned = buf;
	struct u2f_ec_point *pubKey;

	const struct u2f_state *state = u2f_get_state();
	uint8_t kh_version =
		(req->flags & U2F_UV_ENABLED_KH) ? U2F_KH_VERSION_1 : 0;

	/* Buffer for generating key handle as part of response. */
	union u2f_key_handle_variant *kh_buf;

	size_t response_buf_size = *response_size;

	*response_size = 0;

	if (input_size != sizeof(struct u2f_generate_req))
		return VENDOR_RC_BOGUS_ARGS;

	if (kh_version == 0) {
		if (response_buf_size < sizeof(struct u2f_generate_resp))
			return VENDOR_RC_BOGUS_ARGS;
		pubKey = &resp->pubKey;
		kh_buf = (union u2f_key_handle_variant *)&resp->keyHandle;
	} else {
		if (response_buf_size <
		    sizeof(struct u2f_generate_versioned_resp))
			return VENDOR_RC_BOGUS_ARGS;
		pubKey = &resp_versioned->pubKey;
		kh_buf = (union u2f_key_handle_variant *)&resp_versioned
				 ->keyHandle;
	}

	if (state == NULL)
		return VENDOR_RC_INTERNAL_ERROR;

	/* Maybe enforce user presence, w/ optional consume */
	if (pop_check_presence(req->flags & G2F_CONSUME) != POP_TOUCH_YES &&
	    (req->flags & U2F_AUTH_FLAG_TUP) != 0)
		return VENDOR_RC_NOT_ALLOWED;

	if (!u2f_generate(state, req->userSecret, req->appId,
			  req->authTimeSecretHash, kh_buf, kh_version, pubKey))
		return VENDOR_RC_INTERNAL_ERROR;

	/*
	 * From this point: the request 'req' content is invalid as it is
	 * overridden by the response we are building in the same buffer.
	 */
	if (kh_version == 0) {
		*response_size = sizeof(struct u2f_generate_resp);
	} else {
		*response_size = sizeof(struct u2f_generate_versioned_resp);
	}

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2F_GENERATE, u2f_generate_cmd);

/* Below, we depend on the response not being larger than than the request. */
BUILD_ASSERT(sizeof(struct u2f_sign_resp) <= sizeof(struct u2f_sign_req));

/* U2F SIGN command */
enum vendor_cmd_rc u2f_sign_cmd(enum vendor_cmd_cc code, void *buf,
				size_t input_size, size_t *response_size)
{
	const struct u2f_sign_req *req = buf;
	const struct u2f_sign_versioned_req *req_versioned = buf;
	union u2f_key_handle_variant *kh;

	const struct u2f_state *state = u2f_get_state();

	const uint8_t *hash, *user, *origin;

	uint8_t flags;
	struct u2f_sign_resp *resp;

	/* Version of KH; 0 if KH is not versioned. */
	uint8_t kh_version;

	/* Response is smaller than request, so no need to check this. */
	*response_size = 0;
	if (!state)
		return VENDOR_RC_INTERNAL_ERROR;

	/**
	 * Request can be in old (non-versioned) and new (versioned) formats,
	 * which differs in size. Use request size to distinguish it.
	 */
	if (input_size == sizeof(struct u2f_sign_req)) {
		kh_version = 0;
		kh = (union u2f_key_handle_variant *)&req->keyHandle;
		hash = req->hash;
		flags = req->flags;
		user = req->userSecret;
		origin = req->appId;
	} else if (input_size == sizeof(struct u2f_sign_versioned_req)) {
		kh_version = req_versioned->keyHandle.header.version;
		kh = (union u2f_key_handle_variant *)&req_versioned->keyHandle;
		hash = req_versioned->hash;
		flags = req_versioned->flags;
		user = req_versioned->userSecret;
		origin = req_versioned->appId;
	} else {
		return VENDOR_RC_BOGUS_ARGS;
	}

	/* We might not actually need to sign anything. */
	if ((flags & U2F_AUTH_CHECK_ONLY) == U2F_AUTH_CHECK_ONLY) {
		if (!u2f_authorize_keyhandle(state, kh, kh_version, user,
					     origin))
			return VENDOR_RC_PASSWORD_REQUIRED;
		return VENDOR_RC_SUCCESS;
	}

	/*
	 * Enforce user presence for version 0 KHs, with optional consume.
	 */
	if (pop_check_presence(flags & G2F_CONSUME) != POP_TOUCH_YES) {
		if (kh_version != U2F_KH_VERSION_1)
			return VENDOR_RC_NOT_ALLOWED;
		if ((flags & U2F_AUTH_FLAG_TUP) != 0)
			return VENDOR_RC_NOT_ALLOWED;
		/*
		 * TODO(yichengli): When auth-time secrets is ready, enforce
		 * authorization hmac when no power button press.
		 */
	}

	/*
	 * u2f_sign first consume all data from request 'req', and compute
	 * result in temporary storage. Once accomplished, it stores it in
	 * provided buffer. This allows overlap between input and output
	 * parameters.
	 * The response is smaller than the request, so we have the space.
	 */
	resp = buf;

	if (!u2f_sign(state, kh, kh_version, user, origin, hash, &resp->sig))
		return VENDOR_RC_INTERNAL_ERROR;

	*response_size = sizeof(*resp);

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2F_SIGN, u2f_sign_cmd);

static inline size_t u2f_attest_format_size(uint8_t format)
{
	switch (format) {
	case U2F_ATTEST_FORMAT_REG_RESP:
		return sizeof(struct g2f_register_msg);
	default:
		return 0;
	}
}

/* U2F ATTEST command */
static enum vendor_cmd_rc u2f_attest_cmd(enum vendor_cmd_cc code, void *buf,
					 size_t input_size,
					 size_t *response_size)
{
	const struct u2f_attest_req *req = buf;
	struct u2f_attest_resp *resp;
	struct g2f_register_msg *msg = (void *)req->data;

	size_t response_buf_size = *response_size;

	const struct u2f_state *state = u2f_get_state();

	*response_size = 0;

	if (!state)
		return VENDOR_RC_INTERNAL_ERROR;

	if (input_size < offsetof(struct u2f_attest_req, data) ||
	    input_size <
		    (offsetof(struct u2f_attest_req, data) + req->dataLen) ||
	    input_size > sizeof(struct u2f_attest_req) ||
	    response_buf_size < sizeof(*resp))
		return VENDOR_RC_BOGUS_ARGS;

	/* Only one format is supported, key handle version is 0 */
	if (req->format != U2F_ATTEST_FORMAT_REG_RESP)
		return VENDOR_RC_NOT_ALLOWED;

	if (req->dataLen != sizeof(struct g2f_register_msg))
		return VENDOR_RC_NOT_ALLOWED;

	if (msg->reserved != 0)
		return VENDOR_RC_NOT_ALLOWED;

	/*
	 * u2f_attest first consume all data from request 'req', and compute
	 * result in temporary storage. Once accomplished, it stores it in
	 * provided buffer. This allows overlap between input and output
	 * parameters.
	 * The response is smaller than the request, so we have the space.
	 */
	resp = buf;
	if (!u2f_attest(state, (union u2f_key_handle_variant *)&msg->key_handle,
			0, req->userSecret, msg->app_id, &msg->public_key,
			req->data, u2f_attest_format_size(req->format),
			&resp->sig)) {
		CPRINTF("G2F Attestation failed");
		return VENDOR_RC_NOT_ALLOWED;
	}

	*response_size = sizeof(*resp);
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2F_ATTEST, u2f_attest_cmd);
