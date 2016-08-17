/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inter-Processor Communication module for Rotor MCU */

#include "atomic.h"
#include "battery.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "ipc.h"
#include "panic.h" /* for PANIC_DATA_PTR */
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/*
 * The basic flow to send messages to partner cores is the following:
 *
 * For a given channel,
 *  - Before sending a message, check to see that TX is allowed.
 *  - Write the data to the IPC data write register.
 *  - Block further transmissions of messages to the partner core on the
 *    channel. (disallow TX)
 *  - Set appropriate bit in interrupt set register to notify the
 *    the recipient that a new message is waiting.
 *
 * When a core receives a message, the core should acknowledge the receipt of
 * the message by writing the appropriate bit in the interrupt set register.
 * When the partner core sees this, it can reuse that channel to send another
 * message.  The sending core should allow TX for this channel now.
 */

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/*
 * IPC shared memory buffers
 *
 * There are 2 buffers per IPC channel, 1 buffer per side.  These buffers are
 * placed just before the panic data pointer.
 */
#define IPC_SHMEM_BUF_SIZE (ROTOR_MCU_NUM_IPC_CH * 2 * ROTOR_MCU_IPC_BUF_LEN)
#define IPC_SHMEM_BUF_PTR ((uint8_t *)PANIC_DATA_PTR - IPC_SHMEM_BUF_SIZE)

/* Pointer to host command request buffer from partner core. */
static uint32_t *hc_req[ROTOR_MCU_NUM_IPC_CH];
/* Pointers to host command response buffer. */
static uint32_t *hc_resp[ROTOR_MCU_NUM_IPC_CH];
/* IPC host packets */
static struct host_packet ipc_pkt[ROTOR_MCU_NUM_IPC_CH];

/* Bitmap indicating whether we can send a message to the receiving CPU. */
static uint32_t tx_allowed_map;

/* The last IPC channel we served a host request from. */
static uint8_t last_served_ch = -1;

/* Bitmap indicating pending host command request channels. */
static uint32_t pending_ch;

/*
 * Flag indicating if we're currently processing a host command.  This will be
 * cleared once the host command response is sent.
 */
static uint32_t hc_in_prog;

#define IPC_BLOCK_TIMEOUT (1 * MSEC)

static void ipc_recv_hc(uint8_t channel);

/**
 * Check if TX are allowed on a channel.
 *
 * @param channel	IPC channel
 *
 * @return 1 if TX allowed, 0 if TX not allowed.
 */
static int tx_allowed(int channel)
{
	return (tx_allowed_map & (1 << channel)) ? 1 : 0;
}

uint32_t *get_ipc_buffer(uint8_t channel)
{
	return (uint32_t *)(IPC_SHMEM_BUF_PTR +
			    (ROTOR_MCU_IPC_BUF_LEN * 2 * channel) +
			    ROTOR_MCU_IPC_BUF_LEN);
}

/**
 * Read data from read only registers.
 *
 * Note, data must be latched before reading these registers.  Make sure to
 * write to the dummy register to do this.
 *
 * @param channel	IPC channel.
 * @param r0		Address of destination for read register 0.
 * @param r1		Address of destination for read register 1.
 */
static void read_words(uint8_t channel, uint32_t *r0, uint32_t *r1)
{
	if (r0)
		*r0 = ROTOR_MCU_IPC_RDR_0(channel);
	if (r1)
		*r1 = ROTOR_MCU_IPC_RDR_1(channel);

#ifdef CONFIG_BRINGUP
	CPRINTS("IPC rd: ch%d r0(%08x) r1(%08x)", channel, *r0, *r1);
#endif /* defined(CONFIG_BRINGUP) */
}

/**
 * Write words to be transferred to the other processor.
 *
 * @param channel	IPC channel.
 * @param r0		Value for register 0.
 * @param r1		Value for register 1.
 */
static void write_words(uint8_t channel, uint32_t r0, uint32_t r1)
{
	ROTOR_MCU_IPC_WDR_0(channel) = r0;
	ROTOR_MCU_IPC_WDR_1(channel) = r1;

#ifdef CONFIG_BRINGUP
	CPRINTS("IPC wr: ch%d r0(%08x) r1(%08x)", channel, r0, r1);
#endif /* defined(CONFIG_BRINGUP) */
}

