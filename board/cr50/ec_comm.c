/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file implements functions for EC-EFS2 feature including
 * EC-CR50 communication and AP Vendor command support.
 * For more information, visit http://go/ec-efs2 and http://go/ec-cr50-comm.
 */
#include "common.h"
#include "console.h"
#include "crc8.h"
#include "hooks.h"
#include "registers.h"
#include "timer.h"
#include "tpm_nvmem.h"
#include "tpm_nvmem_ops.h"
#include "vboot.h"

#ifdef CR50_RELAXED
#define CPRINTS(format, args...) cprints(CC_TASK, format, ## args)
#else
#define CPRINTS(format, args...)
#endif

static struct ec_comm_info_ {
	uint32_t ec_hash_is_loaded:1;	/* Is EC hash loaded from nvmem? */
	uint32_t reserved:31;
	uint32_t ec_hash_error_code;

	uint8_t ec_hash[VB2_SHA256_DIGEST_SIZE];
} comm_info;


void ec_comm_init(void)
{
	struct vb2_secdata_kernel sec;
	uint8_t crc;

	if (!board_ec_cr50_comm_support())
		return;

	/* Read an EC hash in kernel secdata (TPM kernel NV index). */

	if (comm_info.ec_hash_is_loaded)
		return;

	if (read_tpm_nvmem(KERNEL_NV_INDEX, sizeof(sec), &sec) !=
		tpm_read_success) {
		CPRINTS("secdata_kernel: read error");
		comm_info.ec_hash_error_code = EC_ERROR_VBOOT_DATA;
		return;
	}

	/*
	 * Check Kernel Version. CRC offset may be different with old struct
	 * version
	 */
	if (sec.kernel_versions < VB2_SECDATA_KERNEL_VERSION_MIN) {
		CPRINTS("secdata_kernel: version incompatible");
		comm_info.ec_hash_error_code = EC_ERROR_VBOOT_DATA_INCOMPATIBLE;
		return;
	}

	/* Verify UID */
	if (sec.uid != VB2_SECDATA_KERNEL_UID) {
		CPRINTS("secdata_kernel: bad UID");
		comm_info.ec_hash_error_code = EC_ERROR_VBOOT_DATA_VERIFY;
		return;
	}

	/* Check CRC */
	crc = crc8((uint8_t *)&sec, offsetof(struct vb2_secdata_kernel, crc8));
	if (crc != sec.crc8) {
		CPRINTS("secdata_kernel: bad CRC");
		comm_info.ec_hash_error_code = EC_ERROR_CRC;
		return;
	}

	/* Read hash and copy to ec_hash */
	memcpy(comm_info.ec_hash, sec.ec_hash, sizeof(sec.ec_hash));
	comm_info.ec_hash_error_code = EC_SUCCESS;
}

#ifdef CR50_RELAXED
/*
 * console command, printing EC-CR50-Comm status.
 */
static int command_ec_comm(int argc, char **argv)
{
	/* Execute command */
	ccprintf("ec_hash_is_loaded  : %d\n", comm_info.ec_hash_is_loaded);
	ccprintf("ec_hash_error_code : 0x%08x\n", comm_info.ec_hash_error_code);

	ccprintf("hash_nvm           : %ph\n",
		 HEX_BUF(comm_info.ec_hash, VB2_SHA256_DIGEST_SIZE));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_comm, command_ec_comm, NULL,
			"Dump EC-CR50-comm info");
#endif  /* CR50_RELAXED */
