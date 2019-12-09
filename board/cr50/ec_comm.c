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
#include "task.h"
#include "timer.h"
#include "tpm_nvmem.h"
#include "tpm_nvmem_ops.h"
#include "tpm_vendor_cmds.h"
#include "uartn.h"
#include "usart.h"
#include "vboot.h"

#if !USE_EC_CR50_COMM
#error "USE_EC_CR50_COMM must be true."
#endif  /* !USE_EC_CR50_COMM */

#ifdef CR50_RELAXED
#define CPRINTS(format, args...) cprints(CC_TASK, "EC-COMM: " format, ## args)
#else
#define CPRINTS(format, args...)
#endif

#define CR50_COMM_NO_RESPONSE          0x00
#define UART_NULL                      0xff
/*
 * EC communications state machine (FSM) is supposed to be active between TPM
 * reset (including power on) and the moment EC_IN_RW transition is latched.
 *
 * Each packet is supposed to be prepended by at least one synchronization
 * character (0xEC). If packet does not verify, the entire stream contents,
 * including all sync characters is delivered to the USB bridge.
 *
 * Meaning of the states included in the enum below is as follows:
 */
enum ec_comm_phase {
	PHASE_READY_COMM = 0,
	PHASE_RECEIVING_PREFIX,
	PHASE_RECEIVING_HEADER,
	PHASE_RECEIVING_DATA,
};

/*
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

/*
 * Context of EC Packet Mode.
 */
static struct ec_packet_context_ {
	uint8_t phase;		/* enum ec_comm_phase */
	uint8_t prefix_count;
	uint8_t bytes_received;
	uint8_t bytes_expected;

	uint8_t uart;
	uint8_t reserved[3];

	union {
		struct cr50_comm_packet ph;
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	};
} ec_packet_ctx;

/*
 * Process the received packet.
 *
 * @return CR50_COMM_SUCCESS if the packet has been processed successfully,
 *         CR50_COMM_ERROR_CRC if CRC is incorrect,
 *         CR50_COMM_ERROR_UNSUPPORTED if the cmd is unknown,
 *         CR50_COMM_ERROR_SIZE if data size is not as expected, or
 *         CR50_COMM_HASH_MISMATCH if the given hash and the hash in NVM are not
 *                                 same.
 */
static uint8_t decode_packet_(struct ec_packet_context_ *ctx)
{
	uint8_t boot_mode;
	uint8_t crc8_calc;

	CPRINTS("decoding a packet: %ph",
		 HEX_BUF((uint8_t *)ec_packet_ctx.packet,
			 CR50_COMM_MAX_PACKET_SIZE));
	/*
	 * We know the size field makes sense and matches the actual packet
	 * length. Let's verify CRC.
	 */
	crc8_calc = crc8((const uint8_t *)&ctx->ph.cmd,
			 ctx->bytes_received - offsetof(struct cr50_comm_packet,
							cmd));
	if (crc8_calc != ctx->ph.crc)
		return CR50_COMM_ERROR_CRC;

	/*
	 * Process the command based on the cmd.
	 */
	switch (ctx->ph.cmd) {
	case CR50_COMM_CMD_SET_BOOT_MODE:
		if (ctx->ph.size != 1)
			return CR50_COMM_ERROR_SIZE;

		boot_mode = ctx->ph.data[0];

		CPRINTS("CR50_COMM_CMD_SET_BOOT_MODE with boot mode"
			" value 0x%02x", boot_mode);

		if (boot_mode != EC_EFS_BOOT_MODE_RECOVERY &&
		    boot_mode != EC_EFS_BOOT_MODE_NO_BOOT_RECOVERY) {
			board_reboot_ec();
			return CR50_COMM_NO_RESPONSE;
		}

		ec_efs_ctx.boot_mode = boot_mode;
		break;

	case CR50_COMM_CMD_VERIFY_HASH:
		if (ctx->ph.size != sizeof(ec_efs_ctx.hash))
			return CR50_COMM_ERROR_SIZE;

		CPRINTS("CR50_COMM_CMD_VERIFY_HASH with hash:");
		CPRINTS("\t%ph", HEX_BUF((uint8_t *)ctx->ph.data,
				VB2_SHA256_DIGEST_SIZE/2));
		CPRINTS("\t%ph", HEX_BUF((uint8_t *)&ctx->ph.data[
				VB2_SHA256_DIGEST_SIZE/2],
				VB2_SHA256_DIGEST_SIZE/2));

		if (memcmp(ctx->ph.data, ec_efs_ctx.hash,
				sizeof(ec_efs_ctx.hash))) {
			ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_NO_BOOT;
			return CR50_COMM_HASH_MISMATCH;
		}

		ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_NORMAL;
		break;

	default:
		return CR50_COMM_ERROR_UNSUPPORTED;
	}

	return CR50_COMM_SUCCESS;
}

