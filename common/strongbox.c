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
	const uint8_t *certificate_subject;
	size_t certificate_subject_len;
	const uint8_t *certificate_serial;
	size_t certificate_serial_len;
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

static size_t SB_cert_name(const p256_int *d, const p256_int *pk_x,
			   const p256_int *pk_y, const p256_int *serial,
			   const char *name, uint8_t *cert, const size_t n);

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
 * A 32-byte structure containing hardware details. The caller must provide an
 * output buffer of at least 32 bytes.
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
	static const uint8_t r[32] = { /* version */
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
	case KM_TAG_CERTIFICATE_SUBJECT:
		if (params->certificate_subject != NULL)
			return SBERR_InvalidTag;
		params->certificate_subject_len =
			prefix_get_data_len_bytes(tag_word);
		params->certificate_subject = (const uint8_t *)&p_tag[1];
		break;
	case KM_TAG_CERTIFICATE_SERIAL:
		if (params->certificate_subject != NULL)
			return SBERR_InvalidTag;
		params->certificate_serial_len =
			prefix_get_data_len_bytes(tag_word);
		params->certificate_serial = (const uint8_t *)&p_tag[1];
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

enum attest {
	ATTEST_FALSE,
	ATTEST_TRUE,
	ATTEST_NO_KEY,
};

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
static enum strongbox_error generate_key_blob(
	struct km *km, const uint32_t *tags, size_t tag_words,
	uint32_t *out_buf, size_t *out_words, enum attest attest)
{
	struct km_key_params params = { 0 };
	struct km_key_params sign_params = { 0 };
	p256_int attest_key;
	uint32_t *key_blob_size = out_buf;
	size_t blob_start_words = 0;
	size_t blob_size_words;
	size_t total_words;
	size_t out_buf_words = *out_words;
	uint32_t *cert_size;
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

	if (attest != ATTEST_FALSE) {
		size_t attest_key_space, attest_key_size = 0;
		/* Check below is already done, but keep it for safety */
		if (tags[0] > tag_words - 1)
			return SBERR_UnknownError;
		attest_key_space = tag_words - tags[0] - 1;
		if (attest_key_space >= 1) {
			/* peek into blob's total size */
			attest_key_size = tags[tags[0] + 1];
			/* Check if attest key blob is plausible */
			if (attest_key_size + 1 != attest_key_space)
				return SBERR_InvalidKeyBlob;
			/* Import attestation key, skipping length */
			err = import_blob(km, tags + tags[0] + 2,
					  attest_key_size, NULL, 0,
					  &sign_params, attest_key.a);
			if (err != SB_OK)
				return err;
			if ((sign_params.attrs.purpose_flags &
			     ((1 << KM_PURPOSE_SIGN) |
			      (1 << KM_PURPOSE_ATTEST_KEY))) == 0)
				return SBERR_IncompatiblePurpose;
		} else /* Internally mark that we don't have attestation key */
			attest = ATTEST_NO_KEY;
	}

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

	total_words = blob_start_words + blob_size_words;
	/* Total size of key blob in 32-bit words */
	*key_blob_size = total_words;
	cert_size = out_buf + total_words;
	if (attest == ATTEST_TRUE) {
		/* TODO: pass Key parameters with serial, subject */
		*cert_size = SB_cert_name(
			&attest_key, &km->last_pk_x, &km->last_pk_x,
			&km->last_pk_x /* serial */, "Android Keystore Key",
			(uint8_t *)(cert_size + 1),
			(out_buf_words - total_words - 1) * sizeof(uint32_t));
		/* Prepend certificate with 32-bit little-endian size field. */
		total_words += (*cert_size + 3 + sizeof(*cert_size)) /
			       sizeof(uint32_t);
	} else if (attest == ATTEST_NO_KEY)
		*cert_size = 0;
	*out_words = total_words + 1; /* + blob size */
	return SB_OK;
};

/**
 * @brief Implements the core logic for IKeyMintDevice.generateKey().
 *
 * Parses key generation parameters, creates a hardware-backed key, and returns
 * it as an encrypted, versioned, and integrity-protected key blob.
 *
 * Input Buffer (`buf`):
 * A serialized list of Key parameters(tags).
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
 * Note: that key blob include KeyCharacteristics as its part, but in AIDL
 * KeyCharacteristics are returned separately as key blob format is opaque.
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
	err = generate_key_blob(km, buf, req_len_words, out_buf, &total_words,
				true);
	if (err != SB_OK)
		return err;

	if (total_words * sizeof(uint32_t) >= out_len_max)
		return SBERR_UnknownError;

	memcpy(buf, out_buf, total_words * sizeof(uint32_t));

	*out_len_bytes = total_words * sizeof(uint32_t);
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
 * The result of the operation. Specifically for the Strongbox-subset it is:
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
 *  macedPublicKey);
 *
 * The data returned by this function is a concatenation of two parts:
 * 1. A handle to the private key, which is a key blob.
 * 2. The MacedPublicKey.
 *
 * The format is as follows, with all fields being 32-bit aligned:
 *
 * - [ variable size ] Key Blob: A handle to the private key. The first 4 bytes
 *   of the key blob contain its total size in 32-bit words.
 *
 * - [ 4 bytes ] MacedPublicKey Length: The size of the MacedPublicKey data in
 *   bytes.
 *
 * - [ variable size ] MacedPublicKey Data: The CBOR-encoded MacedPublicKey.
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
				&total_words, false);
	if (err != SB_OK)
		return err;

#define CBOR_MACED_KEY_LEN (87 + 32)
#define CBOR_MACED_KEY_WORDS \
	((CBOR_MACED_KEY_LEN + sizeof(uint32_t) - 1) / sizeof(uint32_t))

	/* After the key blob add MacedPublicKey, but check we have space. */
	if (total_words + CBOR_MACED_KEY_WORDS > buf_size_words)
		return SBERR_UnknownError;

	/* Place the length of the Mac'ed key in bytes.	 */
	buf[total_words] = CBOR_MACED_KEY_LEN;

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
#define CBOR_PUBLIC_KEY_LEN 77
#define CBOR_PUBLIC_KEY_WORDS \
	((CBOR_PUBLIC_KEY_LEN + sizeof(uint32_t) - 1) / sizeof(uint32_t))

	b32 = buf + total_words + 1;
	b8 = (uint8_t *)b32;
	b32[0] = U32(CBOR_HDR1(CBOR_MAJOR_ARR, 4),
		     CBOR_HDR1(CBOR_MAJOR_BSTR, 3),
		     CBOR_HDR1(CBOR_MAJOR_MAP, 1), CBOR_UINT0(1));
	b32[1] = U32(CBOR_UINT0(5),
		     /* unprotected: { } */
		     CBOR_HDR1(CBOR_MAJOR_MAP, 0),
		     CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1),
		     CBOR_PUBLIC_KEY_LEN);
	b32[2] = U32(/* Public key, Map header: 5 entries*/
		     CBOR_HDR1(CBOR_MAJOR_MAP, 5), COSE_KEY_LABEL_KTY,
		     CBOR_UINT0(2), /* EC2 */
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
	*out_len_bytes = /* key blob, 4-byte len, MACed Key, 1-byte padding */
		total_words * sizeof(uint32_t) + CBOR_MACED_KEY_LEN + 4 + 1;
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
 * Implementation for GenerateCertificateV2Request command.
 *
 * @param km Keymint context
 * @param buf Input buffer
 * @param buf_size_words Input buffer size in 32-bit words
 * @param req_len_words Request size in words
 * @param out_len_bytes Output buffer size in bytes
 * @return enum strongbox_error
 *
 * generateCertificateRequestV2 creates a certificate signing request to be sent
 * to the provisioning server. Implements:
 * byte[] generateCertificateRequestV2(in MacedPublicKey[] keysToSign, in byte[]
 * challenge);
 *
 * Input Buffer (`buf`) Format:
 * The input buffer is a sequence of fields, all 32-bit aligned.
 * - [ 4 bytes ] uint32_t key_count: Number of MACed public keys.
 * @param in MacedPublicKey[] keysToSign contains the set of keys to certify.
 * The IRemotelyProvisionedComponent must validate the MACs on each key.  If any
 * entry in the array lacks a valid MAC, the method must return
 * STATUS_INVALID_MAC.  This method must not accept test keys. If any entry in
 * the array is a test key, the method must return
 * - [ n times ] Repeated for each key:
 *   - [ 4 bytes ] uint32_t maced_key_len_bytes: Length of the following
 *     MACed key.
 *   - [ m bytes ] uint8_t maced_key[]: The MACed public key data, padded
 *     to a 4-byte boundary.
 *        STATUS_TEST_KEY_IN_PRODUCTION_REQUEST.
 * @param in challenge contains a byte string from the provisioning server which
 * will be included in the signed data of the CSR structure. Different
 * provisioned backends may use different semantic data for this field, but the
 * supported sizes must be between 0 and 64 bytes, inclusive.
 * - [ 4 bytes ] uint32_t challenge_len_bytes: Length of the challenge.
 * - [ p bytes ] uint8_t challenge[]: The challenge data, padded to a 4-byte
 *   boundary.
 * Strongbox-subset specific:
 * - [ 4 bytes ] uint32_t device_info_len_bytes: Length of the DeviceInfo.
 * - [ q bytes ] uint8_t device_info[]: The CBOR-encoded DeviceInfo data.
 *   In the DeviceInfo map fields:
 *    - "security_level": "strongbox",
 *    - "vb_state": "green",
 *    - "bootloader_state": "locked"
 * have to be placed last, as future version would append these fields based
 * on the actual GSC state.
 * generateCertificateRequestV2 creates a SignedData<[challenge,
 * CSR]> part of the CSR to be sent to the provisioning server.
 * This is part need to be prepended with the
 * AuthenticatedRequest<CsrPayload> structure
 * [ version: 1, UdsCerts, DiceCertChain,  {SignedData<Data>} ]
 *
 * @return a CBOR Certificate Signing Request (Csr) Signed Data
 * serialized into a byte array.
 *         See generateCertificateRequestV2.cddl for CDDL definitions.
 *
 */
static inline struct slice_ref_s slice(const size_t size, const uint8_t *data)
{
	return (struct slice_ref_s){ .data = data, .size = size };
}
#define MAX_CSR_PUB_KEYS 20
static enum strongbox_error sb_GenerateCertificateReq(struct km *km,
						      uint32_t *buf,
						      size_t buf_size_words,
						      size_t req_len_words,
						      size_t *out_len_bytes)
{
	size_t key_count, maced_len, device_info_len;
	enum dcrypto_result result;
	const struct sha256_digest *digest;
	struct hmac_sha256_ctx sha;
	uint32_t public_key[MAX_CSR_PUB_KEYS][CBOR_PUBLIC_KEY_WORDS];
	uint32_t challenge[16], challenge_words, challenge_len;
	uint8_t *b8, *data_to_sign, *data_start;
	size_t index, data_len, data_len_to_sign, prefix_bytes;
	static const uint8_t kSigStructFixedHdr[] = {
		/* Array header: 4 elements */
		CBOR_HDR1(CBOR_MAJOR_ARR, 4),
		/* 1. Context: tstr("Signature1") */
		CBOR_HDR1(CBOR_MAJOR_TSTR, CDI_SIG_STRUCT_CONTEXT_VALUE_LEN),
		'S',
		'i',
		'g',
		'n',
		'a',
		't',
		'u',
		'r',
		'e',
		'1',
		/* 2. Body protected: bstr(COSE param) */
		/* BSTR of size 3 - see the rest of the struct */
		CBOR_HDR1(CBOR_MAJOR_BSTR, 3),
		/* Map header: 1 elem */
		CBOR_HDR1(CBOR_MAJOR_MAP, 1),
		/* Alg: uint(1) => nint(-7) */
		COSE_PARAM_LABEL_ALG,
		CBOR_NINT0(-7), /* ECDSA w/ SHA-256 */
		/* 3. External AAD: Bstr(0 bytes) */
		CBOR_HDR1(CBOR_MAJOR_BSTR, 0),
	};

	if (req_len_words < 34)
		return SBERR_InvalidArgument;

	*out_len_bytes = 0;

	key_count = buf[0];
	if (key_count == 0 || key_count > MAX_CSR_PUB_KEYS)
		return SBERR_InvalidArgument;

	/* Check that we have at least space for all the keys */
	if (key_count * (CBOR_PUBLIC_KEY_WORDS + 1) > buf_size_words)
		return SBERR_InvalidArgument;

	index = 1;
	for (size_t i = 0; i < key_count; i++) {
		maced_len = buf[index];

		if (maced_len != 87 + 32)
			return SBERR_InvalidArgument;

		b8 = (uint8_t *)(buf + index + 1);

		/* Add an HMAC tag for integrity */
		result = DCRYPTO_hw_hmac_sha256_init(&sha, km->hmac_tag_key,
						     sizeof(km->hmac_tag_key));
		if (result != DCRYPTO_OK)
			return SBERR_UnknownError;

		HMAC_SHA256_update(&sha, b8, 87);
		digest = HMAC_SHA256_final(&sha);

		if (memcmp(digest->b8, b8 + 87, SHA256_DIGEST_SIZE) != 0)
			return SBERR_RKP_STATUS_INVALID_MAC;

		/* Copy public key structure locally */
		memcpy(public_key[i], b8 + 8, CBOR_PUBLIC_KEY_LEN);
		index += CBOR_MACED_KEY_WORDS + 1;
	}
	challenge_len = buf[index];
	challenge_words =
		(challenge_len + sizeof(uint32_t) - 1) / sizeof(uint32_t);

	if (challenge_len > 64 || (challenge_words + index > req_len_words))
		return SBERR_InvalidArgument;

	memcpy(challenge, buf + index + 1, challenge_len);
	index += challenge_words + 1;

	device_info_len = buf[index];

	if ((device_info_len / sizeof(uint32_t) + index > req_len_words))
		return SBERR_InvalidArgument;

	/**
	 * Csr = AuthenticatedRequest<CsrPayload>
	 *
	 * AuthenticatedRequest<T> = [
	 *    version: 1,  ; The AuthenticatedRequest CDDL Schema version.
	 *    UdsCerts,
	 *    DiceCertChain,
	 *    --- we only provide SignedData part
	 *    SignedData<[
	 *       challenge: bstr .size (0..64),
	 *       bstr .cbor T,
	 *    ]>,
	 * ];
	 *
	 *  ; COSE_Sign1 (untagged) [RFC9052 s4.2]
	 * SignedData<Data> = [
	 *     protected: bstr .cbor { 1 : AlgorithmES256 },
	 *     unprotected: {},
	 *     payload: bstr .cbor Data / nil,
	 * ; ECDSA(CDI_Leaf_Priv, SignedDataSigStruct<Data>)
	 *     signature: bstr
	 * ]
	 */
	buf[0] = U32(
		/* SignedData<Data>, 4 elements array */
		CBOR_HDR1(CBOR_MAJOR_ARR, 4),
		/* bstr .cbor { 1 : AlgorithmES256 } 43 A10127 */
		CBOR_HDR1(CBOR_MAJOR_BSTR, 3), CBOR_HDR1(CBOR_MAJOR_MAP, 1),
		CBOR_HDR1(CBOR_MAJOR_UINT, 1));
	b8 = (uint8_t *)(buf + 1);
	*b8++ = CBOR_HDR1(CBOR_MAJOR_NINT, 7);
	/* unprotected: {} */
	*b8++ = CBOR_HDR1(CBOR_MAJOR_MAP, 0);
	prefix_bytes = 6;

	/* payload: bstr .cbor Data / nil, Data is array [challenge, CsrPayload]
	 */
	data_len = 1 + 2 + challenge_len + 10 + device_info_len + 1 +
		   key_count * CBOR_PUBLIC_KEY_LEN;

	if (data_len > 255) {
		*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES2);
		*b8++ = (uint8_t)(((data_len) & 0xFF00) >> 8);
		*b8++ = (uint8_t)((data_len) & 0x00FF);
		prefix_bytes += 3;
	} else {
		*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
		*b8++ = data_len;
		prefix_bytes += 2;
	}
	data_to_sign = b8;
	/* Now temporarily put here prefix of SignedDataSigStruct<Data> */
	/* ; Sig_structure for SignedData [ RFC9052 s4.4]
	 * SignedDataSigStruct<Data> = [
	 *     context: "Signature1",
	 *     protected: bstr .cbor { 1 : AlgorithmES256 },
	 *     external_aad: bstr .size 0,
	 *     payload: bstr .cbor Data / nil,
	 * ]
	 */

	memcpy(b8, kSigStructFixedHdr, sizeof(kSigStructFixedHdr));
	b8 += sizeof(kSigStructFixedHdr);
	if (data_len > 255) {
		*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES2);
		*b8++ = (uint8_t)(((data_len) & 0xFF00) >> 8);
		*b8++ = (uint8_t)((data_len) & 0x00FF);
		data_len_to_sign = data_len + sizeof(kSigStructFixedHdr) + 3;
	} else {
		*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
		*b8++ = data_len;
		data_len_to_sign = data_len + sizeof(kSigStructFixedHdr) + 2;
	}
	data_start = b8;
	/* challenge: bstr .size (0..64) */
	*b8++ = CBOR_HDR1(CBOR_MAJOR_ARR, 2);
	*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
	*b8++ = challenge_len;
	memcpy(b8, challenge, challenge_len);
	b8 += challenge_len;

	/*
	 * CsrPayload = [
	 *   version: 3, CertificateType: tstr, ; "keymint"
	 *   DeviceInfo, KeysToSign,
	 * ]
	 */
	*b8++ = CBOR_HDR1(CBOR_MAJOR_ARR, 4);
	*b8++ = CBOR_HDR1(CBOR_MAJOR_UINT, 3);
	*b8++ = CBOR_HDR1(CBOR_MAJOR_TSTR, 7);
	*b8++ = 'k';
	*b8++ = 'e';
	*b8++ = 'y';
	*b8++ = 'm';
	*b8++ = 'i';
	*b8++ = 'n';
	*b8++ = 't';

	/*
	 * DeviceInfo = {
	 * "brand" : tstr,
	 * "manufacturer" : tstr,
	 *     "product" : tstr,
	 *     "model" : tstr,
	 *     "device" : tstr,
	 *   ; Taken from the AVB values
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
	 *     "vb_state" : "green" / "yellow" / "orange",
	 *     "bootloader_state" : "locked" / "unlocked",
	 * }
	 *
	 */

	/* Move DeviceInfo - we can make it in-place */
	memmove(b8, buf + index + 1, device_info_len);
	b8 += device_info_len;

	/* KeysToSign = [ * PublicKey ] */
	*b8++ = CBOR_HDR1(CBOR_MAJOR_ARR, key_count);
	for (size_t i = 0; i < key_count; i++) {
		memcpy(b8, public_key[i], CBOR_PUBLIC_KEY_LEN);
		b8 += CBOR_PUBLIC_KEY_LEN;
	}
	*b8++ = CBOR_HDR1(CBOR_MAJOR_BSTR, CBOR_BYTES1);
	*b8++ = ECDSA_SIG_BYTES;

	if (!sign_with_cdi_key(BOOT_PARAM_DICE_CHAIN_GSC,
			       slice(data_len_to_sign, data_to_sign), b8))
		return SBERR_UnknownError;

	/* Now remove signing prefix */
	memmove(data_to_sign, data_start, data_len + 2 + ECDSA_SIG_BYTES);
	*out_len_bytes = prefix_bytes + data_len + ECDSA_SIG_BYTES + 2;
	return SB_OK;
}

