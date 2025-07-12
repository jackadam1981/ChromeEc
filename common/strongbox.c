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
#include "cbor_boot_param.h"
#include "boot_param.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ##args)

struct km_key_attr {
	uint32_t purpose_flags;
	enum km_algorithm algorithm;
	uint32_t key_size;
	enum km_ec_curve curve_id;
	enum km_digest digest;
	bool caller_nonce;
	uint64_t rsa_exponent;
	uint64_t date_not_after;
	uint64_t date_not_before;
};

/* Corresponds to `KeyAttributes` struct. Filled during tag parsing. */
struct km_key_params {
	struct km_key_attr attrs;
	const uint8_t *application_id;
	size_t application_id_len;
	const uint8_t *application_data;
	size_t application_data_len;
	const uint8_t *attestation_challenge;
	size_t attestation_challenge_len;
};

/* Make DIGEST::NONE to use same space as SHA256 context */
struct digest_none_ctx {
	uint32_t update_size;
	uint32_t update_context[sizeof(struct sha256_ctx) / 4 - 1];
};

struct km_operation {
	uint32_t operation_id;
	struct km_key_attr attrs;
	enum km_purpose purpose;
	uint32_t key[8];
	union {
		struct digest_none_ctx none_ctx;
		struct sha256_ctx sha256_ctx;
	};
};

#define KM_MAX_OPS 4

/* Keymint context */
struct km {
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
	struct km_operation ops[KM_MAX_OPS];
};

/* Keymint context */
static struct km km;

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
		if (cmd_p->command_code == p->code) {
			/* Check that input size is aligned, which is convention
			 * for all SB commands.
			 */
			if (p->in_size & (sizeof(uint32_t) - 1))
				return SBERR_InvalidArgument;

			return cmd_p->handler(&km, p->buffer,
					      p->out_size / sizeof(uint32_t),
					      p->in_size / sizeof(uint32_t),
					      &p->out_size);
		}
		cmd_p++;
	}

	/* Command not found or not allowed */
	p->out_size = 0;
	return SBERR_Unimplemented;
}

/**
 * @brief Implements IKeyMintDevice::getHardwareInfo.
 *
 * Provides information about the KeyMint hardware.
 *
 * Input Buffer (`p->buffer`, `p->in_size`):
 * - Should be empty. `p->in_size` must be 0.
 *
 * Output Buffer (`p->buffer`, `p->out_size`):
 * A 39-byte structure containing hardware details. The caller must provide an
 * output buffer of at least 39 bytes.
 * - [ 4 bytes ] uint32_t version
 * - [ 4 bytes ] enum km_security_level (Strongbox)
 * - [ 4 bytes ] uint32_t keymint_name_len (value: 4)
 * - [ 4 bytes ] char[] keymint_name ("CR50")
 * - [ 4 bytes ] uint32_t keymint_author_len (value: 6)
 * - [ 8 bytes ] char[] keymint_author ("GOOGLE\0\0")
 * - [ 4 bytes ] uint32_t timestamp_token_required (false)
 *
 * @param p Vendor command parameters.
 * @return SB_OK on success, or an error code on failure.
 */