int send_message(uint8_t channel, uint32_t *src, uint8_t bit)
{
	int timeout;

	/* Check for valid IPC channel. */
	if ((channel >= ROTOR_MCU_NUM_IPC_CH) || (bit > 10))
		return EC_ERROR_INVAL;

	/*
	 * Basically, the user has to have their data already available in a
	 * buffer, we then copy the address to R0 and then set a bit in the
	 * register to let them know.  Or we could do it the polling way and
	 * just XOR or something... make it a config option?
	 */

	/*
	 * If TX is not allowed, we shouldn't be sending the message.  Maybe
	 * just sleep in the meantime.  Also, send a timeout or something.
	 */
	timeout = 5;
	while (!tx_allowed(channel)) {
		usleep(IPC_BLOCK_TIMEOUT);
		timeout--;

		if (timeout <= 0) {
			CPRINTS("IPC Timed out waiting for ACK from ch%d",
				channel);
			return EC_ERROR_TIMEOUT;
		}
	};

	/* Place the pointer to the message in WDR 0. */
	write_words(channel, (uint32_t)src, 0);

	/*
	 * Since we are going to send a message to the partner core, block
	 * further transmission to it until we hear an ACK back from the partner
	 * core.  We have to do this before sending our message otherwise, a
	 * race condition could occur when the partner core sends an ACK back
	 * before we clear the bit in our map.  When we would then return, we
	 * would clear the bit and would be blocked from sending new messages.
	 */
	tx_allowed_map &= ~(1 << channel);

	/*
	 * Set appropriate bit in interrupt set register to notify the recipient
	 * that a message is waiting.
	 */
	ROTOR_MCU_IPC_ISRW(channel) = (1 << bit);
#ifdef CONFIG_BRINGUP
	CPRINTS("IPC set bit %d ch%d", bit, channel);
#endif /* defined(CONFIG_BRINGUP) */

#ifdef CONFIG_BRINGUP
		CPRINTS("IPC TX OK: %s%s%s",
			(tx_allowed_map & (1 << 0)) ? "AP " : "",
			(tx_allowed_map & (1 << 1)) ? "APMU " : "",
			(tx_allowed_map & (1 << 2)) ? "SP " : "");
#endif /* defined(CONFIG_BRINGUP) */

	return EC_SUCCESS;
}

/* Host command send response functions. */
/**
 * Sends a host command response to the partner core via IPC.
 *
 * @param channel	IPC channel to send the response through.
 * @param pkt		pointer to response host_packet.
 */
static void ipc_send_hc_response(uint8_t channel, struct host_packet *pkt)
{
	uint8_t i, c;

	/*
	 * The response should already be in the shared buffer, so we'll just
	 * need to write the address of the buffer to WDR 0 and send the message
	 * back to the partner core.
	 */

	/* Ignore host command in progress since we have our own IPC channel. */
	if (pkt->driver_result == EC_RES_IN_PROGRESS)
		return;

	send_message(channel, (uint32_t *)pkt->response, 0);

	/*
	 * Now that we've finished processing the message and are sending back a
	 * response, we can send our ACK back.
	 */
	ROTOR_MCU_IPC_ISRW(channel) = (1 << 3);

	/* Clear the host command in progress flag. */
	atomic_clear(&hc_in_prog, 1);

	/*
	 * If there are pending host commands, go back and service them now in a
	 * round robin fashion.
	 */
	while (pending_ch != 0) {
		for (i = 0; i < ROTOR_MCU_NUM_IPC_CH; i++) {
			c = (last_served_ch + 1 + i) % ROTOR_MCU_NUM_IPC_CH;
			if (pending_ch & (1 << c))
				ipc_recv_hc(c);
		}
	};
}

static void ap_ipc_send_hc_response(struct host_packet *pkt)
{
	ipc_send_hc_response(ROTOR_MCU_AP_IPC, pkt);
}
static void apmu_ipc_send_hc_response(struct host_packet *pkt)
{
	ipc_send_hc_response(ROTOR_MCU_APMU_IPC, pkt);
}
static void sp_ipc_send_hc_response(struct host_packet *pkt)
{
	ipc_send_hc_response(ROTOR_MCU_SP_IPC, pkt);
}

void (*ipc_send_hc_response_funcs[])(struct host_packet *pkt) = {
	ap_ipc_send_hc_response,
	apmu_ipc_send_hc_response,
	sp_ipc_send_hc_response
};

/**
 * Receive a host command to the host command stack from an IPC channel.
 *
 * @param channel	IPC channel where a host command message is.
 */
static void ipc_recv_hc(uint8_t channel)
{
	if (channel < ROTOR_MCU_NUM_IPC_CH)
		atomic_or(&pending_ch, 1 << channel);

	/*
	 * If we're currently processing a host command, hold off on this one
	 * for now.  It's been marked as pending, so we'll get back to it after
	 * we finish processing this one.
	 */
	if (hc_in_prog)
		return;

	atomic_or(&hc_in_prog, 1);
	atomic_clear(&pending_ch, 1 << channel);
	last_served_ch = channel;

	/*
	 * These should be host command messages.  The RDR 0 will have the
	 * address of the buffer which should contain the host command request.
	 */
	read_words(channel, (uint32_t *)&hc_req[channel], NULL);

	/* Fill out the host packet. */
	memset(&ipc_pkt[channel], 0, sizeof(struct host_packet));
	ipc_pkt[channel].send_response = ipc_send_hc_response_funcs[channel];
	ipc_pkt[channel].request = hc_req[channel];
	ipc_pkt[channel].request_temp = NULL;
	ipc_pkt[channel].request_max = ROTOR_MCU_IPC_BUF_LEN;
	ipc_pkt[channel].request_size = ROTOR_MCU_IPC_BUF_LEN;

	/* Grab our IPC shared memory buffer to send our response in. */
	hc_resp[channel] = get_ipc_buffer(channel);
	ipc_pkt[channel].response = (void *)hc_resp[channel];
	ipc_pkt[channel].response_max = ROTOR_MCU_IPC_BUF_LEN;
	ipc_pkt[channel].response_size = 0;

	/* We only support host command protocol 3 and newer. */
	if (*hc_req[channel] >= EC_COMMAND_PROTOCOL_3)
		ipc_pkt[channel].driver_result = EC_RES_SUCCESS;
	else
		ipc_pkt[channel].driver_result = EC_RES_INVALID_HEADER;

	host_packet_receive(&ipc_pkt[channel]);
}