DECLARE_STRONGBOX_COMMAND(SB_RpcGenerateCertificateV2Request,
			  sb_GenerateCertificateReq);

/* Limit the size of long form encoded objects to < 64 kB. */
#define MAX_ASN1_OBJ_LEN_BYTES 3

/* Reserve space for TLV encoding */
#define SEQ_SMALL  2 /* < 128 bytes (1B type, 1B 7-bit length) */
#define SEQ_MEDIUM 3 /* < 256 bytes (1B type, 1B length size, 1B length) */
#define SEQ_LARGE  4 /* < 65536 bytes (1B type, 1B length size, 2B length) */

/* Tag related constants. */
enum {
	V_ASN1_BOOL = 0x01,
	V_ASN1_INT = 0x02,
	V_ASN1_BIT_STRING = 0x03, /* OCTET STRING*/
	V_ASN1_BYTES = 0x04,
	V_ASN1_NULL = 0x05,
	V_ASN1_OBJ = 0x06,
	V_ASN1_ENUM = 0x0a,
	V_ASN1_UTF8 = 0x0c,
	V_ASN1_SEQUENCE = 0x10,
	V_ASN1_SET = 0x11,
	V_ASN1_ASCII = 0x13,
	V_ASN1_TIME = 0x18,
	V_ASN1_CONSTRUCTED = 0x20,
	/* short helpers */
	V_BITS = V_ASN1_BIT_STRING,
	V_SEQ = V_ASN1_CONSTRUCTED | V_ASN1_SEQUENCE,
	V_SET = V_ASN1_CONSTRUCTED | V_ASN1_SET,
};

