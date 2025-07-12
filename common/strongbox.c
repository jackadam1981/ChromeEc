/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "console.h"
#include "extension.h"
#include "link_defs.h"
#include "strongbox.h"
#include "util.h"
#include "dcrypto.h"
#include "internal.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ##args)

uint32_t extension_route_strongbox_command(struct vendor_cmd_params *p)
{
	const struct strongbox_command *cmd_p;
	const struct strongbox_command *end_p;

#ifdef DEBUG_EXTENSION
	CPRINTS("%s(%d,%s) is=%d os=%d", __func__, p->code,
		p->flags & VENDOR_CMD_FROM_USB ? "USB" : "AP", p->in_size,
		p->out_size);
#endif
	/* Check that command came from valid interface in a valid state. */
	if ((p->flags & (VENDOR_CMD_FROM_USB | VENDOR_CMD_FROM_ALT_IF))
#ifdef CONFIG_BOARD_ID_SUPPORT
	    || board_id_is_mismatched()
#endif
	)
		return SBERR_HardwareNotYetAvailable;

	/* Find the command handler */
	cmd_p = (const struct strongbox_command *)&__strongbox_cmds;
	end_p = (const struct strongbox_command *)&__strongbox_cmds_end;
	while (cmd_p != end_p) {
		if (cmd_p->command_code == p->code)
			return cmd_p->handler(p);
		cmd_p++;
	}

	/* Command not found or not allowed */
	p->out_size = 0;
	return SBERR_Unimplemented;
}

typedef struct {
	uint32_t purpose_flags;
	keymint_algorithm_t algorithm;
	uint32_t key_size;
	keymint_ec_curve_t curve_id;
	keymint_digest_t digest;
	bool caller_nonce;
	uint64_t rsa_exponent;
} key_attributes_t;

// Corresponds to `KeyAttributes` struct. Filled during tag parsing.
typedef struct {
	key_attributes_t attrs;
	const uint8_t *application_id;
	size_t application_id_len;
	const uint8_t *application_data;
	size_t application_data_len;
	const uint8_t *attestation_challenge;
	size_t attestation_challenge_len;
} key_parameters_t;

typedef struct {
	uint32_t operation_id;
	key_attributes_t attrs;
	keymint_purpose_t purpose;
	uint32_t key[8];
	uint32_t update_size;
	uint32_t update_context[16];
} key_operation_t;

#define KM_MAX_OPS 4

// Corresponds to `Keymint` struct. Holds device state.
typedef struct {
	uint32_t os_version;
	uint32_t os_patchlevel;
	uint32_t vendor_patchlevel;
	uint32_t boot_patchlevel;
	uint32_t hmac_tag_key[8];
	uint32_t drbg_seed_key[8];
	uint32_t used_slots;
	key_operation_t ops[KM_MAX_OPS];
} keymint_t;

/* Keymint context */
static keymint_t km;

enum strongbox_error sb_GetHardwareInfo(struct vendor_cmd_params *p)
{
	static const uint8_t r[35] = { /* version */
				       0x00, 0x00, 0x00, 0x00,
				       /* keymint_security_level_t Strongbox */
				       0x00, 0x00, 0x00, 0x02,
				       /* Keymint name */
				       0x00, 0x04, 'C', 'R', '5', '0',
				       /* Key mint author */
				       0x00, 0x06, 'G', 'O', 'O', 'G', 'L', 'E',
				       /* timestamp_token_required = false */
				       0x00
	};

	p->out_size = 0;
	if (p->in_size)
		return SBERR_InvalidArgument;

	p->out_size = sizeof(r);
	memcpy(p->buffer, &r, sizeof(r));
	return SB_OK;
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceGetHardwareInfo, sb_GetHardwareInfo);

/**
 * @brief Checks if a tag is disallowed during key generation/import.
 */
