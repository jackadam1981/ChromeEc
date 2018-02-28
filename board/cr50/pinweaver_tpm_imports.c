/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pinweaver_tpm_imports.h>

/* Cr50 imports. */
#include <compile_time_macros.h>

/* TPM2 imports. */
#include <Global.h>
#include <NV_fp.h>
#include <TpmError.h>
#include <tpm_types.h>

/* TPM handle related defines.
 * It might make sense to move these to
 * src/platform/third_party/tpm2/tpm_types.h
 */
#define TPM_HT_VENDOR_SPECIFIC 0x08
#define TPM_VS_PINWEAVER_TREE 0x48000001
#define TPM_VS_PINWEAVER_LOG 0x48000002

/* Declarations from util.h which is not compatible with the TPM headers. */
void *memcpy(void *dest, const void *src, size_t len);

/* struct pw_log_storage_t depends on struct pw_get_log_entry_t so any changes
 * need to result in a version change.
 */
BUILD_ASSERT(PW_STORAGE_VERSION == PW_PROTOCOL_VERSION);

uint32_t get_restart_count(void)
{
	return gp.resetCount;
}

static void init_tree_nvindex(NV_INDEX *nvIndex)
{
	TPMS_NV_PUBLIC *pa = &nvIndex->publicArea;

	pa->nvIndex = TPM_VS_PINWEAVER_TREE;
	pa->dataSize = sizeof(struct pw_long_term_storage_t);
	pa->attributes.TPMA_NV_PPREAD = SET;
	pa->attributes.TPMA_NV_PPWRITE = SET;
}

static void init_log_nvindex(NV_INDEX *nvIndex)
{
	TPMS_NV_PUBLIC *pa = &nvIndex->publicArea;

	pa->nvIndex = TPM_VS_PINWEAVER_LOG;
	pa->dataSize = sizeof(struct pw_log_storage_t);
	pa->attributes.TPMA_NV_PPREAD = SET;
	pa->attributes.TPMA_NV_PPWRITE = SET;
}

TPM_RC pw_init_status;
void pinweaver_storage_init(void)
{
	NV_INDEX nvIndex = {};

	init_tree_nvindex(&nvIndex);

	pw_init_status = NvDefineIndex(&nvIndex.publicArea, &nvIndex.authValue);
	if (pw_init_status != TPM_RC_SUCCESS &&
			pw_init_status != TPM_RC_NV_DEFINED)
		return;

	/* If return code is TPM_RC_NV_DEFINED, and the storage format changes
	 * any code required to update old storage format for
	 * pw_long_term_storage_t needs to be included here.
	 */
	pAssert(PW_STORAGE_VERSION == 0);

	init_log_nvindex(&nvIndex);

	pw_init_status = NvDefineIndex(&nvIndex.publicArea, &nvIndex.authValue);
	if (pw_init_status != TPM_RC_SUCCESS &&
	    pw_init_status != TPM_RC_NV_DEFINED)
		return;

	/* If return code is TPM_RC_NV_DEFINED, and the storage format changes
	 * any code required to update old storage format for pw_log_storage_t
	 * needs to be included here.
	 */

	/* Handle the TPM_RC_NV_DEFINED case. */
	pw_init_status = TPM_RC_SUCCESS;
}

int load_merkle_tree(struct merkle_tree_t *merkle_tree)
{
	TPM_RC tpm_ret;

	/* Handle the immutable data. */
	{
		NV_INDEX nvIndex;
		struct pw_long_term_storage_t data;

		if (pw_init_status != TPM_RC_SUCCESS)
			return PW_ERR_TPM_INIT_FAILED;

		tpm_ret = NvIsAvailable();
		if (tpm_ret != TPM_RC_SUCCESS)
			return PW_ERR_TPM_NV_UNAVAILABLE;

		/* TODO(allenwebb) modify NvGetIndexInfo not to fail for
		 * HandleGetType(TPM_HT_VENDOR_SPECIFIC).
		 */
		NvGetIndexInfo(TPM_VS_PINWEAVER_TREE, &nvIndex);

		NvGetIndexData(TPM_VS_PINWEAVER_TREE, &nvIndex,
			       0 /* offset */, sizeof(data) /* size */,
			       &data /* buffer */);

		merkle_tree->bits_per_level = data.bits_per_level;
		merkle_tree->height = data.height;
		memcpy(merkle_tree->hmac_key, data.hmac_key,
		       sizeof(data.hmac_key));
		memcpy(merkle_tree->wrap_key, data.wrap_key,
		       sizeof(data.wrap_key));
	}

	/* Handle the root hash. */
	{
		struct pw_log_storage_t log;
		int ret;

		ret = load_log_data(&log);
		if (ret != EC_SUCCESS)
			return ret;

		memcpy(merkle_tree->root, log.entries[0].root,
		       sizeof(merkle_tree->root));
	}

	return EC_SUCCESS;
}