struct asn1 {
	uint8_t *p;
	size_t n;
};

#define SEQ_START(X, T, L)         \
	do {                       \
		int __old = (X).n; \
		uint8_t __t = (T); \
		int __l = (L);     \
		(X).n += __l;
#define SEQ_END(X)                                                       \
	(X).n = asn1_seq((X).p + __old, __t, __l, (X).n - __old - __l) + \
		__old;                                                   \
	}                                                                \
	while (0)

#define OID(X) sizeof(OID_##X), OID_##X
static const uint8_t OID_ecdsa_with_SHA256[8] = { 0x2A, 0x86, 0x48, 0xCE,
						  0x3D, 0x04, 0x03, 0x02 };

static const uint8_t OID_id_ecPublicKey[7] = { 0x2A, 0x86, 0x48, 0xCE,
					       0x3D, 0x02, 0x01 };
static const uint8_t OID_prime256v1[8] = { 0x2A, 0x86, 0x48, 0xCE,
					   0x3D, 0x03, 0x01, 0x07 };

static const uint8_t OID_commonName[3] = { 0x55, 0x04, 0x03 };
static const uint8_t OID_organizationName[3] = { 0x55, 0x04, 0x0a };

/**
 * Encoding OID: 1.3.6.1.4.1.11129.2.1.17
 *  KeyDescription ::= SEQUENCE {
 *     attestationVersion           INTEGER, # Value 400
 *     attestationSecurityLevel     SecurityLevel,
 *     keyMintVersion               INTEGER, # Value 400
 *     keyMintSecurityLevel         SecurityLevel,
 *     attestationChallenge         OCTET_STRING,
 *     uniqueId                     OCTET_STRING,
 *     softwareEnforced             AuthorizationList,
 *     hardwareEnforced             AuthorizationList,
 * }
 *
 * SecurityLevel ::= ENUMERATED {
 *     Software                     (0),
 *     TrustedEnvironment           (1),
 *     StrongBox                    (2),
 * }
 */