static bool tag_is_gen_disallowed(keymint_tag_t tag)
{
	switch (tag) {
	case KM_TAG_INVALID:
	case KM_TAG_MAX_USES_PER_BOOT:
	case KM_TAG_MIN_SECONDS_BETWEEN_OPS:
	case KM_TAG_ORIGIN:
	case KM_TAG_ROOT_OF_TRUST:
	case KM_TAG_OS_VERSION:
	case KM_TAG_OS_PATCHLEVEL:
	case KM_TAG_RESET_SINCE_ID_ROTATION:
		return true;
	default:
		return false;
	}
}

/**
 * @brief Checks if a tag is software-enforced.
 */
static bool tag_is_sw_enforced(keymint_tag_t tag)
{
	/* Any tag not HW-enforced is considered SW-enforced for this
	 * translation. */
	switch (tag) {
	case KM_TAG_ACTIVE_DATETIME:
	case KM_TAG_ORIGINATION_EXPIRE_DATETIME:
	case KM_TAG_USAGE_EXPIRE_DATETIME:
	case KM_TAG_USER_ID:
	case KM_TAG_ALLOW_WHILE_ON_BODY:
	case KM_TAG_CREATION_DATETIME:
	case KM_TAG_ATTESTATION_APPLICATION_ID:
	case KM_TAG_MAX_BOOT_LEVEL:
		return true;
	default:
		return false;
	}
	/* return !tag_is_hw_enforced(tag); */
}

/**
 * @brief Checks if a tag is hardware-enforced.
 */
static bool tag_is_hw_enforced(keymint_tag_t tag)
{
	/* For this translation, we assume tags that are not SW-enforced and not
	 * special input tags are HW-enforced. A more robust implementation
	 * would use a complete list. */
	if (tag_is_sw_enforced(tag)) {
		return false;
	}
	/* Tags that are only for input and don't appear in characteristics */
	switch (tag) {
	case KM_TAG_APPLICATION_ID:
	case KM_TAG_APPLICATION_DATA:
	case KM_TAG_ATTESTATION_CHALLENGE: /* and other ATTESTATION_ID_* tags */
		return false;
	default:
		return true;
	}
}

static enum strongbox_error process_tag(keymint_tag_t tag,
					key_parameters_t *params,
					const uint32_t *p_tag)
{
	uint32_t tag_word = p_tag[0];

	switch (tag) {
	case KM_TAG_ROLLBACK_RESISTANCE:
		return SBERR_RollbackResistanceUnavailable;
	case KM_TAG_ALGORITHM:
		if (params->attrs.algorithm != 0)
			return SBERR_InvalidTag;
		params->attrs.algorithm = (keymint_algorithm_t)p_tag[1];
		break;
	case KM_TAG_KEY_SIZE:
		if (params->attrs.key_size != 0)
			return SBERR_InvalidTag;
		params->attrs.key_size = p_tag[1];
		break;
	case KM_TAG_EC_CURVE:
		if (params->attrs.curve_id != 0)
			return SBERR_InvalidTag;
		params->attrs.curve_id = p_tag[1];
		break;

	case KM_TAG_DIGEST:
		if (params->attrs.digest != 0)
			return SBERR_InvalidTag;
		params->attrs.digest = p_tag[1];
		break;

	case KM_TAG_RSA_PUBLIC_EXPONENT:
		if (params->attrs.rsa_exponent != 0)
			return SBERR_InvalidTag;
		memcpy(&params->attrs.rsa_exponent, &p_tag[1],
		       sizeof(uint64_t));
		break;
	case KM_TAG_CALLER_NONCE:
		if (params->attrs.caller_nonce)
			return SBERR_InvalidTag;
		params->attrs.caller_nonce = true;
		break;
	case KM_TAG_PURPOSE: {
		uint32_t purpose = p_tag[1];
		if (purpose >= 32)
			return SBERR_InvalidTag;
		params->attrs.purpose_flags |= (1 << purpose);
		break;
	}
	case KM_TAG_APPLICATION_ID:
		if (params->application_id != NULL)
			return SBERR_InvalidTag;
		params->application_id_len =
			prefix_get_data_len_bytes(tag_word);
		params->application_id = (const uint8_t *)&p_tag[1];
		break;
	case KM_TAG_APPLICATION_DATA:
		if (params->application_data != NULL)
			return SBERR_InvalidTag;
		params->application_data_len =
			prefix_get_data_len_bytes(tag_word);
		params->application_data = (const uint8_t *)&p_tag[1];
		break;
	case KM_TAG_ATTESTATION_CHALLENGE:
		if (params->attestation_challenge != NULL)
			return SBERR_InvalidTag;
		params->attestation_challenge_len =
			prefix_get_data_len_bytes(tag_word);
		params->attestation_challenge = (const uint8_t *)&p_tag[1];
		break;

	default:
		break;
	}
	return SB_OK;
}

