/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IPC module for ISH */
#include "registers.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "lpc.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "ipc.h"

#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)

#define IPC_PROTOCOL_SSP  5

ipc_if_ctx_t ipc_ctx[IPC_PEERS_TOTAL]  = {
	[IPC_PEER_HOST] = {
		.in_msg_reg = IPC_HOST2ISH_MSG_REGS,
		.out_msg_reg = IPC_ISH2HOST_MSG_REGS,
		.in_drbl_reg = IPC_HOST2ISH_DOORBELL,
		.out_drbl_reg = IPC_ISH2HOST_DOORBELL,
		.clr_bit = IPC_INT_ISH2HOST_CLR_BIT,
		.irq_in = ISH30_IPC_HOST2ISH_IRQ,
		.irq_clr = ISH30_IPC_ISH2HOST_CLR_IRQ,
		.peer_id = IPC_PEER_HOST
	},
	/* More peers later */
};

static uint8_t mem_mapped[0x200] __attribute__ ((section(".bss.big_align")));

static uint32_t host_events;	/* Currently pending SCI/SMI events */
static uint32_t event_mask[3];	/* Event masks for each type */
static struct host_packet ipc_packet;
static struct host_cmd_handler_args host_cmd_args;
static uint8_t host_cmd_flags;	/* Flags from host command */

static uint8_t params_copy[EC_LPC_HOST_PACKET_SIZE] __aligned(4);

static struct ec_lpc_host_args *const ipc_host_args =
	(struct ec_lpc_host_args *)mem_mapped;

uint8_t new_pimr_bit_arrr[IPC_PEERS_TOTAL][3] = {
	{
		IPC_PIMR_HOST2ISH_OFFS,
		IPC_PIMR_HOST2ISH_OFFS,
		IPC_PIMR_ISH2HOST_CLR_OFFS
	}
};


/*
 * Most registers in LPC module are reset when the host is off. We need to
 * set up LPC again when the host is starting up.
 */

/*
 * Set prio to higher than default; this way LPC memory mapped data is ready
 * before other inits try to initialize their memmap data.
 */

int lpc_query_host_event_state(void)
{
	const uint32_t any_mask = event_mask[0] | event_mask[1] | event_mask[2];
	int evt_index = 0;
	int i;

	for (i = 0; i < 32; i++) {
		const uint32_t e = (1 << i);

		if (host_events & e) {
			host_clear_events(e);

			/*
			 * If host hasn't unmasked this event, drop it.  We do
			 * this at query time rather than event generation time
			 * so that the host has a chance to unmask events
			 * before they're dropped by a query.
			 */
			if (!(e & any_mask))
				continue;

			evt_index = i + 1;	/* Events are 1-based */
			break;
		}
	}

	return evt_index;
}

/* Get protocol information */
static int ipc_get_protocol_info(struct host_cmd_handler_args *args)
{
	struct ec_response_get_protocol_info *r = args->response;

	memset(r, 0, sizeof(*r));
	r->protocol_versions = (1 << 3);
	r->max_request_packet_size = EC_LPC_HOST_PACKET_SIZE;
	r->max_response_packet_size = EC_LPC_HOST_PACKET_SIZE;
	r->flags = 0;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO, ipc_get_protocol_info,
		     EC_VER_MASK(0));

void ipc_set_pimr(uint8_t peer_id, int set, PIMR_SIGNAL_TYPE signal_type)
{
	uint32_t new_pimr_val;

	new_pimr_val = (1 << (new_pimr_bit_arrr[peer_id][signal_type]));

	interrupt_disable();

	if (set)
		new_pimr_val |= REG32(IPC_PIMR);
	else
		new_pimr_val = (REG32(IPC_PIMR)) & (~new_pimr_val);

	REG32(IPC_PIMR) = new_pimr_val;
	interrupt_enable();
}

uint8_t IPC_PROTOCOL_BY_IF(uint8_t peer_id)
{
	(void)peer_id;
	return IPC_PROTOCOL_SSP;
}

int ipc_wait_until_msg_consumed(ipc_if_ctx_t *ctx, int timeout)
{
	int wait_sts = 0;
	uint32_t drbl;

	drbl = REG32(ctx->out_drbl_reg);
	if (!(drbl & IPC_DRBL_BUSY_BIT)) {
		/* doorbell is already cleared. we can continue */
		return 0;
	}

	while (1) {
		wait_sts = task_wait_event_mask(EVENT_FLAG_BIT_WRITE_IPC, -1);
		drbl = REG32(ctx->out_drbl_reg);

		if (!(drbl & IPC_DRBL_BUSY_BIT)) {
			return 0;
		} else if (wait_sts != 0) {
			/* timeout */
			return wait_sts;
		}
	}
}

