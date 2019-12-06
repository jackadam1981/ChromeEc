/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-EFS (Early Firmware Selection)
 */
#include "common.h"
#include "console.h"
#include "crc8.h"
#include "ec_commands.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "tpm_nvmem.h"
#include "tpm_nvmem_ops.h"
#include "vboot.h"

#define CPRINTS(format, args...) cprints(CC_TASK, "EC-EFS: " format, ## args)

/*
 * Context of EC-EFS
 */
static struct ec_efs_context_ {
	union {
		struct context_in_pwdn_scratch_ {
			uint32_t boot_mode:8;	/* enum ec_efs_boot_mode */
			uint32_t reserved:24;
		} b;
		uint32_t val;
	} scratch;

	uint32_t hash_is_loaded:1;		/* Is EC hash loaded       */
						/* from kernel secdata?    */
	uint32_t reserved:31;
	uint32_t secdata_error_code;

	uint8_t hash[SHA256_DIGEST_SIZE];	/* EC-RW digest */
} ec_efs_ctx;

enum ec_efs_error_code_ {
	EC_EFS_ERROR_PACKET_VIOLATION,		/* The command is not allowed */
						/*  in the curret boot mode.  */
	EC_EFS_ERROR_SET_BOOT_MODE,             /* Te given boot mode is not  */
						/*  allowed to be set.        */
};

const char * const ec_efs_error_mesg_[] = {
	"Packet mode enabled in a wrong boot mode",
	"Set the wrong boot_mode",
};

/*
 * Change the boot mode
 *
 * @param mode_val New boot mode value to change
 */
static inline void set_boot_mode_(uint8_t mode_val)
{
	CPRINTS("boot_mode: 0x%02x -> 0x%02x",
		ec_efs_ctx.scratch.b.boot_mode, mode_val);

	ec_efs_ctx.scratch.b.boot_mode = mode_val;
}

/*
 * Initialize EC-EFS context.
 */
static void ec_efs_init_(void)
{
	/* 'buf' is twice as vb2_secdata_kernel for its future extension. */
	uint8_t buf[sizeof(struct vb2_secdata_kernel) * 2];
	struct vb2_secdata_kernel *secdata = (struct vb2_secdata_kernel *)buf;
	const uint32_t secdata_size = sizeof(struct vb2_secdata_kernel);
	uint32_t size_to_crc;
	uint32_t size_struct;
	uint8_t crc;

	if (!board_has_ec_cr50_comm_support())
		return;

	/*
	 * If it is a wakeup from deep sleep, then recover some core EC-EFS
	 * context values, including the boot_mode value, from a PWRD_SCRATCH
	 * register. Otherwise, reset boot_mode.
	 */
	if (system_get_reset_flags() & EC_RESET_FLAG_HIBERNATE)
		ec_efs_ctx.scratch.val = GREG32(PMU, PWRDN_SCRATCH20);
	else
		ec_efs_reset();

	/* Read an EC hash in kernel secdata (TPM kernel NV index). */
	if (ec_efs_ctx.hash_is_loaded)
		return;

	if (read_tpm_nvmem(KERNEL_NV_INDEX, secdata_size,
			   &buf) != tpm_read_success) {
		CPRINTS("secdata_kernel: read error");
		ec_efs_ctx.secdata_error_code = EC_ERROR_VBOOT_DATA;
		return;
	}

	/*
	 * Check Struct Version. CRC offset may be different with old struct
	 * version
	 */
	if (secdata->struct_version < VB2_SECDATA_KERNEL_STRUCT_VERSION_MIN) {
		CPRINTS("secdata_kernel: version incompatible");
		ec_efs_ctx.secdata_error_code =
			EC_ERROR_VBOOT_DATA_INCOMPATIBLE;
		return;
	}

	/*
	 * Check struct size.
	 */
	size_struct = secdata->struct_size;
	if (size_struct > sizeof(buf)) {
		CPRINTS("secdata_kernel: oversized (%d bytes)", size_struct);
		ec_efs_ctx.secdata_error_code =
			EC_ERROR_VBOOT_DATA_OVERSIZED;
		return;
	}

	if (size_struct < secdata_size) {
		CPRINTS("secdata_kernel: undersized (%d bytes)", size_struct);
		ec_efs_ctx.secdata_error_code =
			EC_ERROR_VBOOT_DATA_UNDERSIZED;
		return;
	}

	/*
	 * If it is bigger than secdata_size, then
	 * the whole struct should be read so that CRC can be checked.
	 */
	if (size_struct > secdata_size) {
		if (read_tpm_nvmem(KERNEL_NV_INDEX, size_struct, &buf)
		    != tpm_read_success) {
			CPRINTS("secdata_kernel: read error");
			ec_efs_ctx.secdata_error_code = EC_ERROR_VBOOT_DATA;
			return;
		}
	}

	/* Check CRC */
	size_to_crc = size_struct -
		      offsetof(struct vb2_secdata_kernel, crc8) -
		      sizeof(secdata->crc8);
	crc = crc8((uint8_t *)&secdata->reserved0, size_to_crc);
	if (crc != secdata->crc8) {
		CPRINTS("secdata_kernel: bad CRC");
		ec_efs_ctx.secdata_error_code = EC_ERROR_CRC;
		return;
	}

	/* Read hash and copy to hash */
	memcpy(ec_efs_ctx.hash, secdata->ec_hash, sizeof(secdata->ec_hash));
	ec_efs_ctx.hash_is_loaded = 1;
	ec_efs_ctx.secdata_error_code = EC_SUCCESS;
}
DECLARE_HOOK(HOOK_INIT, ec_efs_init_, HOOK_PRIO_DEFAULT);