// --- Static Helper Functions ---
static enum strongbox_error process_gen_import_tags(
	keymint_t *km, key_parameters_t *params, const uint32_t *buf,
	size_t req_len_words, uint32_t *out, size_t out_len_words,
	size_t *out_written_words)
{
	size_t tags_len_words;
	size_t hw_tag_start;
	size_t sw_enforced_tag_size_words;
	size_t tag_pos;
	size_t sw_out_start;
	size_t sw_tag_write_pos;

	if (req_len_words < 1 || out_len_words < 14) {
		return SBERR_InvalidTag;
	}

	memset(params, 0, sizeof(*params));

	tags_len_words = buf[0];
	if (tags_len_words > req_len_words - 1) {
		return SBERR_InvalidTag;
	}

	// These tags go into TEE/Strongbox KeyCharacteristics
	out[0] = (uint32_t)SECURITY_LEVEL_STRONGBOX;
	// out[1] is length, filled later
	out[2] = (uint32_t)KM_TAG_ORIGIN;
	// out[3] is keymint_key_origin_t, filled by caller
	out[4] = (uint32_t)KM_TAG_OS_VERSION;
	out[5] = km->os_version;
	out[6] = (uint32_t)KM_TAG_OS_PATCHLEVEL;
	out[7] = km->os_patchlevel;
	out[8] = (uint32_t)KM_TAG_VENDOR_PATCHLEVEL;
	out[9] = km->vendor_patchlevel;
	out[10] = (uint32_t)KM_TAG_BOOT_PATCHLEVEL;
	out[11] = km->boot_patchlevel;

	hw_tag_start = 12;
	sw_enforced_tag_size_words = 0;
	tag_pos = 1; // start after length word

	// Pass 1: Parse tags, copy HW tags, count SW tags, extract attributes
	while (tag_pos < tags_len_words + 1) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);
		enum strongbox_error err;

		if (tag_is_gen_disallowed(tag)) {
			return SBERR_InvalidTag;
		}

		err = process_tag(tag, params, p_tag);
		if (err != SB_OK)
			return err;

		if (tag_is_hw_enforced(tag)) {
			if (hw_tag_start + word_len > out_len_words) {
				return SBERR_InvalidArgument; // Not enough
							      // space
			}
			memcpy(&out[hw_tag_start], p_tag,
			       word_len * sizeof(uint32_t));
			hw_tag_start += word_len;
		} else if (tag_is_sw_enforced(tag)) {
			sw_enforced_tag_size_words += word_len;
		}
		tag_pos += word_len;
	}

	if ((params->attrs.purpose_flags & (1 << KEY_PURPOSE_ATTEST_KEY)) !=
		    0 &&
	    (params->attrs.purpose_flags != (1 << KEY_PURPOSE_ATTEST_KEY))) {
		return SBERR_IncompatiblePurpose;
	}

	if (params->attrs.algorithm == 0)
		return SBERR_InvalidArgument;

	out[1] = (hw_tag_start - 2); // Set length of HW-enforced tags

	if (out_len_words < hw_tag_start + 2)
		return SBERR_InvalidArgument;

	// Mark start of SW-enforced tags
	out[hw_tag_start] = SECURITY_LEVEL_KEYSTORE;
	out[hw_tag_start + 1] = sw_enforced_tag_size_words;

	sw_out_start = hw_tag_start + 2;
	if (out_len_words < sw_out_start + sw_enforced_tag_size_words) {
		return SBERR_InvalidArgument;
	}

	// Pass 2: Copy SW enforced tags
	sw_tag_write_pos = 0;
	tag_pos = 1;
	while (tag_pos < tags_len_words + 1) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);

		if (tag_is_sw_enforced(tag)) {
			if (sw_out_start + sw_tag_write_pos + word_len >
			    out_len_words) {
				return SBERR_InvalidArgument;
			}
			memcpy(&out[sw_out_start + sw_tag_write_pos], p_tag,
			       word_len * sizeof(uint32_t));
			sw_tag_write_pos += word_len;
		}
		tag_pos += word_len;
	}

	*out_written_words = sw_out_start + sw_enforced_tag_size_words;
	return SB_OK;
}

