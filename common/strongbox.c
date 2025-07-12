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

enum strongbox_error sb_GetHardwareInfo(struct vendor_cmd_params *p)
{
	static const uint8_t r[35] = { /* version */
				       0x00, 0x00, 0x00, 0x00,
				       /* SecurityLevel Strongbox */
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

bool is_gen_disallowed(keymint_tag_t tag)
{
	switch (tag) {
	case TAG_INVALID:
	case TAG_MAX_USES_PER_BOOT:
	case TAG_MIN_SECONDS_BETWEEN_OPS:
	case TAG_ORIGIN:
	case TAG_ROOT_OF_TRUST:
	case TAG_OS_VERSION:
	case TAG_OS_PATCHLEVEL:
	case TAG_RESET_SINCE_ID_ROTATION:
		return true;
	default:
		return false;
	}
}

bool is_hw_enforced(keymint_tag_t tag)
{
	// Simplified logic based on "Must be hardware-enforced" comments in
	// Rust source.
	switch (tag) {
	case TAG_PURPOSE:
	case TAG_ALGORITHM:
	case TAG_KEY_SIZE:
	case TAG_BLOCK_MODE:
	case TAG_DIGEST:
	case TAG_PADDING:
	case TAG_CALLER_NONCE:
	case TAG_MIN_MAC_LENGTH:
	case TAG_EC_CURVE:
	case TAG_RSA_PUBLIC_EXPONENT:
	case TAG_INCLUDE_UNIQUE_ID:
	case TAG_RSA_OAEP_MGF_DIGEST:
	case TAG_BOOTLOADER_ONLY:
	case TAG_ROLLBACK_RESISTANCE:
	case TAG_HARDWARE_TYPE:
	case TAG_EARLY_BOOT_ONLY:
	case TAG_MAX_USES_PER_BOOT:
	case TAG_USAGE_COUNT_LIMIT:
	case TAG_USER_SECURE_ID:
	case TAG_NO_AUTH_REQUIRED:
	case TAG_USER_AUTH_TYPE:
	case TAG_AUTH_TIMEOUT:
	case TAG_TRUSTED_USER_PRESENCE_REQUIRED:
	case TAG_TRUSTED_CONFIRMATION_REQUIRED:
	case TAG_ORIGIN:
	case TAG_OS_VERSION:
	case TAG_OS_PATCHLEVEL:
	case TAG_UNIQUE_ID:
	case TAG_VENDOR_PATCHLEVEL:
	case TAG_BOOT_PATCHLEVEL:
	case TAG_DEVICE_UNIQUE_ATTESTATION:
		// Other tags default to SW enforced unless explicitly HW.
		return true;
	default:
		return false;
	}
}

bool is_sw_enforced(keymint_tag_t tag)
{
	// Any tag not HW-enforced is considered SW-enforced for this
	// translation.
	return !is_hw_enforced(tag);
}

// --- Static Helper Functions ---

static void copy_slice_trunc(const uint32_t *data, size_t data_len_words,
			     uint32_t *out, size_t out_len_words)
{
	size_t copy_size_words = MIN(data_len_words, out_len_words);
	if (copy_size_words > 0) {
		memcpy(out, data, copy_size_words * sizeof(uint32_t));
	}
}

// Corresponds to `set_once` for optional u32
static enum strongbox_error set_once_u32(optional_u32_t *var, uint32_t value)
{
	if (var->is_set) {
		return SBERR_InvalidArgument;
	}
	var->value = value;
	var->is_set = true;
	return SB_OK;
}

// Corresponds to `set_once` for optional u64
static enum strongbox_error set_once_u64(optional_u64_t *var, uint64_t value)
{
	if (var->is_set) {
		return SBERR_InvalidArgument;
	}
	var->value = value;
	var->is_set = true;
	return SB_OK;
}

// Corresponds to `set_once` for an optional slice
static enum strongbox_error set_once_slice(optional_slice_t *var,
					   const uint8_t *data, size_t len)
{
	if (var->is_set) {
		return SBERR_InvalidArgument;
	}
	var->data = data;
	var->len = len;
	var->is_set = true;
	return SB_OK;
}

// Parses a single tag entry from the buffer.
static enum strongbox_error parse_tag_data(const uint32_t *buffer,
					   size_t buffer_len_words,
					   TagData_t *out)
{
	TagTypeEnum type;

	if (buffer_len_words < 1)
		return SBERR_InvalidTag;

	out->tag = (keymint_tag_t)buffer[0];
	out->total_word_len = 1; // For the tag itself

	type = tag_get_type_enum(out->tag);

	switch (type) {
	case TAG_TYPE_ENUM_BOOL:
		// No value
		out->value_ptr = NULL;
		break;

	case TAG_TYPE_ENUM_ENUM:
	case TAG_TYPE_ENUM_ENUM_REP:
	case TAG_TYPE_ENUM_UINT:
	case TAG_TYPE_ENUM_UINT_REP:
		if (buffer_len_words < 2)
			return SBERR_InvalidTag;
		out->value_ptr = &buffer[1];
		out->total_word_len += 1;
		break;

	case TAG_TYPE_ENUM_ULONG:
	case TAG_TYPE_ENUM_ULONG_REP:
	case TAG_TYPE_ENUM_DATE:
		if (buffer_len_words < 3)
			return SBERR_InvalidTag;
		out->value_ptr = &buffer[1];
		out->total_word_len += 2; // Date is 64-bit
		break;

	case TAG_TYPE_ENUM_BYTES:
	case TAG_TYPE_ENUM_BIGNUM: {
		// In a standard format, a BYTES tag is followed by a length
		// word. The rust code seems to use a custom `TagPrefix` which
		// isn't provided. This implementation assumes a simple [tag,
		// len_in_bytes, data...] format. For this translation, we will
		// assume length is in the next word.
		uint32_t byte_len;
		size_t data_words;
		if (buffer_len_words < 2)
			return SBERR_InvalidTag;
		byte_len = buffer[1];

		data_words =
			(byte_len + sizeof(uint32_t) - 1) / sizeof(uint32_t);
		if (buffer_len_words < 2 + data_words)
			return SBERR_InvalidTag;
		out->value_ptr = &buffer[1]; // value_ptr points to length
		out->total_word_len += 1 + data_words;
		break;
	}
	default:
		return SBERR_InvalidTag;
	}
	return SB_OK;
}

static enum strongbox_error process_gen_import_tags(
	Keymint_t *km, KeyAttributes_t *attrs, const uint32_t *buf,
	size_t req_len_words, uint32_t *out, size_t out_len_words,
	size_t *out_written_words)
{
	int32_t algorithm;
	size_t tags_len_words;
	size_t hw_tag_start;
	size_t sw_enforced_tag_size_words;
	size_t tag_pos;
	size_t sw_out_start;
	size_t sw_tag_write_pos;

	if (req_len_words < 1 || out_len_words < 14) {
		return SBERR_InvalidTag;
	}

	algorithm = -1; // Not set
	tags_len_words = buf[0];
	if (tags_len_words > req_len_words - 1) {
		return SBERR_InvalidTag;
	}

	// These tags go into TEE/Strongbox KeyCharacteristics
	out[0] = SECURITY_LEVEL_STRONGBOX;
	// out[1] is length, filled later
	out[2] = TAG_ORIGIN;
	// out[3] is KeyOrigin, filled by caller
	out[4] = TAG_OS_VERSION;
	out[5] = km->os_version;
	out[6] = TAG_OS_PATCHLEVEL;
	out[7] = km->os_patchlevel;
	out[8] = TAG_VENDOR_PATCHLEVEL;
	out[9] = km->vendor_patchlevel;
	out[10] = TAG_BOOT_PATCHLEVEL;
	out[11] = km->boot_patchlevel;

	hw_tag_start = 12;
	sw_enforced_tag_size_words = 0;
	tag_pos = 1; // start after length word

	// Pass 1: Parse tags, copy HW tags, count SW tags, extract attributes
	while (tag_pos < tags_len_words + 1) {
		TagData_t tag_data;
		enum strongbox_error err = parse_tag_data(
			&buf[tag_pos], tags_len_words + 1 - tag_pos, &tag_data);
		if (err != SB_OK)
			return err;

		if (is_gen_disallowed(tag_data.tag)) {
			return SBERR_InvalidTag;
		}

		switch (tag_data.tag) {
		case TAG_ROLLBACK_RESISTANCE:
			return SBERR_RollbackResistanceUnavailable;
		case TAG_ALGORITHM:
			if (algorithm != -1)
				return SBERR_InvalidTag;
			algorithm = (int32_t)tag_data.value_ptr[0];
			break;
		case TAG_KEY_SIZE:
			err = set_once_u32(&attrs->key_size,
					   tag_data.value_ptr[0]);
			if (err != SB_OK)
				return err;
			break;
		case TAG_EC_CURVE:
			err = set_once_u32(&attrs->curve_id,
					   tag_data.value_ptr[0]);
			if (err != SB_OK)
				return err;
			break;
		case TAG_RSA_PUBLIC_EXPONENT: {
			uint64_t exponent;
			memcpy(&exponent, tag_data.value_ptr, sizeof(uint64_t));
			err = set_once_u64(&attrs->rsa_exponent, exponent);
			if (err != SB_OK)
				return err;
			break;
		}
		case TAG_CALLER_NONCE:
			if (attrs->caller_nonce)
				return SBERR_InvalidArgument;
			attrs->caller_nonce = true;
			break;
		case TAG_PURPOSE: {
			uint32_t p;
			p = tag_data.value_ptr[0];
			if (p < 32)
				attrs->purpose_flags |= (1 << p);
			else
				return SBERR_InvalidArgument;
			break;
		}
		case TAG_APPLICATION_ID:
		case TAG_APPLICATION_DATA: {
			uint32_t len_bytes = tag_data.value_ptr[0];
			const uint8_t *data =
				(const uint8_t *)&tag_data.value_ptr[1];
			if (tag_data.tag == TAG_APPLICATION_ID) {
				err = set_once_slice(&attrs->application_id,
						     data, len_bytes);
			} else {
				err = set_once_slice(&attrs->application_data,
						     data, len_bytes);
			}
			if (err != SB_OK)
				return err;
			break;
		}
		default:
			break;
		}

		if (is_sw_enforced(tag_data.tag)) {
			sw_enforced_tag_size_words += tag_data.total_word_len;
		} else if (is_hw_enforced(tag_data.tag)) {
			if (hw_tag_start + tag_data.total_word_len >
			    out_len_words) {
				return SBERR_InvalidArgument; // Not enough
							      // space
			}
			copy_slice_trunc(&buf[tag_pos], tag_data.total_word_len,
					 &out[hw_tag_start],
					 out_len_words - hw_tag_start);
			hw_tag_start += tag_data.total_word_len;
		}
		tag_pos += tag_data.total_word_len;
	}

	if ((attrs->purpose_flags & (1 << KEY_PURPOSE_ATTEST_KEY)) != 0 &&
	    (attrs->purpose_flags != (1 << KEY_PURPOSE_ATTEST_KEY))) {
		return SBERR_IncompatiblePurpose;
	}

	if (algorithm == -1)
		return SBERR_InvalidArgument;
	attrs->algorithm = algorithm;

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
		TagData_t tag_data;
		enum strongbox_error err;
		err = parse_tag_data(&buf[tag_pos],
				     tags_len_words + 1 - tag_pos, &tag_data);
		if (err != SB_OK)
			return err;

		if (is_sw_enforced(tag_data.tag)) {
			copy_slice_trunc(&buf[tag_pos], tag_data.total_word_len,
					 &out[sw_out_start + sw_tag_write_pos],
					 out_len_words - (sw_out_start +
							  sw_tag_write_pos));
			sw_tag_write_pos += tag_data.total_word_len;
		}
		tag_pos += tag_data.total_word_len;
	}

	*out_written_words = sw_out_start + sw_enforced_tag_size_words;
	return SB_OK;
}

/* Derive key encryption key and tag key. */
enum dcrypto_result cryptokey_derive_wrapping(
	Keymint_t *km, void *tag_data_ptr, size_t tag_size_bytes,
	const uint8_t *application_id_data, size_t application_id_len,
	const uint8_t *application_data_data, size_t application_data_len,
	void *hmac_key, size_t hmac_key_len, void *aes_key, size_t aes_key_len)
{
	struct drbg_ctx drbg;
	enum dcrypto_result result = DCRYPTO_FAIL;
	hmac_drbg_init(&drbg, km->drbg_seed_key, sizeof(km->drbg_seed_key),
		       tag_data_ptr, tag_size_bytes, application_id_data,
		       application_id_len, 3);

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

enum dcrypto_result cryptokey_generate(Keymint_t *km, Algorithm alg, void *key,
				       size_t key_len)
{
	enum dcrypto_result result;

	(void)km;

	do {
		result = fips_rand_bytes(key, key_len);
		if (result != DCRYPTO_OK)
			return result;
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
	Keymint_t *km, Algorithm alg, void *blob_data_ptr,
	size_t *blob_size_bytes, uint8_t *out_buf, size_t blob_start_bytes,
	const uint8_t *application_id_data, size_t application_id_len,
	const uint8_t *application_data_data, size_t application_data_len)
{
	enum dcrypto_result result = DCRYPTO_FAIL;
	uint32_t aes_key[8], hmac_key[8], key[8];
	result = cryptokey_derive_wrapping(
		km, blob_data_ptr, *blob_size_bytes, application_id_data,
		application_id_len, application_data_data, application_data_len,
		aes_key, sizeof(aes_key), hmac_key, sizeof(hmac_key));
	if (result != DCRYPTO_OK)
		goto clean;
	/* TODO: add handling of different key sizes if needed. */
	result = cryptokey_generate(km, alg, key, sizeof(key));
	if (result != DCRYPTO_OK)
		goto clean;
	result = DCRYPTO_aes_init(aes_key, sizeof(aes_key), const uint8_t *iv, enum cipher_mode c_mode, enum encrypt_mode e_mode)
	/* Encrypt and wrap key */
clean:
	always_memset(aes_key, 0xAA, sizeof(aes_key));
	always_memset(hmac_key, 0xAA, sizeof(hmac_key));
	return result;
}

// --- Public Function Implementation ---

enum strongbox_error generate_key(Keymint_t *km, uint32_t *buf,
				  size_t buf_size_words, size_t req_len_words,
				  size_t *out_len_bytes)
{
	KeyAttributes_t attrs = { 0 };

	// Static buffer on the stack, similar to the Rust version.
	uint32_t out_buf[512];
	size_t blob_start_words = 0;
	uint32_t *blob_size_field;
	uint8_t *blob_data_ptr;
	size_t blob_max_len_bytes;
	size_t blob_size_bytes;
	size_t total_words;

	enum strongbox_error err;

	attrs.algorithm = -1;

	err = process_gen_import_tags(km, &attrs, buf, req_len_words, out_buf,
				      512, &blob_start_words);
	if (err != SB_OK)
		return err;

	if (blob_start_words >= 512)
		return SBERR_UnknownError;

	out_buf[3] = KEY_ORIGIN_GENERATED;

	blob_size_field = &out_buf[blob_start_words];

	blob_data_ptr = (uint8_t *)&out_buf[blob_start_words + 1];

	blob_max_len_bytes = (512 - blob_start_words - 1) * sizeof(uint32_t);

	blob_size_bytes = 0;

	if (attrs.algorithm == ALGORITHM_RSA) {
		// Mocked TI50 RSA key generation
		if (!attrs.key_size.is_set || !attrs.rsa_exponent.is_set)
			return SBERR_InvalidArgument;
		if (attrs.key_size.value != 1024 &&
		    attrs.key_size.value != 2048)
			return SBERR_UnsupportedKeySize;
		if (attrs.rsa_exponent.value != 3 &&
		    attrs.rsa_exponent.value != 65537)
			return SBERR_InvalidArgument;

		// RsaOperation_t rsa = rsa_new();
		// // ... full initialization sequence from rust code...

		// blob_size_bytes = blob_max_len_bytes;
		// rsa_export_bound(&rsa, blob_data_ptr, &blob_size_bytes,
		//                  (uint8_t*)out_buf, blob_start_words *
		//                  sizeof(uint32_t), attrs.application_id.data,
		//                  attrs.application_id.len,
		//                  attrs.application_data.data,
		//                  attrs.application_data.len);
		return SBERR_Unimplemented;
	} else {
		// Mocked TI50 Symmetric/EC key generation
		if (attrs.algorithm == ALGORITHM_AES) {
			if (!attrs.key_size.is_set ||
			    (attrs.key_size.value != 128 &&
			     attrs.key_size.value != 192 &&
			     attrs.key_size.value != 256)) {
				return SBERR_UnsupportedKeySize;
			}
		}
		if (attrs.algorithm != ALGORITHM_EC)
			return SBERR_Unimplemented;

		// ... more validation for other algs
		// CryptoKey_t key = key_new();
		blob_size_bytes = blob_max_len_bytes;
		cryptokey_export_bound(km, attrs.algorithm, blob_data_ptr,
				       &blob_size_bytes, (uint8_t *)out_buf,
				       blob_start_words * sizeof(uint32_t),
				       attrs.application_id.data,
				       attrs.application_id.len,
				       attrs.application_data.data,
				       attrs.application_data.len);
	}

	*blob_size_field = (uint32_t)blob_size_bytes;
	total_words = blob_start_words + 1 + (blob_size_bytes + 3) / 4;

	if (total_words > buf_size_words)
		return SBERR_UnknownError; // Not enough space in original
					   // buffer

	memcpy(buf, out_buf, total_words * sizeof(uint32_t));
	*out_len_bytes = total_words * sizeof(uint32_t);

	return SB_OK;
}

Keymint_t km;

enum strongbox_error sb_GenerateKey(struct vendor_cmd_params *p)
{
	return generate_key(&km, p->buffer, p->out_size / sizeof(uint32_t),
			    p->in_size / sizeof(uint32_t), &p->out_size);
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceGenerateKey, sb_GenerateKey);