int ec_comm_process_packet(uint8_t ch)
{
	uint8_t response = CR50_COMM_NO_RESPONSE;

	switch (ec_packet_ctx.phase) {
	case PHASE_READY_COMM:
		/*
		 * if it is not the PREFIX, then return 0 so that ch can be
		 * forwarded to USB.
		 */
		if (ch != CR50_COMM_PREFIX)
			return 0;

		/*
		 * Forward ch to USB even if it is CR50_COMM_PREFIX, because it
		 * is not yet sure whether it is a prefix or console output.
		 * Forwarding 0xec to USB is not harmful anyway.
		 */
		if (++ec_packet_ctx.prefix_count < MIN_LENGTH_PREFIX)
			return 0;

		/*
		 * TODO(): Move this ec-reset logic to DIOB7 interrupt handler.
		 *
		 * EC-CR50 packet is detected.
		 * Check if EC is in RO. (BOOT_MODE_RESET)
		 * If not, reset EC.
		 */
		if (ec_efs_ctx.boot_mode != EC_EFS_BOOT_MODE_RESET) {
			CPRINTS("packet is detected while EC is not"
				" reset.");
			board_reboot_ec();
			return 0;
		}

		ec_packet_ctx.phase = PHASE_RECEIVING_PREFIX;
		break;

	case PHASE_RECEIVING_PREFIX:
		if (ch == CR50_COMM_PREFIX)
			break;

		ec_packet_ctx.bytes_received = 0;
		ec_packet_ctx.bytes_expected = sizeof(struct cr50_comm_packet);
		ec_packet_ctx.phase = PHASE_RECEIVING_HEADER;
		/* FALLTHROUGH */

	case PHASE_RECEIVING_HEADER:
		ec_packet_ctx.packet[ec_packet_ctx.bytes_received++] = ch;

		if (ec_packet_ctx.bytes_received < ec_packet_ctx.bytes_expected)
			break;

		/* The header has been received. Let's parse it. */
		if (ec_packet_ctx.ph.magic != CR50_COMM_MAGIC_WORD) {
			response = CR50_COMM_ERROR_MAGIC;
			break;
		}

		if (ec_packet_ctx.ph.size == 0) {
			/* Data size zero, then process the packet now. */
			response = decode_packet_(&ec_packet_ctx);
		} else if (ec_packet_ctx.ph.size > CR50_COMM_MAX_DATA_SIZE) {
			response = CR50_COMM_ERROR_SIZE;
		} else {
			ec_packet_ctx.bytes_expected += ec_packet_ctx.ph.size;
			ec_packet_ctx.phase = PHASE_RECEIVING_DATA;
		}
		break;

	case PHASE_RECEIVING_DATA:
		ec_packet_ctx.packet[ec_packet_ctx.bytes_received++] = ch;

		/* The EC is done sending the packet, let's process it. */
		if (ec_packet_ctx.bytes_received >=
		    ec_packet_ctx.bytes_expected)
			response = decode_packet_(&ec_packet_ctx);
		break;

	default:
		/*
		 * It is in the unknown phase.
		 * Let's turn the phase back to READY_COMM, and
		 * return 0 so that ch can be forwarded to USB.
		 */
		ec_packet_ctx.phase = PHASE_READY_COMM;
		ec_packet_ctx.prefix_count = 0;
		return 0;
	}

	switch (response) {
	case CR50_COMM_NO_RESPONSE:
		/* Do nothing */
		break;

	default:
		CPRINTS("EC-CR50 packet response: 0x%02x", response);

		/*
		 * If it reaches here, EC comm is either broken or one packet
		 * was well-processed. Let's turn the phase back to READY_COMM.
		 */
		uartn_write_char(UART_EC, response);

		ec_packet_ctx.phase = PHASE_READY_COMM;
		ec_packet_ctx.prefix_count = 0;
	}

	return 1;
}