/* Derive key encryption key and tag key. */
enum dcrypto_result cryptokey_derive_wrapping(
	keymint_t *km, void *tag_data_ptr, size_t tag_size_bytes,
	const uint8_t *application_id_data, size_t application_id_len,
	const uint8_t *application_data_data, size_t application_data_len,
	void *hmac_key, size_t hmac_key_len, void *aes_key, size_t aes_key_len)
{
	struct drbg_ctx drbg;
	enum dcrypto_result result = DCRYPTO_FAIL;

	hmac_drbg_init(&drbg, km->drbg_seed_key, sizeof(km->drbg_seed_key),
		       tag_data_ptr, tag_size_bytes, application_id_data,
		       application_id_len, 4);

	/* Mix in another ingredient */
	result = hmac_drbg_generate(&drbg, hmac_key, hmac_key_len,
				    application_data_data,
				    application_data_len);
	if (result != DCRYPTO_OK)
		return result;
	result = hmac_drbg_generate(&drbg, aes_key, aes_key_len, NULL, 0);
	drbg_exit(&drbg);
	return result;
}

enum dcrypto_result cryptokey_generate(keymint_t *km, keymint_algorithm_t alg,
				       void *key, size_t key_len)
{
	enum dcrypto_result result;

	(void)km;
	do {
		if (!(fips_rand_bytes(key, key_len)))
			return DCRYPTO_FAIL;
		result = DCRYPTO_OK;
		if (alg == ALGORITHM_EC) {
			p256_int d, pk_x, pk_y;

			if (key_len != sizeof(d))
				return DCRYPTO_FAIL;
			result = DCRYPTO_p256_key_from_bytes(&pk_x, &pk_y, &d,
							     key);
			if (result != DCRYPTO_RETRY)
				break;
			memcpy(key, &d, sizeof(d));
		}
	} while (result != DCRYPTO_OK);
	return result;
}

enum dcrypto_result cryptokey_export_bound(
	keymint_t *km, keymint_algorithm_t alg, uint32_t *tags_start,
	size_t tag_words, const uint8_t *application_id_data,
	size_t application_id_len, const uint8_t *application_data_data,
	size_t application_data_len, size_t *blob_size_words, uint32_t *out_buf)
{
	enum dcrypto_result result = DCRYPTO_FAIL;
	const struct sha256_digest *digest;
	uint32_t aes_key[8], hmac_key[8], key[8];
	uint32_t *blob_size = out_buf;
	uint8_t *iv;
	struct hmac_sha256_ctx sha;

	if (*blob_size_words < (1 + 4 + 8 + 8))
		return DCRYPTO_FAIL;

	/* Reserve 1 word for size field (in 32-bit words) */
	/* Format of the blob:
	 * size 1x32 in bytes | iv 4x32 | encrypted key 8x32 | tag 8x32
	 */
	out_buf += 1;
	iv = (uint8_t *)out_buf;
	result = cryptokey_derive_wrapping(
		km, tags_start, tag_words * sizeof(uint32_t),
		application_id_data, application_id_len, application_data_data,
		application_data_len, aes_key, sizeof(aes_key), hmac_key,
		sizeof(hmac_key));
	if (result != DCRYPTO_OK)
		goto clean;

	result = DCRYPTO_FAIL;
	/* Create random IV for AES-CTR */
	if (!fips_rand_bytes(iv, 16))
		goto clean;
	out_buf += 4;

	/* So far hardcoded to 256-bit key */
	result = cryptokey_generate(km, alg, key, sizeof(key));
	if (result != DCRYPTO_OK)
		goto clean;

	/* Encrypt key */
	result = DCRYPTO_aes_ctr((uint8_t *)out_buf, (uint8_t *)aes_key, 256,
				 iv, (uint8_t *)key, sizeof(key));
	if (result != DCRYPTO_OK)
		goto clean;
	out_buf += sizeof(key) / sizeof(uint32_t);

	/* Add an HMAC tag for integrity */
	result = DCRYPTO_hw_hmac_sha256_init(&sha, hmac_key, sizeof(hmac_key));
	if (result != DCRYPTO_OK)
		goto clean;
	HMAC_SHA256_update(&sha, key, sizeof(key));
	digest = HMAC_SHA256_final(&sha);

	memcpy(out_buf, digest->b8, SHA256_DIGEST_SIZE);
	out_buf += SHA256_DIGEST_WORDS;
	*blob_size = (uint8_t *)out_buf - (uint8_t *)iv;
	/* Return total size of the blob including length field*/
	*blob_size_words = out_buf - blob_size;
	result = DCRYPTO_OK;
clean:
	always_memset(aes_key, 0xAA, sizeof(aes_key));
	always_memset(hmac_key, 0xAA, sizeof(hmac_key));
	return result;
}

