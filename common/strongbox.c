/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "console.h"
#include "extension.h"
#include "link_defs.h"
#include "strongbox.h"
#include "dcrypto.h"
#include "internal.h"

#include "cbor_basic.h"

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
	uint64_t date_not_after;
	uint64_t date_not_before;
} key_attributes_t;

/* Corresponds to `KeyAttributes` struct. Filled during tag parsing. */
typedef struct {
	key_attributes_t attrs;
	const uint8_t *application_id;
	size_t application_id_len;
	const uint8_t *application_data;
	size_t application_data_len;
	const uint8_t *attestation_challenge;
	size_t attestation_challenge_len;
} key_parameters_t;

/* Make DIGEST::NONE to use same space as SHA256 context */
struct digest_none_ctx {
	uint32_t update_size;
	uint32_t update_context[sizeof(struct sha256_ctx) / 4 - 1];
};

typedef struct {
	uint32_t operation_id;
	key_attributes_t attrs;
	keymint_purpose_t purpose;
	uint32_t key[8];
	union {
		struct digest_none_ctx none_ctx;
		struct sha256_ctx sha256_ctx;
	};
} key_operation_t;

#define KM_MAX_OPS 4

/* Keymint context */
typedef struct {
	uint32_t os_version;
	uint32_t os_patchlevel;
	uint32_t vendor_patchlevel;
	uint32_t boot_patchlevel;
	uint32_t hmac_tag_key[8];
	uint32_t drbg_seed_key[8];
	/* Bit mask for used slots in `ops`. */
	uint32_t used_slots;
	/* Public P256 for the last created key. */
	p256_int last_pk_x, last_pk_y;
	key_operation_t ops[KM_MAX_OPS];
} keymint_t;

/* Keymint context */
static keymint_t km;

enum strongbox_error sb_GetHardwareInfo(struct vendor_cmd_params *p)
{
	/* All values are aligned to 32-bit */
	static const uint8_t r[39] = { /* version */
				       0x00, 0x00, 0x00, 0x00,
				       /* keymint_security_level_t Strongbox */
				       0x00, 0x00, 0x00, 0x02,
				       /* Keymint name, length 0x00000004 */
				       0x00, 0x00, 0x00, 0x04, 'C', 'R', '5',
				       '0',
				       /* Key mint author, data length
					* 0x00000006, actual length 8 bytes
					*/
				       0x00, 0x00, 0x00, 0x06, 'G', 'O', 'O',
				       'G', 'L', 'E', 0x00, 0x00,
				       /* timestamp_token_required = false */
				       0x00, 0x00, 0x00, 0x00
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
	 * translation.
	 */
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
}

/**
 * @brief Checks if a tag is hardware-enforced.
 */
static bool tag_is_hw_enforced(keymint_tag_t tag)
{
	/* For this translation, we assume tags that are not SW-enforced and not
	 * special input tags are HW-enforced. A more robust implementation
	 * would use a complete list.
	 */
	if (tag_is_sw_enforced(tag))
		return false;

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
	case KM_TAG_CERTIFICATE_NOT_AFTER:
		if (params->attrs.date_not_after != 0)
			return SBERR_InvalidTag;
		params->attrs.date_not_after = make64(p_tag[2], p_tag[1]);
		break;
	case KM_TAG_CERTIFICATE_NOT_BEFORE:
		if (params->attrs.date_not_before != 0)
			return SBERR_InvalidTag;
		params->attrs.date_not_before = make64(p_tag[2], p_tag[1]);
		break;
	default:
		break;
	}
	return SB_OK;
}