static const uint8_t OID_keymint[10] = {
	0x2B, 0x06, 0x01, 0x04, 0x01, 0xD6, 0x79, 0x02, 0x01, 0x11,
};
/*
	SEQ_START(ctx, V_SEQ, SEQ_SMALL)
	{
		asn1_int(&ctx, 400);
		asn1_enum_int(
			&ctx,
			V_ASN1_ENUM, 2);
		asn1_int(&ctx, 400);
		asn1_enum_int(
			&ctx,
			V_ASN1_ENUM, 2);
	}
	SEQ_END(ctx);
*/
static const uint8_t keymint_extension[14] = {
	0x30, 0x0e, 0x02, 0x02, 0x01, 0x90, 0x0a,
	0x01, 0x02, 0x02, 0x02, 0x01, 0x90, 0x0a,
};

/**
 * Encoding OID: 1.3.6.1.4.1.11129.2.1.30
 * The extension value consists of Concise Binary Object Representation (CBOR)
 * data that conforms to this Concise Data Definition Language (CDDL) schema:
 * {1 : int,       ; certificates issued
 *  4 : string,    ; validated attested entity (STRONG_BOX/TEE) }
 * The map is unversioned and new optional fields may be added.
 * certs_issued - An approximate number of certificates issued to the device in
 * the last 30 days. This value can be used as a signal for potential abuse if
 * the value is greater than average by some orders of magnitude.
 * validated_attested_entity -
 * The validated attested entity is a string that describes the type of device
 * that was confirmed by the provisioning server to be attested. For example,
 * STRONG_BOX or TEE.
 * https://source.android.com/docs/security/features/keystore/attestation#provisioninginfo_extension_schema
 */
