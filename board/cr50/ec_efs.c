/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-EFS (Early Firmware Selection)
 */
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "registers.h"
#include "sha256.h"
#include "system.h"
#include "vboot.h"

#define CPRINTS(format, args...) cprints(CC_TASK, "EC-EFS: " format, ## args)

/*
 * Context of EC-EFS
 */
static struct ec_efs_context_ {
	uint32_t boot_mode:8;	/* enum ec_efs_boot_mode */
	uint32_t reserved:24;

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
#ifdef CR50_DEV
	CPRINTS("boot_mode: 0x%02x -> 0x%02x",
		ec_efs_ctx.boot_mode, mode_val);
#endif
	ec_efs_ctx.boot_mode = mode_val;
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

	/* TODO(crbug/1020578): Read Hash from Kernel NV Index */
}
DECLARE_HOOK(HOOK_INIT, ec_efs_init_, HOOK_PRIO_DEFAULT);

/*
 * Deferred function to reset EC.
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