/**
 * @brief Parse Key parameters tags and produce KeyCharacteristics.
 *
 * This function parses a list of Keymint tags from an input buffer. It
 * validates the tags, extracts key attributes, and separates the tags into
 * hardware-enforced and software-enforced characteristics. The resulting
 * KeyCharacteristics are serialized into the output buffer.
 *
 * @param km The keymint context, containing device state like OS version.
 * @param params Output parameter. A struct to be filled with the parsed key
 *               attributes.
 * @param origin The origin of the key (e.g., generated, imported).
 * @param buf Input buffer containing the key parameters as a series of tags.
 * @param req_len_words The length of the input buffer in words.
 * @param out Output buffer where the serialized KeyCharacteristics will be
 *            written.
 * @param out_len_words The size of the output buffer in words.
 * @param out_written_words Output parameter. The number of words written to the
 *                          output buffer.
 * Input Buffer (buf) Format: [size in 32-bit words] [tags]
 *
 * The input buffer buf is expected to contain a sequence of Keymint tags. The
 * first word buf[0] indicates the total length of the tag list in words.
 * The function iterates through the tags in buf until it has processed
 * tags_len_words. The output buffer out stores the serialized
 * KeyCharacteristics, separated into hardware-enforced and software-enforced
 * sections. The format is as follows:
 *
 * Hardware-Enforced Section:
 * out[0]: Security Level (always SECURITY_LEVEL_STRONGBOX)
 * out[1]: Length of the hardware-enforced section in words (excluding out[0]
 * and out[1]) Subsequent words: Keymint tags that are hardware-enforced. These
 * tags have the same format as in the input buffer (tag word followed by data
 * payload).
 * Software-Enforced Section (starts immediately after the hardware-enforced
 * section):
 * out[x]: Security Level (always SECURITY_LEVEL_KEYSTORE)
 * out[x + 1]: Length of the software-enforced section in words (excluding
 * out[x] and out[x + 1])
 * Subsequent words: Keymint tags that are software-enforced, using the
 * same format as above.
 * @return SB_OK on success, or an error code on failure.
 */
static enum strongbox_error process_gen_import_tags(
	keymint_t *km, key_parameters_t *params, keymint_key_origin_t origin,
	const uint32_t *buf, size_t req_len_words, uint32_t *out,
	size_t out_len_words, size_t *out_written_words)
{
	size_t tags_len_words;
	size_t hw_tag_start;
	size_t sw_enforced_tag_size_words;
	size_t tag_pos;
	size_t sw_out_start;
	size_t sw_tag_write_pos;

	if (req_len_words < 1 || out_len_words < 14)
		return SBERR_InvalidTag;

	memset(params, 0, sizeof(*params));

	tags_len_words = buf[0];
	if (tags_len_words > req_len_words - 1)
		return SBERR_InvalidTag;

	/* These tags go into TEE/Strongbox KeyCharacteristics */
	out[0] = (uint32_t)SECURITY_LEVEL_STRONGBOX;
	/* out[1] is length, filled later */
	out[2] = (uint32_t)KM_TAG_ORIGIN;
	out[3] = origin;
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
	tag_pos = 1; /* start after length word */

	/* Pass 1: Parse tags, copy HW tags, count SW tags, extract attributes
	 */
	while (tag_pos < tags_len_words + 1) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);
		enum strongbox_error err;

		if (tag_is_gen_disallowed(tag))
			return SBERR_InvalidTag;

		err = process_tag(tag, params, p_tag);
		if (err != SB_OK)
			return err;

		if (tag_is_hw_enforced(tag)) {
			if (hw_tag_start + word_len > out_len_words) {
				/* Not enough space */
				return SBERR_InvalidArgument;
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

	/* Set length of HW-enforced tags */
	out[1] = (hw_tag_start - 2);

	if (out_len_words < hw_tag_start + 2)
		return SBERR_InvalidArgument;

	/* Mark start of SW-enforced tags */
	out[hw_tag_start] = SECURITY_LEVEL_KEYSTORE;
	out[hw_tag_start + 1] = sw_enforced_tag_size_words;

	sw_out_start = hw_tag_start + 2;
	if (out_len_words < sw_out_start + sw_enforced_tag_size_words)
		return SBERR_InvalidArgument;

	/* Pass 2: Copy SW enforced tags */
	sw_tag_write_pos = 0;
	tag_pos = 1;
	while (tag_pos < tags_len_words + 1) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);

		if (tag_is_sw_enforced(tag)) {
			if (sw_out_start + sw_tag_write_pos + word_len >
			    out_len_words)
				return SBERR_InvalidArgument;

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
	keymint_t *km, const void *tag_data_ptr, size_t tag_size_bytes,
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
		/* Generated keys are randomly created */
		if (!(fips_rand_bytes(key, key_len)))
			return DCRYPTO_FAIL;
		result = DCRYPTO_OK;
		/* ECC P256 require test of the key candidate to be in range */
		if (alg == ALGORITHM_EC) {
			p256_int d;

			if (key_len != sizeof(d))
				return DCRYPTO_FAIL;
			/* Test P256 key candidate and save its public key. */
			result = DCRYPTO_p256_key_from_bytes(
				&km->last_pk_x, &km->last_pk_y, &d, key);
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
	keymint_t *km, const uint32_t *tags_start, size_t tag_words,
	const uint8_t *application_id_data, size_t application_id_len,
	const uint8_t *application_data_data, size_t application_data_len,
	const uint32_t *blob, size_t blob_size_words, uint32_t *key)
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
		CPRINTS("blob tag doesn't match");
		result = DCRYPTO_FAIL;
	} else
		result = DCRYPTO_OK;
clean:
	always_memset(aes_key, 0xAA, sizeof(aes_key));
	always_memset(hmac_key, 0xAA, sizeof(hmac_key));
	return result;
}