static const uint8_t OID_strongbox[10] = {
	0x2B, 0x06, 0x01, 0x04, 0x01, 0xD6, 0x79, 0x02, 0x01, 0x1e,
};
static const uint8_t CBOR_strongbox[15] = { CBOR_HDR1(CBOR_MAJOR_MAP, 2),
					    CBOR_HDR1(CBOR_MAJOR_UINT, 1),
					    CBOR_HDR1(CBOR_MAJOR_UINT, 10),
					    CBOR_HDR1(CBOR_MAJOR_UINT, 4),
					    CBOR_HDR1(CBOR_MAJOR_TSTR, 10),
					    'S',
					    'T',
					    'R',
					    'O',
					    'N',
					    'G',
					    '_',
					    'B',
					    'O',
					    'X' };

/* start a tag and return write ptr */
static uint8_t *asn1_tag(struct asn1 *ctx, uint8_t tag)
{
	ctx->p[(ctx->n)++] = tag;
	return ctx->p + ctx->n;
}

/* DER encode length and return encoded size thereof */
static int asn1_len(uint8_t *p, size_t size)
{
	if (size < 128) {
		p[0] = size;
		return 1;
	} else if (size < 256) {
		p[0] = 0x81;
		p[1] = size;
		return 2;
	}
	p[0] = 0x82;
	p[1] = size >> 8;
	p[2] = size;
	return 3;
}