/* This should only be called when a new tree is created. */
int store_merkle_tree(const struct merkle_tree_t *merkle_tree)
{
	TPM_RC tpm_ret;

	/* Handle the immutable data. */
	{
		NV_INDEX nvIndex = {};
		struct pw_long_term_storage_t data;

		if (pw_init_status != TPM_RC_SUCCESS)
			return PW_ERR_TPM_INIT_FAILED;

		tpm_ret = NvIsAvailable();
		if (tpm_ret != TPM_RC_SUCCESS)
			return PW_ERR_TPM_NV_UNAVAILABLE;

		data.storage_version = PW_STORAGE_VERSION;
		data.bits_per_level = merkle_tree->bits_per_level;
		data.height = merkle_tree->height;
		memcpy(data.hmac_key, merkle_tree->hmac_key,
		       sizeof(data.hmac_key));
		memcpy(data.wrap_key, merkle_tree->wrap_key,
		       sizeof(data.wrap_key));

		NvGetIndexInfo(TPM_VS_PINWEAVER_TREE, &nvIndex);

		tpm_ret = NvWriteIndexData(TPM_VS_PINWEAVER_TREE, &nvIndex,
					   0 /* offset */,
					   sizeof(data) /* size */,
					   &data /* buffer */);
		if (tpm_ret != TPM_RC_SUCCESS)
			return PW_ERR_TPM_NV_UNAVAILABLE;
	}

	/* Handle the root hash. */
	{
		struct pw_log_storage_t log = {};
		struct pw_get_log_entry_t *entry = log.entries;

		log.storage_version = PW_STORAGE_VERSION;
		entry->type.v = PW_MTQ_RESET_TREE;
		memcpy(entry->root, merkle_tree->root,
		       sizeof(merkle_tree->root));

		return store_log_data(&log);
	}
}

int load_log_data(struct pw_log_storage_t *log)
{
	TPM_RC tpm_ret;
	NV_INDEX nvIndex = {};

	if (pw_init_status != TPM_RC_SUCCESS)
		return PW_ERR_TPM_INIT_FAILED;

	tpm_ret = NvIsAvailable();
	if (tpm_ret != TPM_RC_SUCCESS)
		return PW_ERR_TPM_NV_UNAVAILABLE;

	NvGetIndexInfo(TPM_VS_PINWEAVER_LOG, &nvIndex);

	NvGetIndexData(TPM_VS_PINWEAVER_TREE, &nvIndex,
		       0 /* offset */, sizeof(*log) /* size */,
		       log /* buffer */);

	return EC_ERROR_UNIMPLEMENTED;
}

int store_log_data(const struct pw_log_storage_t *log)
{
	TPM_RC tpm_ret;
	NV_INDEX nvIndex = {};

	if (pw_init_status != TPM_RC_SUCCESS)
		return PW_ERR_TPM_INIT_FAILED;

	tpm_ret = NvIsAvailable();
	if (tpm_ret != TPM_RC_SUCCESS)
		return PW_ERR_TPM_NV_UNAVAILABLE;

	NvGetIndexInfo(TPM_VS_PINWEAVER_LOG, &nvIndex);

	tpm_ret = NvWriteIndexData(TPM_VS_PINWEAVER_TREE, &nvIndex,
				   0 /* offset */, sizeof(*log) /* size */,
				   (void *)log /* buffer */);
	if (tpm_ret == TPM_RC_SUCCESS)
		return EC_SUCCESS;
	else
		return PW_ERR_TPM_NV_UNAVAILABLE;
}