enum dcrypto_result cryptokey_import_bound(
	keymint_t *km, uint32_t *tags_start, size_t tag_words,
	const uint8_t *application_id_data, size_t application_id_len,
	const uint8_t *application_data_data, size_t application_data_len,
	uint32_t *blob, size_t blob_size_words, uint32_t *key)
{
	enum dcrypto_result result = DCRYPTO_FAIL;
	const struct sha256_digest *digest;
	uint32_t aes_key[8], hmac_key[8];
	uint32_t blob_size = blob[0];
	uint8_t *iv;
	struct hmac_sha256_ctx sha;
	size_t key_size;

	if (blob_size < 13)
		return DCRYPTO_FAIL;

	CPRINTS("tags(%u)=%ph", tag_words, HEX_BUF(tags_start, tag_words * 4));
	CPRINTS("blob(%u)=%ph", blob_size_words,
		HEX_BUF(blob, blob_size_words * 4));

	key_size = (blob_size_words - 1 - 4 - 8) * sizeof(uint32_t);
	/* Reserve 1 word for size field (in 32-bit words) */
	/* Format of the blob:
	 * size 1x32 in bytes | iv 4x32 | encrypted key 8x32 | tag 8x32
	 */
	blob += 1;
	iv = (uint8_t *)blob;

	result = cryptokey_derive_wrapping(
		km, tags_start, tag_words * sizeof(uint32_t),
		application_id_data, application_id_len, application_data_data,
		application_data_len, aes_key, sizeof(aes_key), hmac_key,
		sizeof(hmac_key));
	if (result != DCRYPTO_OK)
		goto clean;

	result = DCRYPTO_FAIL;
	blob += 4;

	/* Decrypt key */
	result = DCRYPTO_aes_ctr((uint8_t *)key, (uint8_t *)aes_key, 256, iv,
				 (uint8_t *)blob, key_size);

	if (result != DCRYPTO_OK)
		goto clean;
	blob += key_size / sizeof(uint32_t);

	/* Add an HMAC tag for integrity */
	result = DCRYPTO_hw_hmac_sha256_init(&sha, hmac_key, sizeof(hmac_key));
	if (result != DCRYPTO_OK)
		goto clean;
	HMAC_SHA256_update(&sha, key, key_size);
	digest = HMAC_SHA256_final(&sha);

	if (memcmp(digest->b8, blob, SHA256_DIGEST_SIZE) != 0) {
		result = DCRYPTO_FAIL;
	} else
		result = DCRYPTO_OK;
clean:
	always_memset(aes_key, 0xAA, sizeof(aes_key));
	always_memset(hmac_key, 0xAA, sizeof(hmac_key));
	return result;
}