/**
 * Set AP to the off state. Disable functionality that should only be available
 * when the AP is on.
 */
static void deferred_reset_ec(void)
{
	CPRINTS("Resetting EC");

	board_reboot_ec();
}
DECLARE_DEFERRED(deferred_reset_ec);

void ec_comm_init(void)
{
	struct vb2_secdata_kernel sec;
	uint8_t crc;

	if (!board_ec_cr50_comm_support())
		return;

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
	ec_efs_ctx.secdata_error_code = EC_SUCCESS;
}

void ec_comm_setup(void)
{
	if (!board_ec_cr50_comm_support())
		return;

	CPRINTS("Setting communication channel ");

	ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_RESET;

	/* TODO(): Setup interrupt instead of enabling packet mode here */
	ec_packet_ctx.uart = UART_EC;
	ccd_update_state();
}

int ec_comm_packet_mode_is_enabled(void)
{
	return ec_packet_ctx.uart == UART_EC;
}

uint8_t ec_comm_get_uart(void)
{
	return ec_packet_ctx.uart;
}

/*
 * TPM vendor command handler to respond with EC Boot Mode.
 *
 * @return VENDOR_RC_SUCCESS
 *
 */
static enum vendor_cmd_rc get_boot_mode_(struct vendor_cmd_params *p)
{
	uint8_t *buffer;

	buffer = (uint8_t *)p->buffer;
	buffer[0] = (uint8_t)ec_efs_ctx.boot_mode;

	p->out_size = 1;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_GET_BOOT_MODE, get_boot_mode_);

/*
 * TPM vendor command handler to reset EC.
 *
 * @return VEDOR_RC_SUCCESS
 */
static enum vendor_cmd_rc reset_ec_(struct vendor_cmd_params *p)
{
	hook_call_deferred(&deferred_reset_ec_data, 50 * MSEC);

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND_P(VENDOR_CC_RESET_EC, reset_ec_);

#ifdef CR50_RELAXED
/*
 * console command, printing EC-CR50-Comm status.
 */
static int command_ec_comm(int argc, char **argv)
{
	/* TODO: DELETE Below */
	/* BEGIN: TO DELETE */
	if (argc > 1) {
		if (!strcasecmp(argv[1], "hash")) {
			int i;

			/* Update the EC Hash */
			if (argc < 2) {
				ccprintf("Hash value to dump is missing\n");
				return EC_ERROR_PARAM2;
			}

			for (i = 0; i < VB2_SHA256_DIGEST_SIZE*2; ++i) {
				char c[2] = { '\0', '\0'};
				char res;
				char *e;

				c[0] = argv[2][i];
				res = strtoi(c, &e, 16);

				ccprintf("c=%c, res=0x%x\n", c[0], res);

				if ((i % 2) == 0) {
					res <<= 4;
					ec_efs_ctx.hash[i/2] = 0;
				}

				ec_efs_ctx.hash[i/2] |= res;
			}

			ec_efs_ctx.hash_is_loaded = 1;
			ec_efs_ctx.secdata_error_code = 0;
		} else {
			return EC_ERROR_PARAM1;
		}
	}
	/* END: TO DELETE */

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

	/*
	 * EC Packet Context
	 */
	ccprintf("\n[EC Packet Context]\n");
	ccprintf("phase              : %d\n", ec_packet_ctx.phase);
	ccprintf("prefix_count       : %d\n", ec_packet_ctx.prefix_count);
	ccprintf("bytes_received     : %d\n", ec_packet_ctx.bytes_received);
	ccprintf("bytes_expected     : %d\n", ec_packet_ctx.bytes_expected);
	ccprintf("uart               : %d\n", ec_packet_ctx.uart);
	ccprintf("EC-CR50 comm       : %sABLED.\n",
		ec_comm_packet_mode_is_enabled() ? "EN" : "DIS");

	ccprintf("E-CR50 comm packet:\n");
	hexdump((uint8_t *)ec_packet_ctx.packet, CR50_COMM_MAX_PACKET_SIZE);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_comm, command_ec_comm, "",
			"Dump EC-CR50-comm info");

#endif  /* CR50_RELAXED */