/*
 * close sequence and move encapsulated data if needed
 * return total length.
 */
static size_t asn1_seq(uint8_t *p, uint8_t tag, size_t l, size_t size)
{
	size_t tl;

	p[0] = tag;
	tl = asn1_len(p + 1, size) + 1;
	/* TODO: tl > l fail */
	if (tl < l)
		memmove(p + tl, p + l, size);

	return tl + size;
}

static void asn1_enum_int(struct asn1 *ctx, uint8_t tag, uint32_t val)
{
	uint8_t *p = asn1_tag(ctx, tag);

	if (!val) {
		*p++ = 1;
		*p++ = 0;
	} else {
		int nbits = 32 - __builtin_clz(val);
		int nbytes = (nbits + 7) / 8;

		if ((nbits & 7) == 0) {
			*p++ = nbytes + 1;
			*p++ = 0;
		} else {
			*p++ = nbytes;
		}
		while (nbytes--)
			*p++ = val >> (nbytes * 8);
	}

	ctx->n = p - ctx->p;
}

/* DER encode (small positive) integer */
static void asn1_int(struct asn1 *ctx, uint32_t val)
{
	asn1_enum_int(ctx, V_ASN1_INT, val);
}

static void asn1_be_int(struct asn1 *ctx, const uint8_t *b, size_t n)
{
	uint8_t *p = asn1_tag(ctx, V_ASN1_INT);
	size_t i;

	for (i = 0; i < n; ++i) {
		if (b[i] != 0)
			break;
	}
	if (b[i] & 0x80) {
		*p++ = n - i + 1;
		*p++ = 0;
	} else {
		*p++ = n - i;
	}
	for (; i < n; ++i)
		*p++ = b[i];

	ctx->n = p - ctx->p;
}

