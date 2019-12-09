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
#include "gpio.h"
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

#ifdef CR50_RELAXED
#define CPRINTS(format, args...) cprints(CC_TASK, "EC-COMM: " format, ## args)
#else
#define CPRINTS(format, args...)
#endif

#define CR50_COMM_NO_RESPONSE          0x00
#define UART_NULL                      0xff
#define EC_BOOT_MODE_MASK              0xff

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
 * Context of EC Packet Mode.
 */
static struct ec_packet_context_ {
	uint8_t phase;		/* enum ec_comm_phase */
	uint8_t prefix_count;
	uint8_t bytes_received;
	uint8_t bytes_expected;

	uint8_t uart;		/* UART_EC if packet mode is enabled */
				/* UART_NULL if packet mode is disabled */
	uint8_t reserved[3];

	union {
		struct cr50_comm_packet ph;
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	};
} ec_packet_ctx;

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

/**
 * return 1 if 'cmd' is alllowed in 'boot_mode' or
 *        0 otherwise.
 */
static int check_command_validity_(uint32_t cmd, uint32_t boot_mode)
{
	if ((cmd == CR50_COMM_CMD_SET_BOOT_MODE) ||
	    (cmd == CR50_COMM_CMD_VERIFY_HASH))
		return boot_mode == EC_EFS_BOOT_MODE_RESET;

	return 0;
}

/**
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

	if (!check_command_validity_(ctx->ph.cmd, ec_efs_ctx.boot_mode)) {
		CPRINTS("Packet mode enabled in a wrong boot mode: %d",
			ec_efs_ctx.boot_mode);

		hook_call_deferred(&deferred_reset_ec_data, 0);
		return CR50_COMM_NO_RESPONSE;
	}

	/*
	 * Process the command based on the cmd.
	 */
	switch (ctx->ph.cmd) {
	case CR50_COMM_CMD_SET_BOOT_MODE:
		if (ctx->ph.size != 1)
			return CR50_COMM_ERROR_SIZE;

		boot_mode = ctx->ph.data[0];

		if (boot_mode != EC_EFS_BOOT_MODE_RECOVERY &&
		    boot_mode != EC_EFS_BOOT_MODE_NO_BOOT_RECOVERY) {
			CPRINTS("Wrong input parameter: 0x%02x", boot_mode);

			hook_call_deferred(&deferred_reset_ec_data, 0);
			return CR50_COMM_NO_RESPONSE;
		}
		CPRINTS("EC boot_mode: 0x%02x -> 0x%02x",
			ec_efs_ctx.boot_mode, boot_mode);
		ec_efs_ctx.boot_mode = boot_mode;
		break;

	case CR50_COMM_CMD_VERIFY_HASH:
		if (ctx->ph.size != sizeof(ec_efs_ctx.hash))
			return CR50_COMM_ERROR_SIZE;

		if (memcmp(ctx->ph.data, ec_efs_ctx.hash,
				sizeof(ec_efs_ctx.hash))) {
			CPRINTS("EC boot_mode: 0x%02x -> 0x%02x",
				ec_efs_ctx.boot_mode, EC_EFS_BOOT_MODE_NO_BOOT);

			ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_NO_BOOT;
			return CR50_COMM_HASH_MISMATCH;
		}
		CPRINTS("EC boot_mode: 0x%02x -> 0x%02x",
			ec_efs_ctx.boot_mode, EC_EFS_BOOT_MODE_NORMAL);

		ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_NORMAL;
		break;

	default:
		return CR50_COMM_ERROR_UNSUPPORTED;
	}

	return CR50_COMM_SUCCESS;
}

/**
 * Transfer a 'response' to 'uart' port using DIOB7 pin.
 */
static void transfer_response_to_ec_(uint8_t response)
{
	int uart = ec_packet_ctx.uart;

	/* Let's response to EC */
	CPRINTS("EC-CR50 packet response: 0x%02x", response);

	/*
	 * Connect DIOB7 to UART2_TX so that Cr50 can send the response
	 * to EC. Must disable DIB7 interrupt.
	 */
	gpio_disable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_disable_interrupt(GPIO_EC_PACKET_MODE_DIS);
	GWRITE(PINMUX, DIOB7_SEL, GC_PINMUX_UART2_TX_SEL);

	/* Send the response to EC. */
	uartn_write_char(uart, response);
	uartn_tx_flush(uart);  /* Flush from UART2_TX to DIOB7 */

	/*
	 * Disconnect DIOB7 from UART2_TX, and re-enable its interrupt.
	 */
	GWRITE(PINMUX, DIOB7_SEL, 0);
	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_DIS);
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

		ec_packet_ctx.phase = PHASE_RECEIVING_PREFIX;
		break;

	case PHASE_RECEIVING_PREFIX:
		if (ch == CR50_COMM_PREFIX) {
			++ec_packet_ctx.prefix_count;
			break;
		}

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

	if (response != CR50_COMM_NO_RESPONSE) {
		transfer_response_to_ec_(response);

		/*
		 * If it reaches here, EC comm is either broken or one packet
		 * was well-processed. Let's turn the phase back to READY_COMM.
		 */
		ec_packet_ctx.phase = PHASE_READY_COMM;
		ec_packet_ctx.prefix_count = 0;
	}

	return 1;
}

