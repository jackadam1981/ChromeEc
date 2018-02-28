/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Compatibility layer between the TPM code and PinWeaver.
 *
 * This is needed because the headers for the TPM are not compatible with the
 * headers used by pinweaver.c. It also makes it easier to mock the
 * functionality derived from the TPM code.
 */

#ifndef __CROS_EC_PINWEAVER_TPM_IMPORTS_H
#define __CROS_EC_PINWEAVER_TPM_IMPORTS_H

#include <pinweaver.h>
#include <stdint.h>

uint32_t get_restart_count(void);

#define PW_STORAGE_VERSION 0

#define PW_LOG_ENTRY_COUNT 5

/* Long term flash storage for tree metadata. */
struct PW_PACKED pw_long_term_storage_t {
	uint16_t storage_version;

	/* log2(Fan out). */
	struct bits_per_level_t bits_per_level;
	/* Height of the tree or param_l / bits_per_level. */
	struct height_t height;

	/* Key used to compute the HMACs of the metadata of the leaves. */
	uint8_t hmac_key[32];

	/* Key used to encrypt and decrypt the metadata of the leaves. */
	uint8_t wrap_key[32];
};

struct PW_PACKED pw_log_storage_t {
	uint16_t storage_version;

	struct pw_get_log_entry_t entries[PW_LOG_ENTRY_COUNT];
};

void pinweaver_storage_init(void);
int load_merkle_tree(struct merkle_tree_t *merkle_tree);
int store_merkle_tree(const struct merkle_tree_t *merkle_tree);
int load_log_data(struct pw_log_storage_t *log);
int store_log_data(const struct pw_log_storage_t *log);

#endif  /* __CROS_EC_PINWEAVER_TPM_IMPORTS_H */
