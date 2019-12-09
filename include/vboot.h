/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_INCLUDE_VBOOT_H
#define __CROS_EC_INCLUDE_VBOOT_H

#include "common.h"
#include "vb21_struct.h"
#include "rsa.h"

#define CR50_COMM_PREFIX                0xec
#define MIN_LENGTH_PREFIX               4
#define CR50_COMM_MAGIC_CHAR0           'E'
#define CR50_COMM_MAGIC_CHAR1           'C'
#define CR50_COMM_MAGIC_WORD            ((CR50_COMM_MAGIC_CHAR1 << 8) | \
					 CR50_COMM_MAGIC_CHAR0)

/**
 * EC-Cr50 data stream looks like as follows:
 *
 *   [preamble]|[header][payload]
 *
 * preamble: 0xec ...
 * header: struct cr50_comm_packet
 * payload: data[]
 */
struct cr50_comm_packet {
	/* Header */
	uint16_t magic;	/* CR50_COMM_MAGIC_WORD */
	uint8_t crc;	/* checksum computed from all bytes after crc */
	uint16_t cmd;	/* CR50_COMM_CMD_* (or control packet with no data) */
	uint8_t size;	/* Payload size. Max 32 bytes. */
	uint8_t data[];	/* Payload */
} __packed;

#define CR50_COMM_MAX_DATA_SIZE         32
#define CR50_COMM_MAX_PACKET_SIZE       (sizeof(struct cr50_comm_packet) + \
					 CR50_COMM_MAX_DATA_SIZE)

/* commands */
#define CR50_COMM_CMD_SET_BOOT_MODE     0x01
#define CR50_COMM_CMD_VERIFY_HASH       0x02

/* return code */
#define CR50_COMM_SUCCESS               0xec
#define CR50_COMM_ERROR_UNKNOWN         0xe0
#define CR50_COMM_ERROR_MAGIC           0xe1
#define CR50_COMM_ERROR_CRC             0xe2
#define CR50_COMM_ERROR_ROLLBACK        0xe3
#define CR50_COMM_ERROR_TIMEOUT         0xe4
#define CR50_COMM_ERROR_UNSUPPORTED     0xe5
#define CR50_COMM_ERROR_SIZE            0xe6
#define CR50_COMM_HASH_MISMATCH         0xe7

enum ec_efs_boot_mode {
	EC_EFS_BOOT_MODE_RESET            = 0xff,
	EC_EFS_BOOT_MODE_NORMAL           = 0x00,
	EC_EFS_BOOT_MODE_RECOVERY         = 0x01,
	EC_EFS_BOOT_MODE_NO_BOOT          = 0x02,
	EC_EFS_BOOT_MODE_NO_BOOT_RECOVERY = 0x03,

	/* boot_mode is uint8_t */
	EC_EFS_BOOT_MODE_LIMIT            = 255,
};

/****************************************************************************
 * This is quoted from 2secdata_struct.h in the directory,
 * src/platform/vboot_reference/firmware/2lib/include/.
 ****************************************************************************/

/* Kernel secure storage space */
#define VB2_SHA256_DIGEST_SIZE          32
#define VB2_SECDATA_KERNEL_VERSION_MIN  3
#define VB2_SECDATA_KERNEL_UID          0x4752574c  /* 'LWRG' */
struct vb2_secdata_kernel {
	/* Struct version, for backwards compatibility */
	uint8_t struct_version;
	/* Unique ID to detect space redefinition */
	uint32_t uid;
	/* Kernel versions */
	uint32_t kernel_versions;
	/* New field for struct_version >= 3 */
	uint8_t ec_hash[VB2_SHA256_DIGEST_SIZE];
	/* Reserved for future expansion */
	uint8_t reserved[3];
	/* CRC; must be last field in struct */
	uint8_t crc8;
} __packed;

/**
 * Validate key contents.
 *
 * @param key
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vb21_is_packed_key_valid(const struct vb21_packed_key *key);

/**
 * Validate signature contents.
 *
 * @param sig Signature to be validated.
 * @param key Key to be used for validating <sig>.
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vb21_is_signature_valid(const struct vb21_signature *sig,
			    const struct vb21_packed_key *key);

/**
 * Check data region is filled with ones
 *
 * @param data  Data to be validated.
 * @param start Offset where validation starts.
 * @param end   Offset where validation ends. data[end] won't be checked.
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vboot_is_padding_valid(const uint8_t *data, uint32_t start, uint32_t end);

/**
 * Verify data by RSA signature
 *
 * @param data Data to be verified.
 * @param len  Number of bytes in <data>.
 * @param key  Key to be used for verification.
 * @param sig  Signature of <data>
 * @return EC_SUCCESS or EC_ERROR_*
 */
int vboot_verify(const uint8_t *data, int len,
		 const struct rsa_public_key *key, const uint8_t *sig);

/**
 * Entry point of EC EFS
 */
void vboot_main(void);

/**
 * Get if vboot requires PD comm to be enabled or not
 *
 * @return 1: need PD communication. 0: PD communication is not needed.
 */
int vboot_need_pd_comm(void);

#endif  /* __CROS_EC_INCLUDE_VBOOT_H */