static enum strongbox_error parse_params(const uint32_t *buf, size_t buf_len,
					 key_parameters_t *params)
{
	uint32_t tag_pos = 0;

	while (tag_pos < buf_len) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		keymint_tag_t tag = prefix_get_tag(tag_word);
		size_t word_len = prefix_get_word_len(tag_word);
		enum strongbox_error err;

		err = process_tag(tag, params, p_tag);
		if (err != SB_OK)
			return err;

		tag_pos += word_len;
	}
	return SB_OK;
}

/**
 * @brief Parses and decrypts a key blob.
 *
 * This function validates the structure of a provided key blob, which is
 * expected to contain serialized hardware and software-enforced key
 * characteristics, along with the encrypted key material. It extracts the
 * key parameters into the `params` struct and decrypts the key into the
 * `key` buffer.
 *
 * @param km The keymint context, used for deriving the decryption key.
 * @param buf Pointer to the buffer containing the key blob to import.
 * @param blob_words The size of the key blob buffer in 32-bit words.
 * @param param_tags Pointer to a buffer with additional parameters for the
 *                   import operation (e.g., application ID).
 * @param param_tags_words The size of the `param_tags` buffer in 32-bit words.
 * @param params Output parameter. A struct to be filled with the parsed key
 *               attributes from the blob.
 * @param key Output parameter. A buffer where the decrypted key material will
 *            be stored.
 * @return SB_OK on successful import, or a strongbox_error code on failure
 *         (e.g., SBERR_InvalidKeyBlob if the blob is malformed).
 */
static enum strongbox_error import_blob(keymint_t *km, const uint32_t *buf,
					size_t blob_words,
					const uint32_t *param_tags,
					size_t param_tags_words,
					key_parameters_t *params, uint32_t *key)
{
	uint32_t hw_tag_words, sw_tag_words, tag_words, key_blob_size;
	enum dcrypto_result result;
	enum strongbox_error err;

	if (blob_words < 12 + 8 + 4)
		return SBERR_InvalidKeyBlob;
	if (buf[0] != SECURITY_LEVEL_STRONGBOX)
		return SBERR_InvalidKeyBlob;
	hw_tag_words = buf[1];
	tag_words = hw_tag_words + 2;
	if (hw_tag_words >= blob_words)
		return SBERR_InvalidKeyBlob;

	if (buf[tag_words] != SECURITY_LEVEL_KEYSTORE)
		return SBERR_InvalidKeyBlob;
	sw_tag_words = buf[tag_words + 1];
	if (sw_tag_words + tag_words >= blob_words)
		return SBERR_InvalidKeyBlob;

	tag_words += sw_tag_words + 2;

	/* Actual encrypted key blob size */
	key_blob_size = buf[tag_words];

	if (key_blob_size / sizeof(uint32_t) + tag_words + 1 != blob_words)
		return SBERR_InvalidKeyBlob;

	/* Parse HW-enforced tags only */
	err = parse_params(buf + 2, hw_tag_words, params);
	if (err != SB_OK)
		return err;

	/* Process additional parameters to get application id and data */
	err = parse_params(param_tags, param_tags_words, params);
	if (err != SB_OK)
		return err;

	result = cryptokey_import_bound(
		km, buf, tag_words, params->application_id,
		params->application_id_len, params->application_data,
		params->application_data_len, buf + tag_words,
		blob_words - tag_words, key);
	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	return SB_OK;
}