/* DER encode positive p256_int */
static void asn1_p256_int(struct asn1 *ctx, const p256_int *n)
{
	uint8_t bn[P256_NBYTES];

	p256_to_bin(n, bn);
	asn1_be_int(ctx, bn, P256_NBYTES);
}

/* DER encode p256 signature */
static void asn1_sig(struct asn1 *ctx, const p256_int *r, const p256_int *s)
{
	SEQ_START(*ctx, V_SEQ, SEQ_SMALL)
	{
		asn1_p256_int(ctx, r);
		asn1_p256_int(ctx, s);
	}
	SEQ_END(*ctx);
}

/* DER encode bytes */
static void asn1_bytes(struct asn1 *ctx, uint8_t tag, const uint8_t *s,
		       size_t n)
{
	uint8_t *p = asn1_tag(ctx, tag);

	p += asn1_len(p, n);
	while (n--)
		*p++ = *s++;

	ctx->n = p - ctx->p;
}

/* DER encode printable string */
static void asn1_string(struct asn1 *ctx, uint8_t tag, const char *s)
{
	size_t n = strlen(s);

	asn1_bytes(ctx, tag, (const uint8_t *)s, n);
}

/* DER encode bytes */
static void asn1_object(struct asn1 *ctx, size_t n, const uint8_t *b)
{
	uint8_t *p = asn1_tag(ctx, V_ASN1_OBJ);

	p += asn1_len(p, n);
	while (n--)
		*p++ = *b++;

	ctx->n = p - ctx->p;
}

/* DER encode p256 pk */
static void asn1_pub(struct asn1 *ctx, const p256_int *x, const p256_int *y)
{
	uint8_t *p = asn1_tag(ctx, 4); /* uncompressed format */

	p256_to_bin(x, p);
	p += P256_NBYTES;
	p256_to_bin(y, p);
	p += P256_NBYTES;

	ctx->n = p - ctx->p;
}

static void add_name(struct asn1 *ctx, const char *oname, const char *cname)
{
	SEQ_START(*ctx, V_SEQ, SEQ_SMALL)
	{
		if (oname) {
			SEQ_START(*ctx, V_SET, SEQ_SMALL)
			{
				SEQ_START(*ctx, V_SEQ, SEQ_SMALL)
				{
					asn1_object(ctx, OID(organizationName));
					asn1_string(ctx, V_ASN1_UTF8, oname);
				}
				SEQ_END(*ctx);
			}
			SEQ_END(*ctx);
		}
		if (cname) {
			SEQ_START(*ctx, V_SET, SEQ_SMALL)
			{
				SEQ_START(*ctx, V_SEQ, SEQ_SMALL)
				{
					asn1_object(ctx, OID(commonName));
					asn1_string(ctx, V_ASN1_UTF8, cname);
				}
				SEQ_END(*ctx);
			}
			SEQ_END(*ctx);
		}
	}
	SEQ_END(*ctx);
}

