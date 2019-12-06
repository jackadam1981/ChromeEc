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

#ifdef CR50_DEV
#define CPRINTS(format, args...) cprints(CC_TASK, "EC-EFS: " format, ## args)
#else
#define CPRINTS(format, args...) do { } while (0)
#endif
/*
 * Context of EC-EFS
 */
static struct ec_efs_context_ {
	uint32_t boot_mode:8;	        /* enum ec_efs_boot_mode */
	uint32_t hash_is_loaded:1;	/* Is EC hash loaded from nvmem */
	uint32_t reserved:23;

	uint32_t secdata_error_code;

	uint8_t hash[SHA256_DIGEST_SIZE];	/* EC-RW digest */
} ec_efs_ctx;

const char * const ec_efs_error_mesg_[] = {
	"Packet mode enabled in a wrong boot mode",
	"Set the wrong boot_mode",
};

/*
 * Return the current boot mode
 * @return uint8_t
 */
static inline enum ec_efs_boot_mode get_boot_mode_(void)
{
	return ec_efs_ctx.boot_mode;
}

/*
 * Change the boot mode
 *
 * @param mode_val New boot mode value to change
 */
static inline void set_boot_mode_(uint8_t mode_val)
{
	CPRINTS("boot_mode: 0x%02x -> 0x%02x", ec_efs_ctx.boot_mode, mode_val);

	ec_efs_ctx.boot_mode = mode_val;
}

static void load_ec_hash_(struct ec_efs_context_ *ctx)
{
	uint8_t buf[256];
	struct vb2_secdata_kernel *secdata = (struct vb2_secdata_kernel *)buf;
	const uint8_t secdata_size = sizeof(struct vb2_secdata_kernel);
	uint8_t size_to_crc;
	uint8_t struct_size;
	uint8_t crc;

	if (read_tpm_nvmem(KERNEL_NV_INDEX, secdata_size,
			   &buf) != tpm_read_success) {
		CPRINTS("secdata_kernel: old version or doesn't exist");
		ctx->secdata_error_code = EC_ERROR_VBOOT_DATA_UNDERSIZED;
		return;
	}

	/*
	 * Check Struct Version. CRC offset may be different with old struct
	 * version
	 */
	if (secdata->struct_version < VB2_SECDATA_KERNEL_STRUCT_VERSION_MIN) {
		CPRINTS("secdata_kernel: version incompatible");
		ctx->secdata_error_code = EC_ERROR_VBOOT_DATA_INCOMPATIBLE;
		return;
	}

	/*
	 * Check struct size.
	 */
	struct_size = secdata->struct_size;
	if (struct_size < secdata_size) {
		CPRINTS("secdata_kernel: undersized (%d bytes)", struct_size);
		ctx->secdata_error_code = EC_ERROR_VBOOT_DATA_UNDERSIZED;
		return;
	}

	/*
	 * If it is bigger than secdata_size, then
	 * the whole struct should be read so that CRC can be checked.
	 */
	if (struct_size > secdata_size) {
		if (read_tpm_nvmem(KERNEL_NV_INDEX, struct_size, &buf)
		    != tpm_read_success) {
			CPRINTS("secdata_kernel: read error");
			ctx->secdata_error_code = EC_ERROR_VBOOT_DATA;
			return;
		}
	}

	/* Check CRC */
	size_to_crc = struct_size -
		      offsetof(struct vb2_secdata_kernel, crc8) -
		      sizeof(secdata->crc8);
	crc = crc8((uint8_t *)&secdata->reserved0, size_to_crc);
	if (crc != secdata->crc8) {
		CPRINTS("secdata_kernel: bad CRC");
		ctx->secdata_error_code = EC_ERROR_CRC;
		return;
	}

	/* Read hash and copy to hash */
	memcpy(ctx->hash, secdata->ec_hash, sizeof(secdata->ec_hash));
	ctx->hash_is_loaded = 1;
	ctx->secdata_error_code = EC_SUCCESS;
}

/*
 * Initialize EC-EFS context.
 */