/**
 * @brief Generates a key blob based on provided key parameters.
 *
 * This function takes a set of KeyMint tags, processes them to create
 * KeyCharacteristics, generates a new cryptographic key according to these
 * parameters, and then encrypts the key material. The final output is a
 * single "key blob" containing both the characteristics and the encrypted key,
 * which is written to the output buffer.
 *
 * @param km The keymint context, used for cryptographic operations.
 * @param tags A pointer to a buffer of KeyMint tags specifying the parameters
 *             for the key to be generated.
 * @param tag_words The length of the `tags` buffer in 32-bit words.
 * @param out_buf The output buffer where the generated key blob will be
 *                written. The blob includes a size prefix, key
 *                characteristics, and the encrypted key material.
 * @param out_words As an input, the size of `out_buf` in words. On successful
 *                  return, this is updated with the total number of words
 *                  written to `out_buf`.
 * @return SB_OK on success, or a strongbox_error code on failure (e.g.,
 *         SBERR_UnsupportedKeySize).
 */
static enum strongbox_error generate_key_blob(keymint_t *km,
					      const uint32_t *tags,
					      size_t tag_words,
					      uint32_t *out_buf,
					      size_t *out_words)
{
	key_parameters_t params = { 0 };
	uint32_t *key_blob_size = out_buf;
	size_t blob_start_words = 0;
	size_t blob_size_words;
	size_t total_words;
	size_t out_buf_words = *out_words;
	enum dcrypto_result result;
	enum strongbox_error err;

	/* Reserve 1 word for size field (in 32-bit words) */
	out_buf += 1;
	out_buf_words -= 1;

	params.attrs.algorithm = -1;
	err = process_gen_import_tags(km, &params, KEY_ORIGIN_GENERATED, tags,
				      tag_words, out_buf, out_buf_words,
				      &blob_start_words);
	if (err != SB_OK)
		return err;

	if (blob_start_words >= out_buf_words)
		return SBERR_UnknownError;

	/* Set max size for the actual key blob */
	blob_size_words = out_buf_words - blob_start_words;

	if (params.attrs.algorithm == ALGORITHM_RSA) {
		if (params.attrs.key_size != 1024 &&
		    params.attrs.key_size != 2048)
			return SBERR_UnsupportedKeySize;
		if (params.attrs.rsa_exponent != 3 &&
		    params.attrs.rsa_exponent != 65537)
			return SBERR_InvalidArgument;
		return SBERR_Unimplemented;
	} else {
		if (params.attrs.algorithm == ALGORITHM_AES) {
			if ((params.attrs.key_size != 128 &&
			     params.attrs.key_size != 192 &&
			     params.attrs.key_size != 256))
				return SBERR_UnsupportedKeySize;
		}
		if (params.attrs.algorithm != ALGORITHM_EC)
			return SBERR_Unimplemented;
		/* Need at least one of the curve_id or key size to be specified
		 */
		if (params.attrs.curve_id != EC_CURVE_P_256 &&
		    params.attrs.key_size != 256)
			return SBERR_Unimplemented;

		/* Create a random key and place it into encrypted key blob with
		 * security tag. Encrypted blob is bound to all the tags and the
		 * Application ID and Application Data which are excluded from
		 * the key characteristics, but provided later with the `begin`
		 * operation.
		 */
		result = cryptokey_export_bound(
			km, params.attrs.algorithm, out_buf, blob_start_words,
			params.application_id, params.application_id_len,
			params.application_data, params.application_data_len,
			&blob_size_words, &out_buf[blob_start_words]);
		if (result != DCRYPTO_OK)
			return SBERR_UnknownError;
	}

	/* TODO: Add support for attestation */
	total_words = blob_start_words + blob_size_words;
	/* Total size of key blob in 32-bit words */
	*key_blob_size = total_words;
	*out_words = total_words + 1;
	return SB_OK;
};