/**
 * Handle an IPC interrupt.
 *
 * @param channel	IPC channel where interrupt fired.
 */
void ipc_irq(uint8_t channel)
{
	uint32_t iir;

	/*
	 * Perform a write to the dummy register to latch the data in the
	 * registers.
	 */
	ROTOR_MCU_IPC_DUMMY(channel) = 1;

	/* Identify the cause of the interrupt. */
	iir = ROTOR_MCU_IPC_IIR(channel) & 0x7FF;

#ifdef CONFIG_BRINGUP
	CPRINTS("IPC IRQ src_ch: %d IIR(%03x)", channel, iir);
#endif /* defined(CONFIG_BRINGUP) */

	/* Clear the interrupt. */
	ROTOR_MCU_IPC_ICR(channel) = iir;

	if (iir & 0x8) {
		/* Allow transmissions on this channel now. */
		tx_allowed_map |= (1 << channel);
#ifdef CONFIG_BRINGUP
		CPRINTS("IPC TX OK: %s%s%s",
			(tx_allowed_map & (1 << 0)) ? "AP " : "",
			(tx_allowed_map & (1 << 1)) ? "APMU " : "",
			(tx_allowed_map & (1 << 2)) ? "SP " : "");
#endif /* defined(CONFIG_BRINGUP) */
	}

	if (iir & 0x1)
		/*
		 * We've received a new message.  It should be a host command
		 * message.
		 */
		ipc_recv_hc(channel);
}

void ap_ipc_irq(void)
{
	task_clear_pending_irq(ROTOR_MCU_IRQ_IPC_WU_AP);
	ipc_irq(ROTOR_MCU_AP_IPC);
}

void apmu_ipc_irq(void)
{
	task_clear_pending_irq(ROTOR_MCU_IRQ_IPC_WU_APMU);
	ipc_irq(ROTOR_MCU_APMU_IPC);
}

void sp_ipc_irq(void)
{
	task_clear_pending_irq(ROTOR_MCU_IRQ_IPC_WU_SP);
	ipc_irq(ROTOR_MCU_SP_IPC);
}
DECLARE_IRQ(ROTOR_MCU_IRQ_IPC_WU_AP, ap_ipc_irq, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_IPC_WU_APMU, apmu_ipc_irq, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_IPC_WU_SP, sp_ipc_irq, 2);

/**
 * Prepare IPC state for communication.
 */
static void ipc_init(void)
{
	/* Make sure that we can talk to any core after init. */
	tx_allowed_map = 0x7;
	task_enable_irq(ROTOR_MCU_IRQ_IPC_WU_AP);
	task_enable_irq(ROTOR_MCU_IRQ_IPC_WU_APMU);
	task_enable_irq(ROTOR_MCU_IRQ_IPC_WU_SP);
	CPRINTS("IPC init done");
#ifdef CONFIG_BRINGUP
	CPRINTS("IPC TX OK: %s%s%s",
		(tx_allowed_map & (1 << 0)) ? "AP " : "",
		(tx_allowed_map & (1 << 1)) ? "APMU " : "",
		(tx_allowed_map & (1 << 2)) ? "SP " : "");
#endif /* defined(CONFIG_BRINGUP) */
}
DECLARE_HOOK(HOOK_INIT, ipc_init, HOOK_PRIO_DEFAULT);

/******************************************************************************/
/* Host Commands */

/**
 * Get protocol information.
 */
static int ipc_get_protocol_info(struct host_cmd_handler_args *args)
{
	struct ec_response_get_protocol_info *r = args->response;

	memset(r, 0, sizeof(*r));
	/* We only support protocol 3+ */
	r->protocol_versions = (1 << 3);
	r->max_request_packet_size = ROTOR_MCU_IPC_BUF_LEN;
	r->max_response_packet_size = ROTOR_MCU_IPC_BUF_LEN;
	r->flags = 0;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO, ipc_get_protocol_info,
		     EC_VER_MASK(0));