int ipc_read(uint8_t peer_id, void *out_buff, uint32_t buff_size)
{
#ifdef ISH_DEBUG
	int i;
#endif
	ipc_if_ctx_t *ctx;
	ipc_oob_msg_t *oob_msg;
	int retval = EC_SUCCESS;
	uint32_t drbl_val, msg_address, read_length;

	ctx = &ipc_ctx[peer_id];
	drbl_val = REG32(ctx->in_drbl_reg);
	read_length = IPC_HEADER_GET_LENGTH(drbl_val);

	if (drbl_val & IPC_OOB_MSG_BIT) {
		if (read_length != sizeof(ipc_oob_msg_t))
			retval = IPC_FAILURE;
		oob_msg = (ipc_oob_msg_t *) ctx->in_msg_reg;
		msg_address = oob_msg->address;
		read_length = oob_msg->length;
	} else {
		if (read_length > IPC_MSG_MAX_SIZE)
			retval = IPC_FAILURE;
		msg_address = ctx->in_msg_reg;
	}

	if (read_length > buff_size)
		retval = IPC_FAILURE;

	if (retval >= 0) {
		/* Copy message to out buffer. */
		memcpy(out_buff, (const uint32_t *)msg_address,
			 read_length);
		retval = read_length;

#ifdef ISH_DEBUG
		CPRINTF("ipc_read, len=0x%0x [", read_length);
		for (i = 0; i < read_length; i++)
			CPRINTF("0x%0x ", (uint8_t)((char *)out_buff)[i]);
		CPUTS("]\n");
#endif
	}

	REG32(ctx->in_drbl_reg) = 0;
	ipc_set_pimr(ctx->peer_id, SET_PIMR, PIMR_SIGNAL_IN);

	return retval;
}

int ipc_write(uint8_t peer_id, void *buff, uint32_t buff_size)
{
	ipc_if_ctx_t *ctx;
	uint32_t drbl_val = 0;
	int retval = IPC_FAILURE;
#ifdef ISH_DEBUG
	int i;
#endif

	ctx = &ipc_ctx[peer_id];

	retval = ipc_wait_until_msg_consumed(ctx, IPC_TIMEOUT);
	if (retval != 0) {
		/* timeout */
		return IPC_FAILURE;
	}

#ifdef ISH_DEBUG
	CPRINTF("ipc_write, len=0x%0x [", buff_size);
	for (i = 0; i < buff_size; i++)
		CPRINTF("0x%0x ", (uint8_t)((char *)buff)[i]);
	CPUTS("]\n");
#endif

	/* write message */
	if (buff_size <= IPC_MSG_MAX_SIZE) {
		/* write to message register */
		memcpy((uint32_t *) ctx->out_msg_reg, buff,
			 buff_size);
		drbl_val = IPC_BUILD_HEADER(buff_size, 4, 1);
	} else {
		/* write out-of-band message */
		ipc_oob_msg_t oob_msg = { (uint32_t) buff, buff_size };

		memcpy((uint32_t *) ctx->out_msg_reg, &oob_msg,
				sizeof(oob_msg));
		drbl_val = IPC_BUILD_HEADER(sizeof(oob_msg),
			    IPC_PROTOCOL_BY_IF(ctx->peer_id), 1);
		drbl_val |= IPC_OOB_MSG_BIT;
	}

	/* write doorbell */
	REG32(ctx->out_drbl_reg) = drbl_val;
	retval = buff_size;

	return retval;
}

uint8_t *lpc_get_memmap_range(void)
{
	return mem_mapped + 0x100;
}

static uint8_t *ipc_get_hostcmd_data_range(void)
{
	return mem_mapped;
}

static void ipc_send_response_packet(struct host_packet *pkt)
{
	ipc_write(ISH_HOST_PEER_ID, pkt->response, pkt->response_size);
}

void lpc_set_host_event_state(uint32_t mask)
{
}

void lpc_set_host_event_mask(enum lpc_host_event_type type, uint32_t mask)
{
}

uint32_t lpc_get_host_event_mask(enum lpc_host_event_type type)
{
	return event_mask[type];
}

static void setup_ipc(void)
{
	uint32_t out_drbl;

	CPRINTS("setup_ipc");

	out_drbl = REG32(IPC_HOST2ISH_DOORBELL);
	if (!IPC_IS_BUSY(out_drbl))
		task_set_event(-1, EVENT_FLAG_BIT_WRITE_IPC, 0);

	task_enable_irq(ISH30_IPC_HOST2ISH_IRQ);
	task_enable_irq(ISH30_IPC_ISH2HOST_CLR_IRQ);

	ipc_set_pimr(ISH_HOST_PEER_ID, true, PIMR_SIGNAL_IN);
	ipc_set_pimr(ISH_HOST_PEER_ID, true, PIMR_SIGNAL_CLR);
}