/**
 * Implement IKeyMintDevice.generateKey()
 * Format of the command:
 * size of key params in 32-bit words | [key parameters]
 *
 * param keyParams Key generation parameters are defined as KeyMintDevice
 * tag/value pairs, provided in params.
 *
 * The input buffer buf is expected to contain a sequence of Keymint tags. The
 * first word buf[0] indicates the total length of the tag list in words.
 * The output buffer out stores the serialized KeyCharacteristics, separated
 * into hardware-enforced and software-enforced sections.
 * The format is as follows:
 *
 * Hardware-Enforced Section:
 * out[0]: Security Level (always SECURITY_LEVEL_STRONGBOX)
 * out[1]: Length of the hardware-enforced section in words (excluding out[0]
 * and out[1]) Subsequent words: Keymint tags that are hardware-enforced. These
 * tags have the same format as in the input buffer (tag word followed by data
 * payload).
 * Software-Enforced Section (starts immediately after the hardware-enforced
 * section):
 * out[x]: Security Level (always SECURITY_LEVEL_KEYSTORE)
 * out[x + 1]: Length of the software-enforced section in words (excluding
 * out[x] and out[x + 1])
 * Subsequent words: Keymint tags that are software-enforced, using the
 * same format as above.
 *
 * param attestationKey, if provided, specifies the key that must be used to
 * sign the attestation certificate. KeyCreationResult generateKey( in
 * KeyParameter[] keyParams, in @nullable AttestationKey attestationKey);
 * @param km Keymint context
 * @param buf Input buffer
 * @param buf_size_words Input buffer size in 32-bit words
 * @param req_len_words Request size in words
 * @param out_len_bytes Output buffer size in bytes
 * @return enum strongbox_error
 */
static enum strongbox_error generate_key(keymint_t *km, uint32_t *buf,
					 size_t buf_size_words,
					 size_t req_len_words,
					 size_t *out_len_bytes)
{
	uint32_t out_buf[512];
	size_t total_words = ARRAY_SIZE(out_buf);
	size_t out_len_max = *out_len_bytes;

	enum strongbox_error err;

	*out_len_bytes = 0;
	err = generate_key_blob(km, buf, req_len_words, out_buf, &total_words);
	if (err != SB_OK)
		return err;

	if (total_words * sizeof(uint32_t) >= out_len_max)
		return SBERR_UnknownError;

	memcpy(buf, out_buf, total_words * sizeof(uint32_t));

	/* Placeholder for the certificate size */
	buf[total_words] = 0;
	*out_len_bytes = total_words * sizeof(uint32_t) + 1 * sizeof(uint32_t);

	return SB_OK;
}