int ec_comm_packet_mode_is_enabled(void)
{
	return board_ec_cr50_comm_support() && (ec_packet_ctx.uart == UART_EC);
}

int ec_comm_is_uart_in_packet_mode(int uart)
{
	return uart == ec_packet_ctx.uart;
}

void ec_comm_packet_mode_en(enum gpio_signal signal)
{
	/* Initialize packet context */
	ec_packet_ctx.phase = PHASE_READY_COMM;
	ec_packet_ctx.prefix_count = 0;
	ec_packet_ctx.uart = UART_EC;  /* Enable Packet Mode */
	ccd_update_state();
}

void ec_comm_packet_mode_dis(enum gpio_signal signal)
{
	ec_packet_ctx.uart = UART_NULL;	 /* Disable Packet Mode. */
	ccd_update_state();
}

void ec_comm_configure_wakepin(void)
{
	/* Disable DIOB7 as a wake pin */
	GWRITE_FIELD(PINMUX, EXITEN0,   DIOB7, 0);
	GWRITE_FIELD(PINMUX, EXITEDGE0, DIOB7, 1); /* edge triggered */
	GWRITE_FIELD(PINMUX, EXITINV0,  DIOB7, 0); /* wake on rising */
	/* enable powerdown exit */
	GWRITE_FIELD(PINMUX, EXITEN0,   DIOB7, 1);

	/* Store Boot Flag to PWDN_SCRATCH20 */
	GREG32(PMU, PWRDN_SCRATCH20) = ec_efs_ctx.boot_mode & EC_BOOT_MODE_MASK;

	CPRINTS("PWRDN_SCRATCH20 = 0x%04x", GREG32(PMU, PWRDN_SCRATCH20));
}

void ec_comm_init(void)
{
	gpio_disable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_disable_interrupt(GPIO_EC_PACKET_MODE_DIS);

	ec_packet_ctx.uart = UART_NULL;

	if (!board_ec_cr50_comm_support()) {
		GWRITE(PINMUX, GPIO1_GPIO7_SEL, 0);
		GWRITE(PINMUX, GPIO1_GPIO8_SEL, 0);
		return;
	}

	CPRINTS("Initializtion");

	init_ec_efs_();

	CPRINTS("PWRDN_SCRATCH20 = 0x%04x", GREG32(PMU, PWRDN_SCRATCH20));
	ec_efs_ctx.boot_mode = GREG32(PMU, PWRDN_SCRATCH20) & EC_BOOT_MODE_MASK;

	/*
	 * If it is a reset not a wakeup from deep sleep,
	 * boot_mode is zero. Let's reset boot_mode.
	 */
	if (ec_efs_ctx.boot_mode == 0)
		ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_RESET;
	CPRINTS("EC boot_mode: -> 0x%02x", ec_efs_ctx.boot_mode);

	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_DIS);
}

void ec_comm_setup(void)
{
	if (!board_ec_cr50_comm_support())
		return;

	CPRINTS("Setup");
	CPRINTS("EC boot_mode: 0x%02x -> 0x%02x",
		ec_efs_ctx.boot_mode, EC_EFS_BOOT_MODE_RESET);

	ec_efs_ctx.boot_mode = EC_EFS_BOOT_MODE_RESET;

	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_EN);
	gpio_enable_interrupt(GPIO_EC_PACKET_MODE_DIS);
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
	if (!board_ec_cr50_comm_support()) {
		ccprintf("this board does not support ec_cr50_comm.\n");
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

	/*
	 * EC Packet Context
	 */
	ccprintf("\n");
	ccprintf("[EC Packet Context]\n");
	ccprintf("phase              : %d\n", ec_packet_ctx.phase);
	ccprintf("prefix_count       : %d\n", ec_packet_ctx.prefix_count);
	ccprintf("bytes_received     : %d\n", ec_packet_ctx.bytes_received);
	ccprintf("bytes_expected     : %d\n", ec_packet_ctx.bytes_expected);
	ccprintf("uart               : 0x%02x\n", ec_packet_ctx.uart);
	ccprintf("packet mode        : %sABLED.\n",
		ec_comm_packet_mode_is_enabled() ? "EN" : "DIS");

	ccprintf("E-CR50 comm packet :\n");
	hexdump((uint8_t *)ec_packet_ctx.packet, CR50_COMM_MAX_PACKET_SIZE);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_comm, command_ec_comm, NULL,
			"Dump EC-CR50-comm info");
#endif  /* CR50_RELAXED */