enum strongbox_error generate_key(keymint_t *km, uint32_t *buf,
				  size_t buf_size_words, size_t req_len_words,
				  size_t *out_len_bytes)
{
	key_parameters_t params = { 0 };
	uint32_t out_buf[512];
	size_t blob_start_words = 0;
	size_t blob_size_words;
	size_t total_words;
	enum dcrypto_result result;

	enum strongbox_error err;

	params.attrs.algorithm = -1;
	err = process_gen_import_tags(km, &params, buf, req_len_words, out_buf,
				      512, &blob_start_words);
	if (err != SB_OK)
		return err;

	if (blob_start_words >= 512)
		return SBERR_UnknownError;

	out_buf[3] = KEY_ORIGIN_GENERATED;

	blob_size_words = ARRAY_SIZE(out_buf) - blob_start_words;

	if (params.attrs.algorithm == ALGORITHM_RSA) {
		if (params.attrs.key_size != 1024 &&
		    params.attrs.key_size != 2048)
			return SBERR_UnsupportedKeySize;
		if (params.attrs.rsa_exponent != 3 &&
		    params.attrs.rsa_exponent != 65537)
			return SBERR_InvalidArgument;
		return SBERR_Unimplemented;
	} else {
		uint32_t key[8];
		// Mocked TI50 Symmetric/EC key generation
		if (params.attrs.algorithm == ALGORITHM_AES) {
			if ((params.attrs.key_size != 128 &&
			     params.attrs.key_size != 192 &&
			     params.attrs.key_size != 256)) {
				return SBERR_UnsupportedKeySize;
			}
		}
		if (params.attrs.algorithm != ALGORITHM_EC)
			return SBERR_Unimplemented;

		result = cryptokey_export_bound(
			km, params.attrs.algorithm, out_buf, blob_start_words,
			params.application_id, params.application_id_len,
			params.application_data, params.application_data_len,
			&blob_size_words, &out_buf[blob_start_words]);
		if (result != DCRYPTO_OK)
			return SBERR_UnknownError;

		/* This is for testing only...*/
		result = cryptokey_import_bound(
			km, out_buf, blob_start_words, params.application_id,
			params.application_id_len, params.application_data,
			params.application_data_len, &out_buf[blob_start_words],
			blob_size_words, key);
		if (result != DCRYPTO_OK)
			return SBERR_UnknownError;
	}

	total_words = blob_start_words + blob_size_words;

	/* Not enough space in original buffer */
	if (total_words > buf_size_words)
		return SBERR_UnknownError;

	memcpy(buf, out_buf, total_words * sizeof(uint32_t));
	*out_len_bytes = total_words * sizeof(uint32_t);

	return SB_OK;
}

enum strongbox_error sb_GenerateKey(struct vendor_cmd_params *p)
{
	CPRINTS("sb_GenerateKey %u, %u, %ph", p->out_size, p->in_size,
		HEX_BUF(p->buffer, p->in_size));

