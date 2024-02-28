/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM NVMEM definitions.
 */
#ifndef __CROS_EC_TPM_NVMEM_H
#define __CROS_EC_TPM_NVMEM_H

/*
 * These NV space definitions were manually copied from
 * src/third_party/coreboot/src/security/vboot/antirollback.h
 * at git hash a03ebe7fc5.
 */
#define FIRMWARE_NV_INDEX       0x1007
#define KERNEL_NV_INDEX         0x1008
#define FWMP_NV_INDEX           0x100a
/* 0x100b: Hash of MRC_CACHE training data for recovery boot */
#define MRC_REC_HASH_NV_INDEX           0x100b
/* 0x100c: OOBE autoconfig public key hashes */
/* 0x100d: Hash of MRC_CACHE training data for non-recovery boot */
#define MRC_RW_HASH_NV_INDEX            0x100d
#define ENT_ROLLBACK_SPACE_INDEX        0x100e
#define VBIOS_CACHE_NV_INDEX            0x100f
/* Widevine Secure Counter space */
#define WIDEVINE_COUNTER_NV_INDEX(n)	(0x3000 + (n))
#define NUM_WIDEVINE_COUNTERS		4


enum tpm_nv_hidden_object {
	TPM_HIDDEN_U2F_KEK,
	TPM_HIDDEN_U2F_KH_SALT,
};


/*
 * These definitions and the structure layout were manually copied from
 * src/platform/vboot_reference/firmware/2lib/include/2secdata.h. at
 * git sha 38d7d1c.
 */
#define FWMP_HASH_SIZE		    32
#define FWMP_DEV_DISABLE_CCD_UNLOCK BIT(6)
#define FWMP_DEV_DISABLE_BOOT       BIT(0)
#define FIRMWARE_FLAG_DEV_MODE      0x02

struct RollbackSpaceFirmware {
	/* Struct version, for backwards compatibility */
	uint8_t struct_version;
	/* Flags (see FIRMWARE_FLAG_* above) */
	uint8_t flags;
	/* Firmware versions */
	uint32_t fw_versions;
	/* Reserved for future expansion */
	uint8_t reserved[3];
	/* Checksum (v2 and later only) */
	uint8_t crc8;
} __packed;

/* Firmware management parameters */
struct RollbackSpaceFwmp {
	/* CRC-8 of fields following struct_size */
	uint8_t crc;
	/* Structure size in bytes */
	uint8_t struct_size;
	/* Structure version */
	uint8_t struct_version;
	/* Reserved; ignored by current reader */
	uint8_t reserved0;
	/* Flags; see enum fwmp_flags */
	uint32_t flags;
	/* Hash of developer kernel key */
	uint8_t dev_key_hash[FWMP_HASH_SIZE];
} __packed;


#endif /* __CROS_EC_TPM_NVMEM_H */