DECLARE_HOOK(HOOK_CHIPSET_STARTUP, setup_ipc, HOOK_PRIO_FIRST);

static void ipc_init(void)
{

	CPRINTS("ipc_init");

	/* Initialize host args and memory map to all zero */
	memset(ipc_host_args, 0, sizeof(*ipc_host_args));
	memset(lpc_get_memmap_range(), 0, EC_MEMMAP_SIZE);

	setup_ipc();
}

DECLARE_HOOK(HOOK_INIT, ipc_init, HOOK_PRIO_INIT_LPC);

/* On boards without a host, this command is used to set up LPC */
static int ipc_command_init(int argc, char **argv)
{
	ipc_init();
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(ipcinit, ipc_command_init, NULL, NULL, NULL);

void ipc_interrupt_handler(void)
{
	uint32_t pisr = REG32(IPC_PISR);
	uint32_t pimr = REG32(IPC_PIMR);
	uint32_t busy_clear = REG32(IPC_BUSY_CLEAR);
	uint32_t drbl = REG32(IPC_ISH2HOST_MSG_REGS);
	uint8_t proto, cmd;

	if ((pisr & IPC_PISR_HOST2ISH_BIT)
	    && (pimr & IPC_PIMR_HOST2ISH_BIT)) {

		/* New message arrived */
		ipc_set_pimr(ISH_HOST_PEER_ID, false, PIMR_SIGNAL_IN);
		task_set_event(TASK_ID_IPC_COMM, EVENT_FLAG_BIT_READ_IPC, 0);
		proto = IPC_HEADER_GET_PROTOCOL(drbl);
		cmd = IPC_HEADER_GET_MNG_CMD(drbl);

		if ((proto == IPC_PROTOCOL_MNG) && (cmd == MNG_TIME_UPDATE))
			/* Ignoring time update from host */
			;
	}

	if ((busy_clear & IPC_INT_ISH2HOST_CLR_BIT)
	    && (pimr & IPC_PIMR_ISH2HOST_CLR_MASK_BIT)) {
		/* Written message cleared */
		REG32(IPC_BUSY_CLEAR) = IPC_ISH_MINIMA_FWSTS;
		task_set_event(TASK_ID_IPC_COMM, EVENT_FLAG_BIT_WRITE_IPC, 0);
	}
}

DECLARE_IRQ(ISH30_IPC_HOST2ISH_IRQ, ipc_interrupt_handler);

void ipc_comm_task(void)
{

	int ret = 0;
	uint32_t out_drbl, pkt_len;

	for (;;) {

		ret = task_wait_event_mask(EVENT_FLAG_BIT_READ_IPC
				| EVENT_FLAG_BIT_WRITE_IPC, -1);

		if ((ret & EVENT_FLAG_BIT_WRITE_IPC))
			continue;
		else if (!(ret & EVENT_FLAG_BIT_READ_IPC))
			continue;

		/* Read the command byte.  This clears the FRMH bit in
		 * the status byte. */
		out_drbl = REG32(IPC_HOST2ISH_DOORBELL);
		pkt_len = out_drbl & 0x3FF;

		ret = ipc_read(ISH_HOST_PEER_ID, ipc_host_args, pkt_len);
		host_cmd_args.command = EC_COMMAND_PROTOCOL_3;

		host_cmd_args.result = EC_RES_SUCCESS;
		host_cmd_flags = ipc_host_args->flags;

		/* We only support new style command (v3) now */
		if (host_cmd_args.command == EC_COMMAND_PROTOCOL_3) {
			ipc_packet.send_response = ipc_send_response_packet;

			ipc_packet.request =
			    (const void *)ipc_get_hostcmd_data_range();
			ipc_packet.request_temp = params_copy;
			ipc_packet.request_max = sizeof(params_copy);
			/* Don't know the request size so pass in
			 * the entire buffer */
			ipc_packet.request_size = EC_LPC_HOST_PACKET_SIZE;

			ipc_packet.response =
			    (void *)ipc_get_hostcmd_data_range();
			ipc_packet.response_max = EC_LPC_HOST_PACKET_SIZE;
			ipc_packet.response_size = 0;

			ipc_packet.driver_result = EC_RES_SUCCESS;
			host_packet_receive(&ipc_packet);
			usleep(10); /* To force yield */

			continue;
		} else {
			/* Old style command unsupported */
			host_cmd_args.result = EC_RES_INVALID_COMMAND;
		}

		/* Hand off to host command handler */
		host_command_received(&host_cmd_args);
	}
}

void lpc_clear_acpi_status_mask(uint8_t mask)
{
}