	return generate_key(&km, p->buffer, p->out_size / sizeof(uint32_t),
			    p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceGenerateKey, sb_GenerateKey);

/* Parse key blob to get its size in words */
static size_t blob_size(uint32_t *blob, size_t blob_size_words,
			size_t *tag_words)
{
	uint32_t words;
	if (blob[0] != SECURITY_LEVEL_STRONGBOX)
		return -1;
	words = blob[1] + 2;
	if (words >= blob_size_words)
		return -1;
	if (blob[words] != SECURITY_LEVEL_KEYSTORE)
		return -1;
	if (blob[words + 1] >= blob_size_words)
		return -1;
	words = words + blob[words + 1] + 2;
	if (blob[words] / sizeof(uint32_t) > blob_size_words)
		return -1;
	if (tag_words)
		*tag_words = words;
	words = words + (blob[words] + 3) / sizeof(uint32_t) + 1;

	return words;
}

/**
 * This is the challenge used to verify authorization of an operation.
 * See IKeyMintOperation.aidl entrypoints updateAad() and update().
 */
// long challenge;
/**
 * begin() uses this field to return additional data from the operation
 * initialization, notably to return the IV or nonce from operations
 * that generate an IV or nonce.
 */
// KeyParameter[] params;
// IKeyMintOperation operation;
/*
    BeginResult begin(in KeyPurpose purpose, in byte[] keyBlob, in
   KeyParameter[] params, in @nullable HardwareAuthToken authToken);
*/
static enum strongbox_error begin_operation(keymint_t *km, uint32_t *buf,
					    size_t buf_size_words,
					    size_t req_len_words,
					    size_t *out_len_bytes)
{
	uint32_t purpose, blob_words, params_words, tag_pos, hw_tag_words,
		tag_words;
	key_parameters_t params = { 0 };
	enum dcrypto_result result;
	uint32_t slot;
	bool unique = true;
	uint32_t operation_id;

	if (req_len_words < 13)
		return SBERR_InvalidArgument;
	purpose = buf[0];
	blob_words = blob_size(buf + 1, req_len_words - 1, &tag_words);
	if (blob_words == -1)
		return SBERR_InvalidArgument;

	params_words = buf[blob_words + 1];
	tag_pos = 3;
	hw_tag_words = buf[2];

	CPRINTS("begin %08x, %u, %u, %u, %u, %u", purpose, req_len_words,
		blob_words, hw_tag_words, params_words, tag_words);
	tag_pos = 3;

	while (tag_pos < hw_tag_words + 4) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);
		enum strongbox_error err;

		err = process_tag(tag, &params, p_tag);
		if (err != SB_OK)
			return err;

		tag_pos += word_len;
	}
	tag_pos = blob_words + 2;

	/* Process additional parameters to get application id and data */
	while (tag_pos < blob_words + params_words + 2) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);
		enum strongbox_error err;

		err = process_tag(tag, &params, p_tag);
		if (err != SB_OK)
			return err;

		tag_pos += word_len;
	}

	CPRINTS("tag-pos = %u, purpose=%08x, adata=%ph, aid=%ph", tag_pos,
		params.attrs.purpose_flags,
		HEX_BUF(params.application_data, params.application_data_len),
		HEX_BUF(params.application_id, params.application_id_len));

	/* TODO: EC keys can be Sign or Agree */
	if ((params.attrs.purpose_flags & (1 << purpose)) == 0)
		return SBERR_IncompatiblePurpose;

	if (params.attrs.algorithm != ALGORITHM_EC)
		return SBERR_UnsupportedAlgorithm;

	if (purpose != KEY_PURPOSE_SIGN)
		return SBERR_UnsupportedPurpose;

	if (km->used_slots == ((1U << KM_MAX_OPS) - 1))
		return SBERR_KeyMaxOpsExceeded;
	slot = count_trailing_zeros(~km->used_slots);
	km->used_slots |= 1U << slot;
	km->ops[slot].attrs = params.attrs;

	result = cryptokey_import_bound(
		km, buf + 1, tag_words, params.application_id,
		params.application_id_len, params.application_data,
		params.application_data_len, buf + tag_words + 1,
		blob_words - tag_words, km->ops[slot].key);
	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	/* Generate random and unique operation id. */
	do {
		if (!fips_rand_bytes(&operation_id, sizeof(operation_id)))
			return SBERR_UnknownError;
		for (size_t i = 0; i < ARRAY_SIZE(km->ops); i++)
			if (km->ops[i].operation_id == operation_id) {
				unique = false;
				break;
			};
	} while (!unique);

	km->ops[slot].operation_id = operation_id;
	km->ops[slot].update_size = 0;
	km->ops[slot].purpose = purpose;
	/* Unused challenge */
	buf[0] = 0;
	buf[1] = 0;
	/* Size of unused KeyParameters (IV, etc) */
	buf[2] = 0;
	/* Keymint Operation ID*/
	buf[3] = operation_id;
	*out_len_bytes = 4 * sizeof(buf[0]);
	return SB_OK;
}