static enum strongbox_error sb_GenerateKey(struct vendor_cmd_params *p)
{
	return generate_key(&km, p->buffer, p->out_size / sizeof(uint32_t),
			    p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceGenerateKey, sb_GenerateKey);

/**
 * Implement IKeyMintDevice::begin() operation.
 * Input format is:
 * 1. 32-bit purpose
 * 2. key blob from generate_key (includes size)
 * 3. size of additional params in 32-bit words
 * 4. additional params - same tags as for generate key
 * We ignore challenge field.
 * Returns:
 * 64-bit challenge (0) | size of KeyParameters(0) | 32-bit operation ID
 *
 *   BeginResult begin(in KeyPurpose purpose, in byte[] keyBlob, in
 *  KeyParameter[] params, in @nullable HardwareAuthToken authToken);
 */
static enum strongbox_error begin_operation(keymint_t *km, uint32_t *buf,
					    size_t buf_size_words,
					    size_t req_len_words,
					    size_t *out_len_bytes)
{
	uint32_t purpose, blob_words, params_words;
	key_parameters_t params = { 0 };
	enum strongbox_error err;
	uint32_t slot;
	bool unique = true;
	uint32_t operation_id;

	if (req_len_words < 13)
		return SBERR_InvalidArgument;

	/* Check if we have free slots for the operation. */
	if (km->used_slots == ((1U << KM_MAX_OPS) - 1))
		return SBERR_KeyMaxOpsExceeded;
	/* Find free slot */
	slot = count_trailing_zeros(~km->used_slots);

	purpose = buf[0];

	blob_words = buf[1];
	if (blob_words > req_len_words - 1)
		return SBERR_InvalidKeyBlob;

	params_words = buf[blob_words + 2];
	if (params_words > req_len_words ||
	    (params_words + blob_words + 2) > req_len_words)
		return SBERR_InvalidArgument;

	err = import_blob(km, buf + 2, blob_words, buf + blob_words + 3,
			  params_words, &params, km->ops[slot].key);
	if (err != SB_OK)
		return err;

	/* Check that requested purpose is compatible with the key. */
	if ((params.attrs.purpose_flags & (1 << purpose)) == 0)
		return SBERR_IncompatiblePurpose;

	/* We only support P256 for now */
	if (params.attrs.algorithm != ALGORITHM_EC)
		return SBERR_UnsupportedAlgorithm;

	/* TODO: EC keys can be Sign or Agree */
	if (purpose != KEY_PURPOSE_SIGN)
		return SBERR_UnsupportedPurpose;

	/* We only support None or SHA256 digests. */
	if (params.attrs.digest != KM_DIGEST_SHA_2_256 &&
	    params.attrs.digest != KM_DIGEST_NONE)
		return SBERR_UnsupportedAlgorithm;

	/* Mark slot as allocated. */
	km->used_slots |= 1U << slot;
	km->ops[slot].attrs = params.attrs;

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

	/* Initialize hash if requested. */
	km->ops[slot].none_ctx.update_size = 0;
	if (params.attrs.digest == KM_DIGEST_SHA_2_256)
		SHA256_sw_init(&km->ops[slot].sha256_ctx);

	km->ops[slot].operation_id = operation_id;
	km->ops[slot].purpose = purpose;
	/* Unused challenge */
	buf[0] = 0;
	buf[1] = 0;
	/* Size of unused KeyParameters (IV, etc) */
	buf[2] = 0;
	/* Keymint Operation ID */
	buf[3] = operation_id;
	*out_len_bytes = 4 * sizeof(buf[0]);
	return SB_OK;
}

enum strongbox_error sb_Begin(struct vendor_cmd_params *p)
{
	return begin_operation(&km, p->buffer, p->out_size / sizeof(uint32_t),
			       p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceBegin, sb_Begin);

/**
 *
 * Format of the command:
 * 32-bit operation id | update size (bytes) | [update content]
 * byte[] update(in byte[] input, in @nullable HardwareAuthToken authToken,
 *               in @nullable TimeStampToken timeStampToken);
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

	if (km->ops[op_index].attrs.digest == KM_DIGEST_SHA_2_256) {
		SHA256_sw_update(&km->ops[op_index].sha256_ctx,
				 (uint8_t *)buf + 2, update_size);
	} else if (km->ops[op_index].attrs.digest == KM_DIGEST_NONE) {
		if (update_size + km->ops[op_index].none_ctx.update_size >
		    sizeof(km->ops[op_index].none_ctx.update_context))
			return SBERR_InvalidArgument;
		memcpy((uint8_t *)(km->ops[op_index].none_ctx.update_context) +
			       km->ops[op_index].none_ctx.update_size,
		       buf + 2, update_size);
		km->ops[op_index].none_ctx.update_size += update_size;
	} else
		return SBERR_UnsupportedAlgorithm;

	*out_len_bytes = 0;
	return SB_OK;
}

enum strongbox_error sb_Update(struct vendor_cmd_params *p)
{
	return update_operation(&km, p->buffer, p->out_size / sizeof(uint32_t),
				p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_OperationUpdate, sb_Update);

/**
 * Format of the Finish command:
 * 32-bit operation id | update size (bytes) | [update content]
 * byte[] finish(in byte[] input, in @nullable HardwareAuthToken authToken);
 */
static enum strongbox_error finish_operation(keymint_t *km, uint32_t *buf,
					     size_t buf_size_words,
					     size_t req_len_words,
					     size_t *out_len_bytes)
{
	size_t op_index, update_size;
	enum dcrypto_result result;
	const uint32_t *sign_data = NULL;

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

	if (km->ops[op_index].attrs.algorithm != ALGORITHM_EC)
		return SBERR_UnsupportedAlgorithm;

	if (km->ops[op_index].attrs.digest == KM_DIGEST_SHA_2_256) {
		SHA256_sw_update(&km->ops[op_index].sha256_ctx,
				 (uint8_t *)buf + 2, update_size);
		sign_data = SHA256_sw_final(&km->ops[op_index].sha256_ctx)->b32;
	} else if (km->ops[op_index].attrs.digest == KM_DIGEST_NONE) {
		if (update_size + km->ops[op_index].none_ctx.update_size >
		    sizeof(km->ops[op_index].none_ctx.update_context))
			return SBERR_InvalidArgument;

		memcpy((uint8_t *)(km->ops[op_index].none_ctx.update_context) +
			       km->ops[op_index].none_ctx.update_size,
		       buf + 2, update_size);
		km->ops[op_index].none_ctx.update_size += update_size;
		sign_data = km->ops[op_index].none_ctx.update_context;

	} else
		return SBERR_UnsupportedAlgorithm;

	*out_len_bytes = 0;

	result = DCRYPTO_p256_ecdsa_sign((p256_int *)km->ops[op_index].key,
					 (p256_int *)sign_data,
					 (p256_int *)&buf[0],
					 (p256_int *)&buf[8]);

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
	return finish_operation(&km, p->buffer, p->out_size / sizeof(uint32_t),
				p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_OperationFinish, sb_Finish);

/**
 *
 * Definition of the command arguments from AIDL:
 * @param out MacedPublicKey macedPublicKey contains the public key of the
 * generated key pair,    MACed so that generateCertificateRequest can easily
 * verify, without the privateKeyHandle, that the contained public key is for
 * remote certification.
 *
 * @return data representing a handle to the private key. The format is
 * implementation-defined,  but note that specific services may define a
 * required format.  KeyMint does.
 *
 * byte[] generateEcdsaP256KeyPair(in boolean testMode, out MacedPublicKey
   macedPublicKey);
*/
static enum strongbox_error generate_key_pair(keymint_t *km, uint32_t *buf,
					      size_t buf_size_words,
					      size_t req_len_words,
					      size_t *out_len_bytes)
{
	static const uint32_t attest_key_params[] = {
		17,
		KM_TAG(KM_TAG_PURPOSE, 0),
		KEY_PURPOSE_ATTEST_KEY,
		KM_TAG(KM_TAG_ALGORITHM, 0),
		ALGORITHM_EC,
		KM_TAG(KM_TAG_KEY_SIZE, 0),
		256,
		KM_TAG(KM_TAG_EC_CURVE, 0),
		EC_CURVE_P_256,
		KM_TAG(KM_TAG_NO_AUTH_REQUIRED, 0),
		KM_TAG(KM_TAG_DIGEST, 0),
		KM_DIGEST_SHA_2_256,
		KM_TAG(KM_TAG_CERTIFICATE_NOT_BEFORE, 0),
		0,
		0,
		KM_TAG(KM_TAG_CERTIFICATE_NOT_AFTER, 0),
		0,
		0,
	};
	size_t out_len_max = *out_len_bytes;
	size_t total_words = out_len_max / sizeof(uint32_t);

	enum strongbox_error err;
	struct hmac_sha256_ctx sha;
	enum dcrypto_result result;
	const struct sha256_digest *digest;

	*out_len_bytes = 0;
	err = generate_key_blob(km, attest_key_params,
				ARRAY_SIZE(attest_key_params), buf,
				&total_words);
	if (err != SB_OK)
		return err;

	/* TODO: Mac'ed keys may have to be in the CBOR format.*/

	/* After the key blob add MacedPublicKey, but check we have space. */
	if (total_words > buf_size_words - 25)
		return SBERR_UnknownError;

	/* Place the length of the Mac'ed key in words. The format is:
	 * 8x32-bit words of big-endian X, 8x32-bit words of big-endian Y,
	 * 8x32-bit words of HMAC
	 */
	buf[total_words] = 8 /* X */ + 8 /*Y */ + 8 /* HMAC*/;
	p256_to_bin(&km->last_pk_x, (uint8_t *)(buf + total_words + 1));
	p256_to_bin(&km->last_pk_y, (uint8_t *)(buf + total_words + 9));

	/* Add an HMAC tag for integrity */
	result = DCRYPTO_hw_hmac_sha256_init(&sha, km->hmac_tag_key,
					     sizeof(km->hmac_tag_key));
	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	HMAC_SHA256_update(&sha, buf + total_words + 1,
			   2 * sizeof(km->last_pk_x));
	digest = HMAC_SHA256_final(&sha);

	memcpy(buf + total_words + 17, digest->b8, SHA256_DIGEST_SIZE);
	*out_len_bytes = (total_words + 25) * sizeof(uint32_t);
	return err;
}

enum strongbox_error sb_GenerateKeyPair(struct vendor_cmd_params *p)
{
	return generate_key_pair(&km, p->buffer, p->out_size / sizeof(uint32_t),
				 p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_RpcGenerateEcdsaP256KeyPair, sb_GenerateKeyPair);

/**
 * Implementation for GenerateCertificateV2Request command. Unlike Keymint's
 * implementation we only process 1 mac'ed key at a time due to in/out size
 * constraints, so Strongbox'es TA counterpart would have to split single
 * request with many mac'ed keys into a sequence of requests.
 *
 * @param km
 * @param buf
 * @param buf_size_words
 * @param req_len_words
 * @param out_len_bytes
 * @return enum strongbox_error
 *
 * generateCertificateRequestV2 creates a certificate signing request to be sent
 * to the provisioning server.
 *
 * @param in MacedPublicKey[] keysToSign contains the set of keys to certify.
 * The IRemotelyProvisionedComponent must validate the MACs on each key.  If any
 * entry in the array lacks a valid MAC, the method must return
 * STATUS_INVALID_MAC.  This method must not accept test keys. If any entry in
 * the array is a test key, the method must return
 *        STATUS_TEST_KEY_IN_PRODUCTION_REQUEST.
 *
 * @param in challenge contains a byte string from the provisioning server which
 * will be included in the signed data of the CSR structure. Different
 * provisioned backends may use different semantic data for this field, but the
 * supported sizes must be between 0 and 64 bytes, inclusive.
 *
 * @return a CBOR Certificate Signing Request (Csr) serialized into a byte
 * array.
 *
 *         See generateCertificateRequestV2.cddl for CDDL definitions.
 *
 * byte[] generateCertificateRequestV2(in MacedPublicKey[] keysToSign, in byte[]
 * challenge);
 */
static enum strongbox_error generate_cert_req(keymint_t *km, uint32_t *buf,
					      size_t buf_size_words,
					      size_t req_len_words,
					      size_t *out_len_bytes)
{
	size_t maced_len;
	size_t challenge_len;
	enum dcrypto_result result;
	const struct sha256_digest *digest;
	struct hmac_sha256_ctx sha;
	uint32_t public_key[16];
	uint32_t challenge[16];
	uint8_t *b8;

	if (req_len_words < 25)
		return SBERR_InvalidArgument;

	*out_len_bytes = 0;
	maced_len = buf[0];
	if (maced_len != 24)
		return SBERR_InvalidArgument;

	/* Add an HMAC tag for integrity */
	result = DCRYPTO_hw_hmac_sha256_init(&sha, km->hmac_tag_key,
					     sizeof(km->hmac_tag_key));
	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	HMAC_SHA256_update(&sha, buf + 1, 2 * sizeof(km->last_pk_x));
	digest = HMAC_SHA256_final(&sha);
	if (memcmp(digest->b8, buf + 17, SHA256_DIGEST_SIZE) != 0)
		return SBERR_RKP_STATUS_INVALID_MAC;

	challenge_len = buf[25];
	if (challenge_len > 64 ||
	    (challenge_len / sizeof(uint32_t) + 26 > req_len_words))
		return SBERR_InvalidArgument;

	memcpy(challenge, buf + 26, challenge_len);

	/* Copy public key locally */
	memcpy(public_key, buf + 1, sizeof(public_key));

	b8 = (uint8_t *)buf;
	/**
	 * AuthenticatedRequest<T> = [
	 *    version: 1,  ; The AuthenticatedRequest CDDL Schema version.
	 *    UdsCerts,
	 *    DiceCertChain,
	 *    SignedData<[
	 *       challenge: bstr .size (0..64),
	 *       bstr .cbor T,
	 *    ]>,
	 * ];
	 */

	/* AuthenticatedRequest<CsrPayload>, 4 elements array */
	b8[0] = CBOR_HDR1(CBOR_MAJOR_ARR, 4);
	/* version: 1,; The AuthenticatedRequest CDDL Schema version.*/
	b8[1] = CBOR_UINT0(1);
	/* UdsCerts */

	return SB_OK;
}

enum strongbox_error sb_GenerateCertificateReq(struct vendor_cmd_params *p)
{
	return generate_cert_req(&km, p->buffer, p->out_size / sizeof(uint32_t),
				 p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_RpcGenerateCertificateV2Request,
			  sb_GenerateCertificateReq);