static void ec_efs_init_(void)
{
	if (!board_has_ec_cr50_comm_support())
		return;

	/*
	 * If it is a wakeup from deep sleep, then recover some core EC-EFS
	 * context values, including the boot_mode value, from a PWRD_SCRATCH
	 * register. Otherwise, reset boot_mode.
	 */
	if (system_get_reset_flags() & EC_RESET_FLAG_HIBERNATE)
		set_boot_mode_(GREG32(PMU, PWRDN_SCRATCH20) & 0xff);
	else
		ec_efs_reset();

	/* Read an EC hash in kernel secdata (TPM kernel NV index). */
	if (ec_efs_ctx.hash_is_loaded)
		return;

	load_ec_hash_(&ec_efs_ctx);
}
DECLARE_HOOK(HOOK_INIT, ec_efs_init_, HOOK_PRIO_DEFAULT);

/*
 * Deferred function to reset EC.
 */
static void deferred_ec_reset(void)
{
	cprintf(CC_TASK, "EC-EFS: reset EC");
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
		if (!strcasecmp(argv[1], "hash")) {
			char *ptr;
			int len;

			if (argc < 2)
				return EC_ERROR_PARAM2;

			ccprintf("dumping hash...\n");

			/* Overwrite EC hash code with argv[2] */
			len = 0;
			ptr = (char *)&argv[2][0];
			while (*ptr) {
				char in[2] = {'\0', '\0'};
				uint8_t out;

				in[0] = *ptr;
				out = strtoul(in, NULL, 16);

				if (len % 2)
					ec_efs_ctx.hash[len/2] |= out;
				else
					ec_efs_ctx.hash[len/2] = out << 4;

				len++;
				ptr++;
			}
		} else {
			return EC_ERROR_PARAM1;
		}
		ccprintf("\n");
	}
#endif

	/*
	 * EC-EFS Context
	 */
	ccprintf("[EC-EFS Context]\n");
	ccprintf("boot_mode          : 0x%02x\n", get_boot_mode_());

	ccprintf("ec_hash is loaded  : %s\n",
		 ec_efs_ctx.hash_is_loaded ? "YES" : "NO");
	ccprintf("secdata_error_code : 0x%08x\n",
		 ec_efs_ctx.secdata_error_code);
#ifdef CR50_RELAXED
	ccprintf("ec_hash_secdata    : %ph\n",
		 HEX_BUF(ec_efs_ctx.hash, SHA256_DIGEST_SIZE));
#endif

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(ec_efs, command_ec_efs, NULL,
			     "Display EC-EFS status");

void ec_efs_reset(void)
{
	set_boot_mode_(EC_EFS_BOOT_MODE_NORMAL);
}

void ec_efs_ready_for_sleep(void)
{
	/* Backup some ec_efs context to scratch register */
	GREG32(PMU, PWRDN_SCRATCH20) &= ~0xff;
	GREG32(PMU, PWRDN_SCRATCH20) |= (get_boot_mode_() & 0xff);
}

/*
 * Change the EC boot mode value.
 *
 * @param data Pointer to the EC-CR50 packet
 * @param size Data (payload) size in EC-CR50 packet
 * @return CR50_COMM_SUCCESS if the packet has been processed successfully,
 *         CR50_COMM_ERROR_SIZE if data size is not as expected, or
 *         0 if it does not respond to EC.
 */
uint16_t ec_efs_set_boot_mode(const char *data, const uint8_t size)
{
	const uint8_t boot_mode = data[0];

	if (size != 1)
		return CR50_COMM_ERROR_SIZE;

	if (boot_mode != EC_EFS_BOOT_MODE_NORMAL) {
		hook_call_deferred(&deferred_ec_reset_data, 0);
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
uint16_t ec_efs_verify_hash(const char *hash_data, const uint8_t size)
{
	if (size != SHA256_DIGEST_SIZE)
		return CR50_COMM_ERROR_SIZE;

	if (!ec_efs_ctx.hash_is_loaded)
		load_ec_hash_(&ec_efs_ctx);

	if (memcmp(hash_data, ec_efs_ctx.hash, SHA256_DIGEST_SIZE)) {
		/* Verification failed */
		set_boot_mode_(EC_EFS_BOOT_MODE_NO_BOOT);
		return CR50_COMM_ERROR_HASH_MISMATCH;
	}

	if (get_boot_mode_() != EC_EFS_BOOT_MODE_NORMAL) {
		hook_call_deferred(&deferred_ec_reset_data, 0);
		return 0;
	}

	return CR50_COMM_SUCCESS;
}

void ec_efs_refresh(void)
{
	ec_efs_ctx.hash_is_loaded = 0;

	load_ec_hash_(&ec_efs_ctx);
}