/*
 * Set AP to the off state. Disable functionality that should only be available
 * when the AP is on.
 */
static void deferred_ec_reset(void)
{
	CPRINTS("reset EC");
	board_reboot_ec();
}
DECLARE_DEFERRED(deferred_ec_reset);

/*
 * A console command, printing EC-EFS status.
 */
static int command_ec_efs(int argc, char **argv)
{
	if (!board_has_ec_cr50_comm_support()) {
		ccprintf("This board does not support ec-efs.\n");
		return EC_ERROR_INVAL;
	}

#ifdef CR50_RELAXED
	if (argc > 1) {
		char *ptr;
		int len;

		if (strcasecmp(argv[1], "hash"))
			return EC_ERROR_PARAM1;

		if (argc < 2)
			return EC_ERROR_PARAM2;

		/* Overwrite EC hash code with argv[2] */
		len = 0;
		ptr = (char *)&argv[2][0];
		while (*ptr) {
			char in = *ptr;
			uint8_t out = strtoul(&in, NULL, 16);

			if (len % 2)
				ec_efs_ctx.hash[len/2] |= out;
			else
				ec_efs_ctx.hash[len/2] = out << 4;

			len++;
			ptr++;
		}
	}
#endif

	/*
	 * EC-EFS Context
	 */
	ccprintf("[EC-EFS Context]\n");
	ccprintf("boot_mode          : 0x%02x\n",
		 ec_efs_ctx.scratch.b.boot_mode);

	ccprintf("ec_hash_is_loaded  : %s\n",
		 ec_efs_ctx.hash_is_loaded ? "YES" : "NO");
	ccprintf("secdata_error_code : 0x%08x\n",
		 ec_efs_ctx.secdata_error_code);
#ifdef CR50_RELAXED
	ccprintf("ec_hash_secdata    : %ph\n",
		 HEX_BUF(ec_efs_ctx.hash, SHA256_DIGEST_SIZE));
#endif

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_efs, command_ec_efs, NULL, "Dump EC-EFS status");

static void ec_reset_to_correct_boot_mode_(int error_code)
{
	CPRINTS("boot_mode          : 0x%02x", ec_efs_ctx.scratch.b.boot_mode);
	CPRINTS("error_code         : 0x%x", error_code);
	if (error_code < ARRAY_SIZE(ec_efs_error_mesg_))
		CPRINTS("ERR: %s", ec_efs_error_mesg_[error_code]);

	hook_call_deferred(&deferred_ec_reset_data, 0);
}

void ec_efs_reset(void)
{
	set_boot_mode_(EC_EFS_BOOT_MODE_RESET);
}

void ec_efs_ready_for_sleep(void)
{
	/* Backup some ec_efs context to scratch register */
	GREG32(PMU, PWRDN_SCRATCH20) = ec_efs_ctx.scratch.val;
}

/*
 * Change the EC boot mode value.
 *
 * @param data Pointer to the EC-CR50 packet
 * @param size Data (payload) size in EC-CR50 packet
 * @return CR50_COMM_SUCCESS if the packet has been processed successfully,
 *         CR50_COMM_ERROR_SIZE if data size is not as expected, or
 *         0 if it deosn't have to respond to EC.
 */
uint16_t ec_efs_set_boot_mode(const char *data, const int size)
{
	const uint8_t boot_mode = data[0];

	if (size != 1)
		return CR50_COMM_ERROR_SIZE;

	if (ec_efs_ctx.scratch.b.boot_mode != EC_EFS_BOOT_MODE_RESET) {
		/* Packet mode enabled in a wrong boot mode */
		ec_reset_to_correct_boot_mode_(EC_EFS_ERROR_PACKET_VIOLATION);
		return 0;
	}

	if (boot_mode != EC_EFS_BOOT_MODE_RECOVERY &&
	    boot_mode != EC_EFS_BOOT_MODE_NO_BOOT_RECOVERY) {
		ec_reset_to_correct_boot_mode_(EC_EFS_ERROR_SET_BOOT_MODE);
		return 0;
	}

	set_boot_mode_(boot_mode);
	return CR50_COMM_SUCCESS;
}

/*
 * Verify the given EC-FW hash against one in kernel secdata.
 *
 * @param data Pointer to the EC-CR50 packet
 * @param size Data (payload) size in EC-CR50 packet
 * @return CR50_COMM_SUCCESS if the packet has been processed successfully,
 *         CR50_COMM_ERROR_SIZE if data size is not as expected, or
 *         CR50_COMM_ERROR_HASH_MISMATCH if the given hash and the hash in NVM
 *                                       are not same.
 *         0 if it deosn't have to respond to EC.
 */
uint16_t ec_efs_verify_hash(const char *hash_data, const int size)
{
	if (ec_efs_ctx.scratch.b.boot_mode != EC_EFS_BOOT_MODE_RESET) {
		/* Packet mode enabled in a wrong boot mode */
		ec_reset_to_correct_boot_mode_(EC_EFS_ERROR_PACKET_VIOLATION);
		return 0;
	}

	if (size != SHA256_DIGEST_SIZE)
		return CR50_COMM_ERROR_SIZE;

	if (memcmp(hash_data, ec_efs_ctx.hash, SHA256_DIGEST_SIZE)) {
		/* Verification failed */
		set_boot_mode_(EC_EFS_BOOT_MODE_NO_BOOT);
		return CR50_COMM_ERROR_HASH_MISMATCH;
	}

	set_boot_mode_(EC_EFS_BOOT_MODE_NORMAL);
	return CR50_COMM_SUCCESS;
}
