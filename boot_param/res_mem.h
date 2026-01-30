/*
 * Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __GSC_UTILS_BOOT_PARAM_RES_MEM_H
#define __GSC_UTILS_BOOT_PARAM_RES_MEM_H

#include "boot_param_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GSC_EARLY_ENTROPY_SIZE	  64
#define GSC_SESSION_KEY_SEED_SIZE 32
#define GSC_AUTH_TOKEN_SIZE	  32
#define GSC_VERSIONED_SEED_SIZE	  32

/* The value of com.android.virt.name property in Trusty image's AVB vbmeta */
static const char desktop_trusty_name_str[] = "desktop-trusty";
/* Reserved memory blobs VM DTB compatible strings */
static const char early_entropy_compat_str[] = "google,early-entropy";
static const char session_key_seed_compat_str[] = "google,session-key-seed";
static const char auth_token_key_seed_compat_str[] =
	"google,auth-token-key-seed";
static const char versioned_seed_compat_str[] = "google,versioned-seed";

/* All values in the structure below are little-endian. */
struct res_mem_hdr_s {
	uint8_t vm_name_offset[4];
	uint8_t blob_offset[4];
	uint8_t blob_size[4];
	uint8_t compat_offset[4];
	uint8_t flags[4];
};

struct res_mem_s {
	struct res_mem_hdrs_s {
		/* The number of reserved blobs in the entry, little-endian */
		uint8_t count[4]; /* equal to 4 */

		/*
		 * The headers for each reserved blob in the entry, all store
		 * the offset and size of blob in the entry.
		 */
		struct res_mem_hdr_s early_entropy;
		struct res_mem_hdr_s session_key_seed;
		struct res_mem_hdr_s auth_token_key_seed;
		struct res_mem_hdr_s versioned_seed;
	} hdrs;

	/*
	 * The payload of reserved blobs entry. The offsets and sizes of
	 * the fields are stored in pvmfw_cfg_rmem_hdr headers above.
	 */
	struct res_mem_blobs_s {
		uint8_t early_entropy[EARLY_ENTROPY_BYTES];
		uint8_t session_key_seed[KEY_SEED_BYTES];
		uint8_t auth_token_key_seed[KEY_SEED_BYTES];
		struct versioned_seed_s {
			uint8_t seed[KEY_SEED_BYTES];
			uint8_t version[4];
		} versioned_seed;
	} blobs;

	/*
	 * It is required that string table is at the end of reserved
	 * blobs entry.
	 */
	struct res_mem_strings_s {
		uint8_t desktop_trusty_name[sizeof(desktop_trusty_name_str)];
		uint8_t early_entropy_compat[sizeof(early_entropy_compat_str)];
		uint8_t session_key_seed_compat[sizeof(
			session_key_seed_compat_str)];
		uint8_t auth_token_key_seed_compat[sizeof(
			auth_token_key_seed_compat_str)];
		uint8_t versioned_seed_compat[sizeof(
			versioned_seed_compat_str)];
	} strings;
};

#define LE32_VALUE(value)                                \
	{ (uint8_t)((value) & 0x000000FF),               \
		(uint8_t)(((value) & 0x0000FF00) >> 8),  \
		(uint8_t)(((value) & 0x00FF0000) >> 16), \
		(uint8_t)(((value) & 0xFF000000) >> 24) }

#define STRING_OFFSET(name)                         \
	LE32_VALUE(sizeof(struct res_mem_blobs_s) + \
		   offsetof(struct res_mem_strings_s, name))

#define BLOB_OFFSET(name) LE32_VALUE(offsetof(struct res_mem_blobs_s, name))

#define LE32_FLAGS LE32_VALUE(1)

#define RES_MEM_HDR_WITH_TYPE(blob_name, blob_type, compat_name)           \
	{ STRING_OFFSET(desktop_trusty_name), BLOB_OFFSET(blob_name),      \
		LE32_VALUE(sizeof(blob_type)), STRING_OFFSET(compat_name), \
		LE32_FLAGS }

#define RES_MEM_HDR_WITH_SIZE(blob_name, blob_size, compat_name)      \
	{ STRING_OFFSET(desktop_trusty_name), BLOB_OFFSET(blob_name), \
		LE32_VALUE(blob_size), STRING_OFFSET(compat_name),    \
		LE32_FLAGS }

static const struct res_mem_hdrs_s res_mem_hdrs = { LE32_VALUE(4), /* count */
	RES_MEM_HDR_WITH_SIZE(
		early_entropy, EARLY_ENTROPY_BYTES, early_entropy_compat),
	RES_MEM_HDR_WITH_SIZE(
		session_key_seed, KEY_SEED_BYTES, session_key_seed_compat),
	RES_MEM_HDR_WITH_SIZE(auth_token_key_seed, KEY_SEED_BYTES,
		auth_token_key_seed_compat),
	RES_MEM_HDR_WITH_TYPE(versioned_seed, struct versioned_seed_s,
		versioned_seed_compat) };

#define set_res_mem_string(res_mem, field) \
	__platform_memcpy(                 \
		&res_mem->strings.field, field##_str, sizeof(field##_str))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __GSC_UTILS_BOOT_PARAM_RES_MEM_H */