enum strongbox_error sb_GetHardwareInfo(struct km *km, uint32_t *buf,
					size_t buf_size_words,
					size_t req_len_words,
					size_t *out_len_bytes)
{
	/* All values are aligned to 32-bit */
	static const uint8_t r[39] = { /* version */
				       0x00, 0x00, 0x00, 0x00,
				       /* enum km_security_level Strongbox */
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
	(void)km;

	/* Clean output len in case of errors. */
	*out_len_bytes = 0;
	if (buf_size_words < sizeof(r) / sizeof(uint32_t))
		return SBERR_InvalidArgument;
	/* No arguments are expected for the command. */
	if (req_len_words)
		return SBERR_InvalidArgument;

	*out_len_bytes = sizeof(r);
	memcpy(buf, &r, sizeof(r));
	return SB_OK;
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceGetHardwareInfo, sb_GetHardwareInfo);

/**
 * @brief Checks if a tag is disallowed during key generation/import.
 */
static bool tag_is_gen_disallowed(enum km_tag tag)
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
static bool tag_is_sw_enforced(enum km_tag tag)
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
static bool tag_is_hw_enforced(enum km_tag tag)
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

static enum strongbox_error process_tag(enum km_tag tag,
					struct km_key_params *params,
					const uint32_t *p_tag)
{
	uint32_t tag_word = p_tag[0];

	switch (tag) {
	case KM_TAG_ROLLBACK_RESISTANCE:
		return SBERR_RollbackResistanceUnavailable;
	case KM_TAG_ALGORITHM:
		if (params->attrs.algorithm != 0)
			return SBERR_InvalidTag;
		params->attrs.algorithm = (enum km_algorithm)p_tag[1];
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
 * first word buf[0] indicates the total length of the tag list in 32-bit words.
 * Each tag is a 32-bit words with following data. Data is always aligned to
 * 32-bits. For BYTES/BIGNUM tags, the length of data is encoded in the middle
 * 12 bits, see `prefix_get_word_len()`.
 *
 * The function iterates through the tags in buf until it has processed
 * tags_len_words. The output buffer out stores the serialized
 * KeyCharacteristics, separated into hardware-enforced and software-enforced
 * sections. The format is as follows:
 *
 * Hardware-Enforced Section:
 * out[0]: Security Level (always KM_SECURITY_STRONGBOX)
 * out[1]: Length of the hardware-enforced section in words (excluding out[0]
 * and out[1]) Subsequent words: Keymint tags that are hardware-enforced. These
 * tags have the same format as in the input buffer (tag word followed by data
 * payload).
 * Software-Enforced Section (starts immediately after the hardware-enforced
 * section):
 * out[x]: Security Level (always KM_SECURITY_KEYSTORE)
 * out[x + 1]: Length of the software-enforced section in words (excluding
 * out[x] and out[x + 1])
 * Subsequent words: Keymint tags that are software-enforced, using the
 * same format as above.
 * @return SB_OK on success, or an error code on failure.
 */
static enum strongbox_error process_gen_import_tags(
	struct km *km, struct km_key_params *params, enum km_origin origin,
	const uint32_t *buf, size_t req_len_words, uint32_t *out,
	size_t out_len_words, size_t *out_written_words)
{
	size_t tags_len_words;
	size_t hw_tag_start;
	size_t sw_enforced_tag_size_words;
	size_t tag_pos;
	size_t sw_out_start;
	size_t sw_tag_write_pos;

	/* 14 words is the minimal size of the result. */
	if (req_len_words < 1 || out_len_words < 14)
		return SBERR_InvalidTag;

	memset(params, 0, sizeof(*params));

	tags_len_words = buf[0];
	if (tags_len_words > req_len_words - 1)
		return SBERR_InvalidTag;

	/* These tags go into TEE/Strongbox KeyCharacteristics */
	out[0] = (uint32_t)KM_SECURITY_STRONGBOX;
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
		enum km_tag tag = prefix_get_tag(tag_word);
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

	if ((params->attrs.purpose_flags & (1 << KM_PURPOSE_ATTEST_KEY)) != 0 &&
	    (params->attrs.purpose_flags != (1 << KM_PURPOSE_ATTEST_KEY))) {
		return SBERR_IncompatiblePurpose;
	}

	if (params->attrs.algorithm == 0)
		return SBERR_InvalidArgument;

	/* Set length of HW-enforced tags */
	out[1] = (hw_tag_start - 2);

	if (out_len_words < hw_tag_start + 2)
		return SBERR_InvalidArgument;

	/* Mark start of SW-enforced tags */
	out[hw_tag_start] = KM_SECURITY_KEYSTORE;
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
		enum km_tag tag = prefix_get_tag(tag_word);
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
	struct km *km, const void *tag_data_ptr, size_t tag_size_bytes,
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

enum dcrypto_result cryptokey_generate(struct km *km, enum km_algorithm alg,
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
		if (alg == KM_ALG_EC) {
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
	struct km *km, enum km_algorithm alg, uint32_t *tags_start,
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
	struct km *km, const uint32_t *tags_start, size_t tag_words,
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
					 struct km_key_params *params)
{
	uint32_t tag_pos = 0;

	while (tag_pos < buf_len) {
		const uint32_t *p_tag = &buf[tag_pos];
		uint32_t tag_word = *p_tag;
		enum km_tag tag = prefix_get_tag(tag_word);
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
static enum strongbox_error import_blob(struct km *km, const uint32_t *buf,
					size_t blob_words,
					const uint32_t *param_tags,
					size_t param_tags_words,
					struct km_key_params *params,
					uint32_t *key)
{
	uint32_t hw_tag_words, sw_tag_words, tag_words, key_blob_size;
	enum dcrypto_result result;
	enum strongbox_error err;

	if (blob_words < 12 + 8 + 4)
		return SBERR_InvalidKeyBlob;
	if (buf[0] != KM_SECURITY_STRONGBOX)
		return SBERR_InvalidKeyBlob;
	hw_tag_words = buf[1];
	tag_words = hw_tag_words + 2;
	if (hw_tag_words >= blob_words)
		return SBERR_InvalidKeyBlob;

	if (buf[tag_words] != KM_SECURITY_KEYSTORE)
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
static enum strongbox_error generate_key_blob(struct km *km,
					      const uint32_t *tags,
					      size_t tag_words,
					      uint32_t *out_buf,
					      size_t *out_words)
{
	struct km_key_params params = { 0 };
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
	err = process_gen_import_tags(km, &params, KM_ORIGIN_GENERATED, tags,
				      tag_words, out_buf, out_buf_words,
				      &blob_start_words);
	if (err != SB_OK)
		return err;

	if (blob_start_words >= out_buf_words)
		return SBERR_UnknownError;

	/* Set max size for the actual key blob */
	blob_size_words = out_buf_words - blob_start_words;

	if (params.attrs.algorithm == KM_ALG_RSA) {
		if (params.attrs.key_size != 1024 &&
		    params.attrs.key_size != 2048)
			return SBERR_UnsupportedKeySize;
		if (params.attrs.rsa_exponent != 3 &&
		    params.attrs.rsa_exponent != 65537)
			return SBERR_InvalidArgument;
		return SBERR_Unimplemented;
	}

	if (params.attrs.algorithm == KM_ALG_AES) {
		if ((params.attrs.key_size != 128 &&
		     params.attrs.key_size != 192 &&
		     params.attrs.key_size != 256))
			return SBERR_UnsupportedKeySize;
	}
	if (params.attrs.algorithm != KM_ALG_EC)
		return SBERR_Unimplemented;
	/* Need at least one of the curve_id or key size to be specified
	 */
	if (params.attrs.curve_id != KM_EC_CURVE_P_256 &&
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

	/* TODO: Add support for attestation */
	total_words = blob_start_words + blob_size_words;
	/* Total size of key blob in 32-bit words */
	*key_blob_size = total_words;
	*out_words = total_words + 1;
	return SB_OK;
};

/**
 * @brief Implements the core logic for IKeyMintDevice.generateKey().
 *
 * Parses key generation parameters, creates a hardware-backed key, and returns
 * it as an encrypted, versioned, and integrity-protected key blob.
 *
 * Input Buffer (`buf`):
 * A serialized list of KeyMint tags.
 * - [ 4 bytes ] uint32_t tags_len_words: The size of the tag list that
 * follows, in 32-bit words.
 * - [ n bytes ] Serialized KeyMint tags. Each tag consists of a 32-bit tag
 * word followed by an optional, 32-bit aligned payload.
 *
 * Output Buffer (`buf`):
 * A KeyCreationResult structure containing the key blob and an empty
 * certificate chain.
 * - [ 4 bytes ] uint32_t key_blob_total_words: Total size of the key blob
 * and characteristics that follow, in 32-bit words.
 * - [ n bytes ] Key blob, containing HW and SW enforced KeyCharacteristics.
 * Hardware-Enforced Section:
 * out[0]: Security Level (always KM_SECURITY_STRONGBOX)
 * out[1]: Length of the hardware-enforced section in words (excluding out[0]
 * and out[1]) Subsequent words: Keymint tags that are hardware-enforced. These
 * tags have the same format as in the input buffer (tag word followed by data
 * payload).
 * Software-Enforced Section (starts immediately after the hardware-enforced
 * section):
 * out[x]: Security Level (always KM_SECURITY_KEYSTORE)
 * out[x + 1]: Length of the software-enforced section in words (excluding
 * out[x] and out[x + 1])
 * Subsequent words: Keymint tags that are software-enforced, using the
 * same format as above.
 * - [ m bytes ] Encrypted key material, prefixed with its size in bytes.
 * - [ 4 bytes ] uint32_t cert_chain_len_bytes: The size of the certificate
 * chain in bytes (currently always 0).
 *
 * @param km KeyMint context.
 * @param buf Input/Output buffer.
 * @param buf_size_words Size of the I/O buffer in 32-bit words.
 * @param req_len_words Size of the input data in 32-bit words.
 * @param out_len_bytes On success, the number of bytes written to the buffer.
 * @return SB_OK on success, or an error code on failure.
 */
static enum strongbox_error sb_GenerateKey(struct km *km, uint32_t *buf,
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

DECLARE_STRONGBOX_COMMAND(SB_DeviceGenerateKey, sb_GenerateKey);

/**
 * @brief Implements the core logic for IKeyMintDevice::begin().
 *
 * Starts a cryptographic operation (e.g., signing) with the provided key.
 *
 * Input Buffer (`buf`):
 * - [ 4 bytes ] enum km_purpose: The purpose of the operation (e.g., SIGN).
 * - [ 4 bytes ] uint32_t key_blob_words: The size of the key blob that
 * follows, in 32-bit words.
 * - [ n bytes ] The key blob generated by `generateKey`.
 * - [ 4 bytes ] uint32_t additional_params_words: The size of the
 * additional parameters list that follows, in 32-bit words.
 * - [ m bytes ] Serialized KeyMint tags for additional parameters
 * (e.g., APPLICATION_ID).
 *
 * Output Buffer (`buf`):
 * A BeginResult structure.
 * - [ 8 bytes ] uint64_t challenge: Currently unused, set to 0.
 * - [ 4 bytes ] uint32_t key_params_size_bytes: Size of KeyParameters that
 * follow. Currently 0.
 * - [ n bytes ] KeyParameters (e.g., IV). Currently empty.
 * - [ 4 bytes ] uint32_t operation_id: A unique handle for this operation.
 *
 * @param km KeyMint context.
 * @param buf Input/Output buffer.
 * @param buf_size_words Size of the I/O buffer in 32-bit words.
 * @param req_len_words Size of the input data in 32-bit words.
 * @param out_len_bytes On success, the number of bytes written to the buffer.
 * @return SB_OK on success, or an error code on failure.
 */
static enum strongbox_error sb_Begin(struct km *km, uint32_t *buf,
				     size_t buf_size_words,
				     size_t req_len_words,
				     size_t *out_len_bytes)
{
	uint32_t purpose, blob_words, params_words;
	struct km_key_params params = { 0 };
	enum strongbox_error err;
	uint32_t slot;
	bool unique = true;
	uint32_t operation_id;

	*out_len_bytes = 0;
	if (req_len_words < 13)
		return SBERR_InvalidArgument;
	if (buf_size_words < 4)
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
	if (params.attrs.algorithm != KM_ALG_EC)
		return SBERR_UnsupportedAlgorithm;

	/* TODO: EC keys can be Sign or Agree */
	if (purpose != KM_PURPOSE_SIGN)
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

DECLARE_STRONGBOX_COMMAND(SB_DeviceBegin, sb_Begin);

/**
 * @brief Implements the core logic for IKeyMintOperation::update().
 *
 * Provides data to an ongoing cryptographic operation. For signing operations,
 * this data is added to the internal digest calculation.
 *
 * Input Buffer (`buf`):
 * - [ 4 bytes ] uint32_t operation_id: The handle from `begin()`.
 * - [ 4 bytes ] uint32_t update_size_bytes: The size of the data to process.
 * - [ n bytes ] The input data to be added to the operation.
 *
 * Output Buffer (`buf`):
 * - Empty. `*out_len_bytes` is set to 0.
 *
 * @param km KeyMint context.
 * @param buf Input/Output buffer.
 * @param buf_size_words Size of the I/O buffer in 32-bit words.
 * @param req_len_words Size of the input data in 32-bit words.
 * @param out_len_bytes On success, the number of bytes written to the buffer.
 * @return SB_OK on success, or an error code on failure.
 */
static enum strongbox_error sb_Update(struct km *km, uint32_t *buf,
				      size_t buf_size_words,
				      size_t req_len_words,
				      size_t *out_len_bytes)
{
	size_t op_index, update_size;

	*out_len_bytes = 0;
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

	return SB_OK;
}
DECLARE_STRONGBOX_COMMAND(SB_OperationUpdate, sb_Update);

/**
 * @brief Implements the core logic for IKeyMintOperation::finish().
 *
 * Finalizes a cryptographic operation. For a signing operation, it processes
 * the last piece of data, computes the final digest, signs it, and returns
 * the signature.
 *
 * Input Buffer (`buf`):
 * - [ 4 bytes ] uint32_t operation_id: The handle from `begin()`.
 * - [ 4 bytes ] uint32_t update_size_bytes: The size of the final data.
 * - [ n bytes ] The final input data for the operation. Can be empty.
 *
 * Output Buffer (`buf`):
 * - [ 64 bytes ] An ECDSA P-256 signature, composed of the 32-byte 'r'
 * value followed by the 32-byte 's' value.
 *
 * @param km KeyMint context.
 * @param buf Input/Output buffer.
 * @param buf_size_words Size of the I/O buffer in 32-bit words.
 * @param req_len_words Size of the input data in 32-bit words.
 * @param out_len_bytes On success, the number of bytes written to the buffer.
 * @return SB_OK on success, or an error code on failure.
 */
static enum strongbox_error sb_Finish(struct km *km, uint32_t *buf,
				      size_t buf_size_words,
				      size_t req_len_words,
				      size_t *out_len_bytes)
{
	size_t op_index, update_size;
	enum dcrypto_result result;
	const uint32_t *sign_data = NULL;

	*out_len_bytes = 0;
	if (req_len_words < 2)
		return SBERR_InvalidArgument;
	update_size = buf[1];
	if (update_size > (req_len_words - 2) * 4)
		return SBERR_InvalidArgument;
	if (buf_size_words < 16) { /* 64 bytes = 16 words */
		*out_len_bytes = 0;
		return SBERR_InvalidArgument;
	}
	for (op_index = 0; op_index < ARRAY_SIZE(km->ops); op_index++)
		if (km->ops[op_index].operation_id == buf[0])
			break;
	if (op_index >= ARRAY_SIZE(km->ops))
		return SBERR_InvalidOperationHandle;

	if (km->ops[op_index].attrs.algorithm != KM_ALG_EC)
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

	result = DCRYPTO_p256_ecdsa_sign((p256_int *)km->ops[op_index].key,
					 (p256_int *)sign_data,
					 (p256_int *)&buf[0],
					 (p256_int *)&buf[8]);

	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	/* Clean up everything by operation id */
	always_memset(&km->ops[op_index].attrs, 0,
		      sizeof(km->ops[op_index]) -
			      offsetof(struct km_operation, attrs));
	km->used_slots &= ~(1u << op_index);
	*out_len_bytes = 64;
	return SB_OK;
}

DECLARE_STRONGBOX_COMMAND(SB_OperationFinish, sb_Finish);

#define U32(a, b, c, d) (((a) | (b << 8) | (c << 16) | (d << 24)))

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
static enum strongbox_error sb_GenerateKeyPair(struct km *km, uint32_t *buf,
					       size_t buf_size_words,
					       size_t req_len_words,
					       size_t *out_len_bytes)
{
	static const uint32_t attest_key_params[] = {
		17,
		KM_TAG(KM_TAG_PURPOSE, 0),
		KM_PURPOSE_ATTEST_KEY,
		KM_TAG(KM_TAG_ALGORITHM, 0),
		KM_ALG_EC,
		KM_TAG(KM_TAG_KEY_SIZE, 0),
		256,
		KM_TAG(KM_TAG_EC_CURVE, 0),
		KM_EC_CURVE_P_256,
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
	size_t total_words = buf_size_words;
	uint32_t *b32;
	uint8_t *b8;

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

	/* After the key blob add MacedPublicKey, but check we have space. */
	if (total_words > buf_size_words - 25)
		return SBERR_UnknownError;

	/* Place the length of the Mac'ed key in bytes.	 */
	buf[total_words] = 87 + 32;

	/**
	 * MacedPublicKey = [                     ; COSE_Mac0 [RFC9052 s6.2]
	 *      protected: bstr .cbor { 1 : 5},    ; Algorithm : HMAC-256
	 *      unprotected: { },
	 *      payload : bstr .cbor PublicKey,
	 *      tag : bstr ; HMAC-256(K_mac, MAC_structure)
	 * ]
	 *; - P256 is BE: https://www.secg.org/sec1-v2.pdf#page=19
	 * (section 2.3.7) PublicKey = {               ; COSE_Key [RFC9052 s7]
	 *     1 : 2,                  ; Key type : EC2
	 *     3 : -7,                 ; Algorithm : ES256
	 *    -1 : 1,                 ; Curve : P256
	 *    -2 : bstr,              ; X coordinate, big-endian
	 *    -3 : bstr,              ; Y coordinate, big-endian
	 * }
	 */
	b32 = buf + total_words + 1;
	b8 = (uint8_t *)b32;
	b32[0] = U32(CBOR_HDR1(CBOR_MAJOR_ARR, 4),
		     CBOR_HDR1(CBOR_MAJOR_BSTR, 3),
		     CBOR_HDR1(CBOR_MAJOR_MAP, 1), CBOR_UINT0(1));
	b32[1] = U32(CBOR_UINT0(5), CBOR_HDR1(CBOR_MAJOR_MAP, 0), /* Unprotected
								   */
		     CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1), 77);
	b32[2] = U32(/* Public key */
		     CBOR_HDR1(CBOR_MAJOR_MAP, 5), /* Map header: 5 entries*/
		     COSE_KEY_LABEL_KTY, CBOR_UINT0(2), /* EC2 */
		     COSE_KEY_LABEL_ALG);
	b32[3] = U32(CBOR_NINT0(-7), /* ECDSA w/SHA-256 */
		     CBOR_NINT0(-1), CBOR_UINT0(1) /* -1 : 1 P256 */,
		     CBOR_NINT0(-2));

	b32[4] = U32(CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1),
		     32 /* -2 : bstr(32)*/, 0, 0);

	p256_to_bin(&km->last_pk_x, b8 + 18);
	b8[50] = CBOR_NINT0(-3); /* Y coordinate */
	b8[51] = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
	b8[52] = 32;
	p256_to_bin(&km->last_pk_y, b8 + 53);
	/* tag */
	b8[85] = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
	b8[86] = SHA256_DIGEST_SIZE;

	/* Add an HMAC tag for integrity */
	result = DCRYPTO_hw_hmac_sha256_init(&sha, km->hmac_tag_key,
					     sizeof(km->hmac_tag_key));
	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	HMAC_SHA256_update(&sha, b32, 87);
	digest = HMAC_SHA256_final(&sha);

	memcpy(b8 + 87, digest->b8, SHA256_DIGEST_SIZE);
	*out_len_bytes = total_words * sizeof(uint32_t) + 87 + 32 + 4 + 1;
	return err;
}

DECLARE_STRONGBOX_COMMAND(SB_RpcGenerateEcdsaP256KeyPair, sb_GenerateKeyPair);

/**
 * Implementation of the GSC specific GetDiceChain command.
 *
 * Due to limits on the TPM command size and the fact that DICE chain is a
 * constant for given firmware/device, move it out of the
 * GenerateCertificateV2Request to separate command which result can be cached
 * by the Strongbox TA.
 * This command doesn't take any arguments and returns DICE chain in CBOR
 * encoding as is.
 *
 * @param km Keymint context
 * @param buf Input buffer
 * @param buf_size_words Input buffer size in 32-bit words
 * @param req_len_words Request size in words
 * @param out_len_bytes Output buffer size in bytes
 * @return enum strongbox_error
 */
enum strongbox_error sb_GetDiceChain(struct km *km, uint32_t *buf,
				     size_t buf_size_words,
				     size_t req_len_words,
				     size_t *out_len_bytes)
{
	*out_len_bytes = 0;

	/* No arguments are expected for the command. */
	if (req_len_words)
		return SBERR_InvalidArgument;

	/* DiceCertChain */
	*out_len_bytes = get_dice_chain_bytes_for_chain(
		(uint8_t *)buf, 0, buf_size_words * sizeof(uint32_t),
		BOOT_PARAM_DICE_CHAIN_GSC);
	return (*out_len_bytes) ? SB_OK : SBERR_UnknownError;
}
DECLARE_STRONGBOX_COMMAND(SB_GetDiceChain, sb_GetDiceChain);

/**
 * Implementation for GenerateCertificateV2Request command. Unlike Keymint's
 * implementation we only process 1 mac'ed key at a time due to in/out size
 * constraints, so Strongbox'es TA counterpart would have to split single
 * request with many mac'ed keys into a sequence of requests.
 *
 * @param km Keymint context
 * @param buf Input buffer
 * @param buf_size_words Input buffer size in 32-bit words
 * @param req_len_words Request size in words
 * @param out_len_bytes Output buffer size in bytes
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
static enum strongbox_error sb_GenerateCertificateReq(struct km *km,
						      uint32_t *buf,
						      size_t buf_size_words,
						      size_t req_len_words,
						      size_t *out_len_bytes)
{
	size_t maced_len;
	size_t challenge_len, challenge_words, uds_cert_len, dice_size;
	enum dcrypto_result result;
	const struct sha256_digest *digest;
	struct hmac_sha256_ctx sha;
	uint32_t public_key[20];
	uint32_t challenge[16];
	uint8_t *b8;

	if (req_len_words < 34)
		return SBERR_InvalidArgument;

	*out_len_bytes = 0;
	maced_len = buf[0];

	if (maced_len != 87 + 32)
		return SBERR_InvalidArgument;

	b8 = (uint8_t *)(buf + 1);

	/* Add an HMAC tag for integrity */
	result = DCRYPTO_hw_hmac_sha256_init(&sha, km->hmac_tag_key,
					     sizeof(km->hmac_tag_key));
	if (result != DCRYPTO_OK)
		return SBERR_UnknownError;

	HMAC_SHA256_update(&sha, b8, 87);
	digest = HMAC_SHA256_final(&sha);

	if (memcmp(digest->b8, b8 + 87, SHA256_DIGEST_SIZE) != 0)
		return SBERR_RKP_STATUS_INVALID_MAC;

	challenge_len = buf[31];
	challenge_words =
		(challenge_len + sizeof(uint32_t) - 1) / sizeof(uint32_t);

	if (challenge_len > 64 || (challenge_words + 27 > req_len_words))
		return SBERR_InvalidArgument;

	memcpy(challenge, buf + 32, challenge_len);

	uds_cert_len = buf[32 + challenge_words];

	if ((uds_cert_len / sizeof(uint32_t) + 34 > req_len_words))
		return SBERR_InvalidArgument;

	/* Copy public key structure locally */
	memcpy(public_key, b8 + 8, 77);

	/* Next item is UDS certificate provided by the caller. */

	b8 = (uint8_t *)buf;
	/**
	 * Csr = AuthenticatedRequest<CsrPayload>
	 *
	 * AuthenticatedRequest<T> = [
	 *    version: 1,  ; The AuthenticatedRequest CDDL Schema version.
	 *    UdsCerts,
	 *    DiceCertChain,
	 *    SignedData<[
	 *       challenge: bstr .size (0..64),
	 *       bstr .cbor T,
	 *    ]>,
	 * ];
	 *  ; COSE_Sign1 (untagged) [RFC9052 s4.2]
	 * SignedData<Data> = [
	 * protected: bstr .cbor { 1 : AlgorithmES256 },
	 *     unprotected: {},
	 *     payload: bstr .cbor Data / nil,
	 * ; ECDSA(CDI_Leaf_Priv, SignedDataSigStruct<Data>)
	 *     signature: bstr
	 * ]
	 * ; Sig_structure for SignedData [ RFC9052 s4.4]
	 * SignedDataSigStruct<Data> = [
	 *     context: "Signature1",
	 *     protected: bstr .cbor { 1 : AlgorithmES256 },
	 *     external_aad: bstr .size 0,
	 *     payload: bstr .cbor Data / nil,
	 * ]
	 */

	/* AuthenticatedRequest<CsrPayload>, 4 elements array */
	b8[0] = CBOR_HDR1(CBOR_MAJOR_ARR, 4);
	/* version: 1, The AuthenticatedRequest CDDL Schema version.*/
	b8[1] = CBOR_UINT0(1);

	b8 += 2;
	/* Copy UdsCerts, assumes correct CBOR format */
	memmove(b8, buf + 32 + challenge_words + 1, uds_cert_len);

	b8 += uds_cert_len;
	/* DiceCertChain */
	dice_size = get_dice_chain_bytes_for_chain(
		b8, 0, buf_size_words * sizeof(uint32_t) - uds_cert_len - 2,
		BOOT_PARAM_DICE_CHAIN_GSC);

	b8 += dice_size;
	/* Start of SignedData */
	*b8++ = CBOR_HDR1(CBOR_MAJOR_ARR, 2);
	*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
	*b8++ = challenge_len;
	memcpy(b8, challenge, challenge_len);
	b8 += challenge_len;
	/*
	 * SignedData<CsrPayload> payload as bstr
	 * ; CBOR Array defining the payload for Csr
	 * CsrPayload = [
	 *   version: 3,
	 *   CertificateType: tstr, ; "keymint"
	 *   DeviceInfo,
	 *   KeysToSign,
	 * ]
	 * DeviceInfo = {
	 * "brand" : tstr,
	 * "manufacturer" : tstr,
	 *     "product" : tstr,
	 *     "model" : tstr,
	 *     "device" : tstr,
	 *   ; Taken from the AVB values
	 *     "vb_state" : "green" / "yellow" / "orange",
	 *     "bootloader_state" : "locked" / "unlocked",
	 *     "vbmeta_digest": bstr,
	 *   ; Same as android.os.Build.VERSION.release. Not optional for TEE.
	 *     ? "os_version" : tstr,                         ;
	 *   ; YYYYMM, must match KeyMint OS_PATCHLEVEL
	 *     "system_patch_level" : uint,
	 *   ; YYYYMMDD, must match KeyMint BOOT_PATCHLEVEL
	 *     "boot_patch_level" : uint,
	 *   ; YYYYMMDD, must match KeyMint VENDOR_PATCHLEVEL
	 *     "vendor_patch_level" : uint,
	 *     "security_level" : "tee" / "strongbox",
	 *   ; 1 if secure boot is enforced for the processor that the IRPC
	 *     "fused": 1 / 0,
	 * }
	 *
	 */

	/* Length of the signed data */
	*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES2);
	*b8++ = 0;
	*b8++ = 0;

	/*
	 * KeysToSign = [ * PublicKey ]
	 * ; NOTE: Integer encoding is different for Ed25519 and P256 keys:
	 * ;       - Ed25519 is LE:
	 * https://www.rfc-editor.org/rfc/rfc8032#section-3.1 ;       - P256 is
	 * BE: https://www.secg.org/sec1-v2.pdf#page=19 (section 2.3.7)
	 * PublicKey = {               ; COSE_Key [RFC9052 s7]
	 * 1 : 2,                  ; Key type : EC2
	 * 3 : -7,                 ; Algorithm : ES256
	 * -1 : 1,                 ; Curve : P256
	 * -2 : bstr,              ; X coordinate, big-endian
	 * -3 : bstr,              ; Y coordinate, big-endian
	 * }
	 */
	*out_len_bytes = 7 + uds_cert_len + dice_size + challenge_len;
	return SB_OK;
}

DECLARE_STRONGBOX_COMMAND(SB_RpcGenerateCertificateV2Request,
			  sb_GenerateCertificateReq);