static size_t SB_cert_name(const p256_int *d, const p256_int *pk_x,
			   const p256_int *pk_y, const p256_int *serial,
			   const char *name, uint8_t *cert, const size_t n)
{
	struct asn1 ctx = { cert, 0 };
	struct sha256_ctx sha;
	p256_int h, r, s;
	struct drbg_ctx drbg;
	enum dcrypto_result result;

	SEQ_START(ctx, V_SEQ, SEQ_LARGE)
	{ /* outer seq */
		/*
		 * Grab current pointer to data to hash later.
		 * Note this will fail if cert body + cert sign is less
		 * than 256 bytes (SEQ_MEDIUM) -- not likely.
		 */
		uint8_t *body = ctx.p + ctx.n;

		/* Cert body seq */
		SEQ_START(ctx, V_SEQ, SEQ_LARGE)
		{
			/* X509 v3 */
			SEQ_START(ctx, 0xa0, SEQ_SMALL)
			{
				asn1_int(&ctx, 2);
			}
			SEQ_END(ctx);

			/* Serial number */
			if (serial)
				asn1_p256_int(&ctx, serial);
			else
				asn1_int(&ctx, 1);

			/* Signature algo */
			SEQ_START(ctx, V_SEQ, SEQ_SMALL)
			{
				asn1_object(&ctx, OID(ecdsa_with_SHA256));
			}
			SEQ_END(ctx);

			/* Issuer: Same as the subject field of the batch
			 * attestation key.
			 */
			add_name(&ctx, "Google LLC", "CR50");

			/* Expiry */
			SEQ_START(ctx, V_SEQ, SEQ_SMALL)
			{
				asn1_string(&ctx, V_ASN1_TIME,
					    "20000101000000Z");
				asn1_string(&ctx, V_ASN1_TIME,
					    "20991231235959Z");
			}
			SEQ_END(ctx);

			/* Subject */
			add_name(&ctx, "StrongBox", name);

			/* Subject pk */
			SEQ_START(ctx, V_SEQ, SEQ_SMALL)
			{
				/* pk parameters */
				SEQ_START(ctx, V_SEQ, SEQ_SMALL)
				{
					asn1_object(&ctx, OID(id_ecPublicKey));
					asn1_object(&ctx, OID(prime256v1));
				}
				SEQ_END(ctx);
				/* pk bits */
				SEQ_START(ctx, V_BITS, SEQ_SMALL)
				{
					/* No unused bit at the end */
					asn1_tag(&ctx, 0);
					asn1_pub(&ctx, pk_x, pk_y);
				}
				SEQ_END(ctx);
			}
			SEQ_END(ctx);

			/* Certificate extension */
			SEQ_START(ctx, 0xa3, SEQ_SMALL)
			{
				SEQ_START(ctx, V_SEQ, SEQ_SMALL)
				{
					SEQ_START(ctx, V_SEQ, SEQ_SMALL)
					{
						asn1_object(&ctx, OID(keymint));
						asn1_bytes(
							&ctx, V_ASN1_BYTES,
							keymint_extension,
							sizeof(keymint_extension));
					}
					SEQ_END(ctx);
					SEQ_START(ctx, V_SEQ, SEQ_SMALL)
					{
						asn1_object(&ctx,
							    OID(strongbox));
						asn1_bytes(
							&ctx, V_ASN1_BYTES,
							CBOR_strongbox,
							sizeof(CBOR_strongbox));
					}
					SEQ_END(ctx);
				}
				SEQ_END(ctx);
			}
			SEQ_END(ctx);
		}
		SEQ_END(ctx); /* Cert body */

		/* Sign all of cert body */
		SHA256_hw_init(&sha);
		SHA256_update(&sha, body, (ctx.p + ctx.n) - body);
		p256_from_bin(SHA256_final(&sha)->b8, &h);
		hmac_drbg_init_rfc6979(&drbg, d, &h);
		result = dcrypto_p256_ecdsa_sign(&drbg, d, &h, &r, &s);
		drbg_exit(&drbg);
		if (result != DCRYPTO_OK)
			return 0;

		/* Append X509 signature */
		SEQ_START(ctx, V_SEQ, SEQ_SMALL);
		asn1_object(&ctx, OID(ecdsa_with_SHA256));
		SEQ_END(ctx);
		SEQ_START(ctx, V_BITS, SEQ_SMALL)
		{
			/* no unused/zero bit at the end */
			asn1_tag(&ctx, 0);
			asn1_sig(&ctx, &r, &s);
		}
		SEQ_END(ctx);
	}
	SEQ_END(ctx); /* end of outer seq */

	return ctx.n;
}