enum strongbox_error sb_Begin(struct vendor_cmd_params *p)
{
	CPRINTS("sb_Begin %u, %u, %ph", p->out_size, p->in_size,
		HEX_BUF(p->buffer, p->in_size));

	return begin_operation(&km, p->buffer, p->out_size / sizeof(uint32_t),
			       p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceBegin, sb_Begin);

/*
byte[] update(in byte[] input, in @nullable HardwareAuthToken authToken,
	    in @nullable TimeStampToken timeStampToken);
*/
static enum strongbox_error update_operation(keymint_t *km, uint32_t *buf,
					     size_t buf_size_words,
					     size_t req_len_words,
					     size_t *out_len_bytes)
{
	size_t op_index, update_size;
	if (req_len_words < 2)
		return SBERR_InvalidArgument;
	update_size = buf[1];
	if (update_size > (req_len_words - 2) * 4)
		return SBERR_InvalidArgument;

	for (op_index = 0; op_index < ARRAY_SIZE(km->ops); op_index++)
		if (km->ops[op_index].operation_id == buf[0])
			break;
	if (op_index >= ARRAY_SIZE(km->ops))
		return SBERR_InvalidOperationHandle;

	if (update_size + km->ops[op_index].update_size >
	    sizeof(km->ops[op_index].update_context))
		return SBERR_InvalidArgument;

	memcpy((uint8_t *)(km->ops[op_index].update_context) +
		       km->ops[op_index].update_size,
	       buf + 2, update_size);
	km->ops[op_index].update_size += update_size;
	*out_len_bytes = 0;
	return SB_OK;
}

enum strongbox_error sb_Update(struct vendor_cmd_params *p)
{
	CPRINTS("sb_Update %u, %u, %ph", p->out_size, p->in_size,
		HEX_BUF(p->buffer, p->in_size));

	return update_operation(&km, p->buffer, p->out_size / sizeof(uint32_t),
				p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_OperationUpdate, sb_Update);

static enum strongbox_error finish_operation(keymint_t *km, uint32_t *buf,
					     size_t buf_size_words,
					     size_t req_len_words,
					     size_t *out_len_bytes)
{
	size_t op_index, update_size;
	enum dcrypto_result result;
	if (req_len_words < 2)
		return SBERR_InvalidArgument;
	update_size = buf[1];
	if (update_size > (req_len_words - 2) * 4)
		return SBERR_InvalidArgument;

	for (op_index = 0; op_index < ARRAY_SIZE(km->ops); op_index++)
		if (km->ops[op_index].operation_id == buf[0])
			break;
	if (op_index >= ARRAY_SIZE(km->ops))
		return SBERR_InvalidOperationHandle;

	if (update_size + km->ops[op_index].update_size >
	    sizeof(km->ops[op_index].update_context))
		return SBERR_InvalidArgument;

	memcpy((uint8_t *)(km->ops[op_index].update_context) +
		       km->ops[op_index].update_size,
	       buf + 2, update_size);
	km->ops[op_index].update_size += update_size;

	if (km->ops[op_index].attrs.algorithm != ALGORITHM_EC)
		return SBERR_UnsupportedAlgorithm;
	*out_len_bytes = 0;

	result = DCRYPTO_p256_ecdsa_sign(
		(p256_int *)km->ops[op_index].key,
		(p256_int *)km->ops[op_index].update_context,
		(p256_int *)&buf[0], (p256_int *)&buf[8]);

	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	/* Clean up everything by operation id */
	always_memset(&km->ops[op_index].attrs, 0,
		      sizeof(km->ops[op_index]) -
			      offsetof(key_operation_t, attrs));
	km->used_slots &= ~(1u << op_index);
	*out_len_bytes = 64;
	return SB_OK;
}

enum strongbox_error sb_Finish(struct vendor_cmd_params *p)
{
	CPRINTS("sb_Finish %u, %u, %ph", p->out_size, p->in_size,
		HEX_BUF(p->buffer, p->in_size));

	return finish_operation(&km, p->buffer, p->out_size / sizeof(uint32_t),
				p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_OperationFinish, sb_Finish);
