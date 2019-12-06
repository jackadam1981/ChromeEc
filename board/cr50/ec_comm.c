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
#include "extension.h"
#include "hooks.h"
#include "registers.h"
#include "timer.h"
#include "tpm_nvmem.h"
#include "tpm_nvmem_ops.h"
#include "tpm_vendor_cmds.h"
#include "vboot.h"

#ifdef CR50_RELAXED
#define CPRINTS(format, args...) cprints(CC_TASK, "EC-COMM: " format, ## args)
#else
#define CPRINTS(format, args...)
#endif

/**
 * Context of EC-EFS
 */
static struct ec_efs_context_ {
	uint32_t hash_is_loaded:1;		/* Is EC hash loaded       */
						/* from kernel secdata?    */
	uint32_t reserved:31;
	uint32_t secdata_error_code;

	enum ec_efs_boot_mode boot_mode;
	uint8_t hash[VB2_SHA256_DIGEST_SIZE];	/* EC-RW digest */
} ec_efs_ctx;

/**
 * Set AP to the off state. Disable functionality that should only be available
 * when the AP is on.
 */
static void deferred_reset_ec(void)
{
	CPRINTS("reset EC");

	board_reboot_ec();
}
DECLARE_DEFERRED(deferred_reset_ec);

/**
 * Initialize EC-EFS context.
 */
static void init_ec_efs_(void)
{
	struct vb2_secdata_kernel sec;
	uint8_t crc;

	/* Read an EC hash in kernel secdata (TPM kernel NV index). */
	if (ec_efs_ctx.hash_is_loaded)
		return;

	if (read_tpm_nvmem(KERNEL_NV_INDEX, sizeof(sec), &sec) !=
		tpm_read_success) {
		CPRINTS("secdata_kernel: read error");
		ec_efs_ctx.secdata_error_code = EC_ERROR_VBOOT_DATA;
		return;
	}

	/*
	 * Check Kernel Version. CRC offset may be different with old struct
	 * version
	 */
	if (sec.kernel_versions < VB2_SECDATA_KERNEL_VERSION_MIN) {
		CPRINTS("secdata_kernel: version incompatible");
		ec_efs_ctx.secdata_error_code =
			EC_ERROR_VBOOT_DATA_INCOMPATIBLE;
		return;
	}

	/* Verify UID */
	if (sec.uid != VB2_SECDATA_KERNEL_UID) {
		CPRINTS("secdata_kernel: bad UID");
		ec_efs_ctx.secdata_error_code = EC_ERROR_VBOOT_DATA_VERIFY;
		return;
	}

	/* Check CRC */
	crc = crc8((uint8_t *)&sec, offsetof(struct vb2_secdata_kernel, crc8));
	if (crc != sec.crc8) {
		CPRINTS("secdata_kernel: bad CRC");
		ec_efs_ctx.secdata_error_code = EC_ERROR_CRC;
		return;
	}

	/* Read hash and copy to hash */
	memcpy(ec_efs_ctx.hash, sec.ec_hash, sizeof(sec.ec_hash));
	ec_efs_ctx.hash_is_loaded = 1;
	ec_efs_ctx.secdata_error_code = EC_SUCCESS;
}

void ec_comm_init(void)
{
	if (!board_ec_cr50_comm_support())
		return;

	init_ec_efs_();
}

void ec_comm_setup(void)
{
	if (!board_ec_cr50_comm_support())
		return;

	ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_RESET;
}

/**
 * TPM vendor command handler to respond with EC Boot Mode.
 *
 * @return VENDOR_RC_SUCCESS
 *
 */
static enum vendor_cmd_rc get_boot_mode_(struct vendor_cmd_params *p)
{
	uint8_t *buffer;

	if (!board_ec_cr50_comm_support())
		return VENDOR_RC_NOT_ALLOWED;

	buffer = (uint8_t *)p->buffer;
	buffer[0] = (uint8_t)ec_efs_ctx.boot_mode;

	p->out_size = 1;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_GET_BOOT_MODE, get_boot_mode_);

/**
 * TPM vendor command handler to reset EC.
 *
 * @return VEDOR_RC_SUCCESS
 */
static enum vendor_cmd_rc reset_ec_(struct vendor_cmd_params *p)
{
	if (!board_ec_cr50_comm_support())
		return VENDOR_RC_NOT_ALLOWED;

	hook_call_deferred(&deferred_reset_ec_data, 50 * MSEC);

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_RESET_EC, reset_ec_);

#ifdef CR50_RELAXED
/**
 * A console command, printing EC-CR50-Comm status.
 */
static int command_ec_comm(int argc, char **argv)
{
	if (!board_ec_cr50_comm_support()) {
		ccprintf("this board does not support ec_cr50_comm");
		return EC_ERROR_INVAL;
	}

	/*
	 * EC-EFS Context
	 */
	ccprintf("[EC-EFS Context]\n");
	ccprintf("ec_hash_is_loaded  : %s\n",
		 ec_efs_ctx.hash_is_loaded ? "YES" : "NO");
	ccprintf("secdata_error_code : 0x%08x\n",
		 ec_efs_ctx.secdata_error_code);
	ccprintf("boot_mode          : 0x%02x\n", ec_efs_ctx.boot_mode);
	ccprintf("ec_hash_secdata    : %ph\n",
		 HEX_BUF(ec_efs_ctx.hash, VB2_SHA256_DIGEST_SIZE));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_comm, command_ec_comm, NULL,
			"Dump EC-CR50-comm info");
#endif  /* CR50_RELAXED */
