/* Copyright 2020 The Chromium OS Authors. All rights reserved.
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
#include "ec_commands.h"
#include "extension.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
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
	union {
		struct context_in_pwdn_scratch_ {
			uint32_t boot_mode:8;	/* enum ec_efs_boot_mode */
			uint32_t reserved:24;
		} b;
		uint32_t val;
	} scratch;

	uint32_t efs_enabled:1;
	uint32_t hash_is_loaded:1;		/* Is EC hash loaded       */
						/* from kernel secdata?    */
	uint32_t reserved:30;
	uint32_t secdata_error_code;

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
	 * Check Struct Version. CRC offset may be different with old struct
	 * version
	 */
	if (sec.struct_version < VB2_SECDATA_KERNEL_STRUCT_VERSION_MIN) {
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

int ec_comm_is_enabled(void)
{
	return !!ec_efs_ctx.efs_enabled;
}

void ec_comm_packet_mode_en(enum gpio_signal signal)
{
	ccd_update_state();
}

void ec_comm_packet_mode_dis(enum gpio_signal signal)
{
	ccd_update_state();
}

void ec_comm_configure_wakepin(void)
{
	if (!ec_efs_ctx.efs_enabled)
		return;

	/* Disable DIOB7 as a wake pin */
	GWRITE_FIELD(PINMUX, EXITEN0,   DIOB3, 0);
	GWRITE_FIELD(PINMUX, EXITEDGE0, DIOB3, 1); /* edge triggered */
	GWRITE_FIELD(PINMUX, EXITINV0,  DIOB3, 0); /* wake on rising */
	/* enable powerdown exit */
	GWRITE_FIELD(PINMUX, EXITEN0,   DIOB3, 1);

	/* Store Boot Flag to PWDN_SCRATCH20 */
	GREG32(PMU, PWRDN_SCRATCH20) = ec_efs_ctx.scratch.val;
}

void ec_comm_init(void)
{
	uint32_t ctl_backup = GREAD(PINMUX, DIOB3_CTL);
	uint32_t sel_backup = GREAD(PINMUX, DIOB3_SEL);

	/*
	 * Apply pull-up resistor on DIOB3 and read the level.
	 * If the level is high, the board supports EC-CR50 communication.
	 * Otherwise, it does not.
	 */

	/* Disable FED/RED interrupt on DIOB3 temporarily. */
	gpio_disable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_disable_interrupt(GPIO_EC_PACKET_MODE_DIS);

	/*
	 * Disconnect output pinmux of DIOB3 since GPIO_AP_FLASH_SEL flag is
	 * GPIO_OUT_LOW.
	 */
	GWRITE(PINMUX, DIOB3_SEL, 0);
	udelay(100);

	/* Configure DIOB3 as DIO_CTL_IE_MASK | GPIO_PULL_UP. */
	REG_WRITE_MLV(GREG32(PINMUX, DIOB3_CTL),
		(DIO_CTL_IE_MASK | DIO_CTL_PU_MASK | DIO_CTL_PD_MASK), 0,
		(DIO_CTL_IE_MASK | DIO_CTL_PU_MASK));
	udelay(100);

	/*
	 * Read the level of DIOB3.
	 * Boards supporting EC-EFS have a 1M pull-down on DIOB3, and
	 * The other boards have a 10K pull-down.
	 */
	ec_efs_ctx.efs_enabled = !!gpio_get_level(GPIO_EC_PACKET_MODE_EN);

	/* Recover the DIOB3 pinmux control register value. */
	GWRITE(PINMUX, DIOB3_CTL, ctl_backup);

	if (ec_efs_ctx.efs_enabled) {
		/* Connect GPIO_AP_FLASH_SELECT to DIOB4. */
		GWRITE(PINMUX, DIOB4_SEL, GC_PINMUX_GPIO0_GPIO2_SEL);
		GWRITE(PINMUX, GPIO0_GPIO2_SEL, GC_PINMUX_DIOB4_SEL);

		/* Enable FED/RED interrupt on DIOB3 */
		gpio_enable_interrupt(GPIO_EC_PACKET_MODE_EN);
		gpio_enable_interrupt(GPIO_EC_PACKET_MODE_DIS);
	} else {
		/* Recover the DIOB3 pinmux select register value. */
		GWRITE(PINMUX, DIOB3_SEL, sel_backup);

		/*
		 * Disconnect GPIO_EC_PACKET_MODE_EN and
		 *  GPIO_EC_PACKET_MODE_DIS from DIOB3.
		 */
		GWRITE(PINMUX, GPIO1_GPIO7_SEL, 0);
		GWRITE(PINMUX, GPIO1_GPIO8_SEL, 0);

		/* No need to proceed */
		return;
	}

	CPRINTS("Initializtion");

	/*
	 * If it is a wakeup from deep sleep, then recover some core EC-EFS
	 * context values, including the boot_mode value, from a PWRD_SCRATCH
	 * register. Otherwise, reset boot_mode.
	 */
	if (system_get_reset_flags() & EC_RESET_FLAG_HIBERNATE)
		ec_efs_ctx.scratch.val = GREG32(PMU, PWRDN_SCRATCH20);
	else
		ec_efs_ctx.scratch.b.boot_mode = EC_EFS_BOOT_MODE_RESET;

	init_ec_efs_();
}

void ec_comm_setup(void)
{
	if (!ec_efs_ctx.efs_enabled)
		return;

	CPRINTS("Setup");

	ec_efs_ctx.scratch.b.boot_mode = EC_EFS_BOOT_MODE_RESET;
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

	if (!ec_efs_ctx.efs_enabled)
		return VENDOR_RC_NOT_ALLOWED;

	buffer = (uint8_t *)p->buffer;
	buffer[0] = (uint8_t)ec_efs_ctx.scratch.b.boot_mode;

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
	if (!ec_efs_ctx.efs_enabled)
		return VENDOR_RC_NOT_ALLOWED;

	CPRINTS("VENDOR_CC_RESET_EC");

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
	if (!ec_efs_ctx.efs_enabled) {
		ccprintf("this board does not support ec_cr50_comm.\n");
		return EC_ERROR_INVAL;
	}

	/*
	 * EC-EFS Context
	 */
	ccprintf("[EC-EFS Context]\n");
	ccprintf("EFS feature        : %sABLED\n",
		 ec_efs_ctx.efs_enabled ? "EN" : "DIS");
	ccprintf("ec_hash_is_loaded  : %s\n",
		 ec_efs_ctx.hash_is_loaded ? "YES" : "NO");
	ccprintf("secdata_error_code : 0x%08x\n",
		 ec_efs_ctx.secdata_error_code);

	ccprintf("boot_mode          : 0x%02x\n",
		 ec_efs_ctx.scratch.b.boot_mode);
	ccprintf("ec_hash_secdata    : %ph\n",
		 HEX_BUF(ec_efs_ctx.hash, VB2_SHA256_DIGEST_SIZE));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_comm, command_ec_comm, NULL,
			"Dump EC-CR50-comm info");
#endif  /* CR50_RELAXED */
