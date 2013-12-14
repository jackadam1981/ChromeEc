/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPC module for Chrome EC */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lpc.h"
#include "port80.h"
#include "registers.h"
#include "task.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)

#define PNPCFG_ADDR_PORT	0x00
#define PNPCFG_DATA_PORT	0x01

/* Timeout in loop counts for EC2I wait loops. */
#define EC2I_LOOP_CNT_TIMEOUT	1000

/* Base address for shared RAM. */
#define LPC_POOL_BASE             0x80000

/* LPC pool offsets. */
#define LPC_POOL_OFFS_HOST_CMD	  (EC_LPC_ADDR_HOST_CMD & 0xfff)
#define LPC_POOL_OFFS_HOST_STATUS (EC_LPC_ADDR_HOST_STATUS & 0xfff)
#define LPC_POOL_OFFS_HOST_DATA   (EC_LPC_ADDR_HOST_DATA & 0xfff)
#define LPC_POOL_OFFS_MEMMAP      (EC_LPC_ADDR_MEMMAP & 0xfff)
#define LPC_POOL_OFFS_CMD_DATA    (EC_LPC_ADDR_HOST_PACKET & 0xfff)

/* LPC pool data pointers. */
#define LPC_POOL_HOST_CMD         (LPC_POOL_BASE + LPC_POOL_OFFS_HOST_CMD)
#define LPC_POOL_HOST_STATUS      (LPC_POOL_BASE + LPC_POOL_OFFS_HOST_STATUS)
#define LPC_POOL_HOST_DATA        (LPC_POOL_BASE + LPC_POOL_OFFS_HOST_DATA)
#define LPC_POOL_MEMMAP           (LPC_POOL_BASE + LPC_POOL_OFFS_MEMMAP)
#define LPC_POOL_CMD_DATA         (LPC_POOL_BASE + LPC_POOL_OFFS_CMD_DATA)

#define LPC_HOST_CMD_REG          (*(uint8_t *)LPC_POOL_HOST_CMD)
#define LPC_HOST_STATUS_REG       (*(uint8_t *)LPC_POOL_HOST_STATUS)
#define LPC_HOST_DATA_REG         (*(uint8_t *)LPC_POOL_HOST_DATA)

/* Host status register bits. */
#define EC_LPC_CMDR_PENDING	(1 << 1)  /* Write pending to EC */
#define EC_LPC_CMDR_BUSY	(1 << 2)  /* EC is busy processing a command */

static uint32_t host_events;     /* Currently pending SCI/SMI events */
static uint32_t event_mask[3];   /* Event masks for each type */
static struct host_packet lpc_packet;
static struct host_cmd_handler_args host_cmd_args;
static uint8_t host_cmd_flags;   /* Flags from host command */

/* Params must be 32-bit aligned */
static uint8_t params_copy[EC_LPC_HOST_PACKET_SIZE] __aligned(4);

static uint8_t * const cmd_params = (uint8_t *)LPC_POOL_CMD_DATA +
	EC_LPC_ADDR_HOST_PARAM - EC_LPC_ADDR_HOST_ARGS;
static struct ec_lpc_host_args * const lpc_host_args =
	(struct ec_lpc_host_args *)LPC_POOL_CMD_DATA;

/**
 * Write to host PNPCFG registers.
 */
static int ec2i_write_host_pnpcfg(int addr, int data)
{
	int timeout_counter;

	/* Disable host access to I-bus. */
	IT83XX_EC2I_LSIOHA |= 0x01;

	/* Enable EC access to I-bus. */
	IT83XX_EC2I_IBCTL = 0x01;

	/* Make sure no writes or reads are in progress. */
	timeout_counter = 0;
	while (IT83XX_EC2I_IBCTL & 0x06)
		if (timeout_counter++ > EC2I_LOOP_CNT_TIMEOUT)
			return EC_ERROR_TIMEOUT;

	/* Provide EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x01;

	/* Set address and data. */
	IT83XX_EC2I_IHIOA = addr;
	IT83XX_EC2I_IHD = data;

	/* Wait for write to complete. */
	timeout_counter = 0;
	while (IT83XX_EC2I_IBCTL & 0x04)
		if (timeout_counter++ > EC2I_LOOP_CNT_TIMEOUT)
			return EC_ERROR_TIMEOUT;

	/* Disable EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x00;

	/* Disable EC access to I-bus. */
	IT83XX_EC2I_IBCTL &= ~0x01;

	/* Enable host access to I-bus. */
	IT83XX_EC2I_LSIOHA &= ~0x01;

	return EC_SUCCESS;
}

/**
 * Read from host PNPCFG registers.
 */
static int ec2i_read_host_pnpcfg(int addr, int *data)
{
	int timeout_counter;

	/* Disable host access to I-bus. */
	IT83XX_EC2I_LSIOHA |= 0x01;

	/* Enable EC access to I-bus. */
	IT83XX_EC2I_IBCTL = 0x01;

	/* Make sure no writes or reads are in progress. */
	timeout_counter = 0;
	while (IT83XX_EC2I_IBCTL & 0x06)
		if (timeout_counter++ > EC2I_LOOP_CNT_TIMEOUT)
			return EC_ERROR_TIMEOUT;

	/* Set address. */
	IT83XX_EC2I_IHIOA = addr;

	/* Provide EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x01;

	/* Enable EC access to I-bus and initiate a read. */
	IT83XX_EC2I_IBCTL |= 0x03;

	/* Wait for read to complete. */
	timeout_counter = 0;
	while (IT83XX_EC2I_IBCTL & 0x02)
		if (timeout_counter++ > EC2I_LOOP_CNT_TIMEOUT)
			return EC_ERROR_TIMEOUT;

	/* Read the data. */
	*data = IT83XX_EC2I_IHD;

	/* Disable EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x00;

	/* Disable EC access to I-bus. */
	IT83XX_EC2I_IBCTL &= ~0x01;

	/* Enable host access to I-bus. */
	IT83XX_EC2I_LSIOHA &= ~0x01;

	return EC_SUCCESS;
}

uint8_t *lpc_get_memmap_range(void)
{
	return (uint8_t *)LPC_POOL_MEMMAP;
}

/**
 * Update the host event status.
 *
 * Sends a pulse if masked event status becomes non-zero:
 *   - SMI pulse via EC_SMI_L GPIO
 *   - SCI pulse via LPC0SCI
 */
static void update_host_event_status(void)
{
	return;
}

void lpc_set_host_event_state(uint32_t mask)
{
	if (mask != host_events) {
		host_events = mask;
		update_host_event_status();
	}
}

int lpc_query_host_event_state(void)
{
	int evt_index = 0;
	int i;

	for (i = 0; i < 32; i++) {
		if (host_events & (1 << i)) {
			host_clear_events(1 << i);
			evt_index = i + 1;	/* Events are 1-based */
			break;
		}
	}

	return evt_index;
}


void lpc_set_host_event_mask(enum lpc_host_event_type type, uint32_t mask)
{
	event_mask[type] = mask;
	update_host_event_status();
}

uint32_t lpc_get_host_event_mask(enum lpc_host_event_type type)
{
	return event_mask[type];
}


static void lpc_send_response(struct host_cmd_handler_args *args)
{
	uint8_t *out;
	int size = args->response_size;
	int csum;
	int i;

	/* Ignore in-progress on LPC since interface is synchronous anyway. */
	if (args->result == EC_RES_IN_PROGRESS)
		return;

	/* Handle negative size. */
	if (size < 0) {
		args->result = EC_RES_INVALID_RESPONSE;
		size = 0;
	}

	/* New-style response. */
	lpc_host_args->flags =
		(host_cmd_flags & ~EC_HOST_ARGS_FLAG_FROM_HOST) |
		EC_HOST_ARGS_FLAG_TO_HOST;

	lpc_host_args->data_size = size;

	csum = args->command + lpc_host_args->flags +
		lpc_host_args->command_version +
		lpc_host_args->data_size;

	for (i = 0, out = (uint8_t *)args->response; i < size; i++, out++)
		csum += *out;

	lpc_host_args->checksum = (uint8_t)csum;

	/* Fail if response doesn't fit in the param buffer. */
	if (size > EC_PROTO2_MAX_PARAM_SIZE)
		args->result = EC_RES_INVALID_RESPONSE;

	/* Write result to the data byte. */
	LPC_HOST_DATA_REG = args->result;

	/* Clear the busy bit, so the host knows the EC is done. */
	task_disable_irq(IT83XX_IRQ_H2RAM_LPC);
	LPC_HOST_STATUS_REG &= ~EC_LPC_CMDR_BUSY;
	task_enable_irq(IT83XX_IRQ_H2RAM_LPC);
}

static void lpc_send_response_packet(struct host_packet *pkt)
{
	/* Ignore in-progress on LPC since interface is synchronous anyway */
	if (pkt->driver_result == EC_RES_IN_PROGRESS)
		return;

	/* Write result to the data byte. */
	LPC_HOST_DATA_REG = pkt->driver_result;

	/* Clear the busy bit, so the host knows the EC is done. */
	task_disable_irq(IT83XX_IRQ_H2RAM_LPC);
	LPC_HOST_STATUS_REG &= ~EC_LPC_CMDR_BUSY;
	task_enable_irq(IT83XX_IRQ_H2RAM_LPC);
}

/**
 * Handle write to host command I/O ports.
 */
static void handle_host_write(void)
{
	/* Set the busy bit in the status byte, then clear pending bit. */
	LPC_HOST_STATUS_REG |= EC_LPC_CMDR_BUSY;
	LPC_HOST_STATUS_REG &= ~EC_LPC_CMDR_PENDING;

	/* Read in the command. */
	host_cmd_args.command = LPC_HOST_CMD_REG;

	host_cmd_args.result = EC_RES_SUCCESS;
	host_cmd_args.send_response = lpc_send_response;

	host_cmd_flags = lpc_host_args->flags;

	/* See if we have an old or new style command */
	if (host_cmd_args.command == EC_COMMAND_PROTOCOL_3) {
		lpc_packet.send_response = lpc_send_response_packet;

		lpc_packet.request = (const void *)LPC_POOL_CMD_DATA;
		lpc_packet.request_temp = params_copy;
		lpc_packet.request_max = sizeof(params_copy);
		/* Don't know the request size so pass in the entire buffer */
		lpc_packet.request_size = EC_LPC_HOST_PACKET_SIZE;

		lpc_packet.response = (void *)LPC_POOL_CMD_DATA;
		lpc_packet.response_max = EC_LPC_HOST_PACKET_SIZE;
		lpc_packet.response_size = 0;

		lpc_packet.driver_result = EC_RES_SUCCESS;
		host_packet_receive(&lpc_packet);
		return;

	} else if (host_cmd_flags & EC_HOST_ARGS_FLAG_FROM_HOST) {
		/* Version 2 (link) style command */
		int size = lpc_host_args->data_size;
		int csum, i;

		host_cmd_args.version = lpc_host_args->command_version;
		host_cmd_args.params = params_copy;
		host_cmd_args.params_size = size;
		host_cmd_args.response = cmd_params;
		host_cmd_args.response_max = EC_PROTO2_MAX_PARAM_SIZE;
		host_cmd_args.response_size = 0;

		/* Verify params size */
		if (size > EC_PROTO2_MAX_PARAM_SIZE) {
			host_cmd_args.result = EC_RES_INVALID_PARAM;
		} else {
			const uint8_t *src = cmd_params;
			uint8_t *copy = params_copy;

			/*
			 * Verify checksum and copy params out of LPC space.
			 * This ensures the data acted on by the host command
			 * handler can't be changed by host writes after the
			 * checksum is verified.
			 */
			csum = host_cmd_args.command +
				host_cmd_flags +
				host_cmd_args.version +
				host_cmd_args.params_size;

			for (i = 0; i < size; i++) {
				csum += *src;
				*(copy++) = *(src++);
			}

			if ((uint8_t)csum != lpc_host_args->checksum)
				host_cmd_args.result = EC_RES_INVALID_CHECKSUM;
		}
	} else {
		/* Old style command, now unsupported */
		host_cmd_args.result = EC_RES_INVALID_COMMAND;
	}

	/* Hand off to host command handler */
	host_command_received(&host_cmd_args);
}

void lpc_port80_interrupt(enum gpio_signal signal)
{
	int addr;
	int status = 0;

	/* Set active logical device to the RTCT. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x07);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x10);

	/* Read address of last port 0x80 write to BRAM. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0xf5);
	status |= ec2i_read_host_pnpcfg(PNPCFG_DATA_PORT, &addr);

	/* Read last port 0x80 data and store it. */
	if (status == EC_SUCCESS)
		port_80_write(REG8(IT83XX_BRAM_BASE + 128 + addr));
}

static void __host_write_irq(void)
{
	/*
	 * Clear interrupt by clearing the H2RAM host semaphore status for
	 * whichever channel caused the interrupt.
	 */
	IT83XX_SMFI_HRAMHSS |= IT83XX_SMFI_HRAMHSS;

	handle_host_write();
}
DECLARE_IRQ(IT83XX_IRQ_H2RAM_LPC, __host_write_irq, 1);

static void lpc_init(void)
{
	int status = 0;

	/*
	 * Specify not using LPCPD# or LPCRST# to reset PNPCFG.
	 * TODO: eventually, we probably want to use LPCPD.
	 */
	IT83XX_GCTRL_RSTS &= ~0x08;
	IT83XX_GPIO_GCR |= 0x06;

	/***************************************************/
	/* Host debug port (0x80) setup. */
	/* Enable Debug Port 80h. */
	IT83XX_GCTRL_SPCTRL1 |= 0xc0;

	/*
	 * Set pins E0 and E7 to be L80HLat so that we can trigger
	 * an interrupt when a port 0x80 command is received.
	 */
	gpio_set_alternate_function(GPIO_E, 1<<0, 1);

	/* Enable port 0x80 interrupts. */
	gpio_enable_interrupt(GPIO_L80HLAT);

	/***************************************************/
	/* Host UART (0x3f8-0x3ff) setup. */
	/* Set UART2 as active logical device. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x07);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x02);

	/* Move IO Descriptor 0 to 0x3f8. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x60);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x03);
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x61);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0xf8);

	/* Activate UART2 Plug and Play. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x30);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x01);

	if (status != EC_SUCCESS) {
		CPRINTF("[%T Failed to initialize LPC bus");
		return;
	}

	/***************************************************/
	/* Host memory mapped data (0xd100-0xd1ff) setup. */
	/* Enable Host RAM window 0 accessed by LPC I/O cycle. */
	IT83XX_SMFI_HRAMWC |= 0x11;

	/* Put Host window at the memmap offset and set size 256 bytes. */
	/* TODO: what if EC_MEMMAP_SIZE is changed? Throw compile error? */
	IT83XX_SMFI_HRAMW0BA = LPC_POOL_OFFS_MEMMAP >> 4;
	IT83XX_SMFI_HRAMW0AAS = 0x04;

	/* Set SMFI as active logical device. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x07);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x0f);

	/* LPC I/O path and host window address 0x0900. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0xf5);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0xd0);
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0xf6);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x01);

	/* Activate SMFI Plug and Play. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x30);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x01);

	if (status != EC_SUCCESS) {
		CPRINTF("[%T Failed to initialize LPC bus");
		return;
	}

	/* Initialize memory map to all zero. */
	memset(lpc_get_memmap_range(), 0, EC_MEMMAP_SIZE);

	/* We support LPC args and version 3 protocol. */
	*(lpc_get_memmap_range() + EC_MEMMAP_HOST_CMD_FLAGS) =
		EC_HOST_CMD_FLAG_LPC_ARGS_SUPPORTED |
		EC_HOST_CMD_FLAG_VERSION_3;

	/***************************************************/
	/* Host commands (0xd300/0xd301/0xd302 and 0xd200-0xd2ff) setup. */
	/* Enable Host RAM window 1 and 2 accessed by LPC I/O cycle. */
	IT83XX_SMFI_HRAMWC |= 0x16;

	/* Put Host window at the host command offset and size 16 bytes. */
	IT83XX_SMFI_HRAMW1BA = LPC_POOL_OFFS_HOST_CMD >> 4;
	IT83XX_SMFI_HRAMW1AAS = 0x00;

	/* Put Host window at the command data offset and size 256 bytes. */
	IT83XX_SMFI_HRAMW2BA = LPC_POOL_OFFS_CMD_DATA >> 4;
	IT83XX_SMFI_HRAMW2AAS = 0x04;

	/* Host semaphore 1 interrupt enable (for RAM window 1). */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0xf9);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x02);

	/*
	 * Place host semaphore at 0 so that we get an interrupt when
	 * 0xd300 is written.
	 */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0xfa);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x00);

	/* Clear interrupt status and enable interrupt. */
	task_clear_pending_irq(IT83XX_IRQ_H2RAM_LPC);
	task_enable_irq(IT83XX_IRQ_H2RAM_LPC);

	/* Activate RTCT Plug and Play. */
	status |= ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x30);
	status |= ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x01);

	if (status != EC_SUCCESS) {
		CPRINTF("[%T Failed to initialize LPC bus");
		return;
	}

	/* Initialize host args to all zero. */
	memset(lpc_host_args, 0, sizeof(*lpc_host_args));
}
/*
 * Set prio to higher than default; this way LPC memory mapped data is ready
 * before other inits try to initialize their memmap data.
 */
DECLARE_HOOK(HOOK_INIT, lpc_init, HOOK_PRIO_INIT_LPC);

/*****************************************************************************/
/* Console commands */

static int command_memmap(int argc, char **argv)
{
	uint32_t val, offset;
	char *e;
	int i;

	if (argc > 3 || argc == 2)
		return EC_ERROR_PARAM_COUNT;

	/* If three arguments, then we have an offset and value to set. */
	if (argc == 3) {
		offset = strtoi(argv[1], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

		val = strtoi(argv[2], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

		*(lpc_get_memmap_range()+offset) = val;
	}

	/* Display full contents of memmap. */
	for (i = 0; i < EC_MEMMAP_SIZE; i++) {
		ccprintf("%02x ", *(lpc_get_memmap_range()+i));
		if (i%16 == 15) {
			ccprintf("\n");
			cflush();
		}
	}

	ccprintf("\n");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(memmap, command_memmap,
			"[offset value]",
			"Print or set value in memory map",
			NULL);

static int command_hostcmdmem(int argc, char **argv)
{
	uint32_t val, offset;
	char *e;
	int i;

	if (argc > 3 || argc == 2)
		return EC_ERROR_PARAM_COUNT;

	/* If three arguments, then we have an offset and value to set. */
	if (argc == 3) {
		offset = strtoi(argv[1], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

		val = strtoi(argv[2], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

		*((uint8_t *)LPC_POOL_CMD_DATA+offset) = val;
	}

	/* Display full contents of memmap. */
	ccprintf("%02x %02x %02x\n", *((uint8_t *)LPC_POOL_HOST_CMD),
			*((uint8_t *)LPC_POOL_HOST_STATUS),
			*((uint8_t *)LPC_POOL_HOST_DATA));
	for (i = 0; i < 256; i++) {
		ccprintf("%02x ", *((uint8_t *)LPC_POOL_CMD_DATA+i));
		if (i%16 == 15) {
			ccprintf("\n");
			cflush();
		}
	}

	ccprintf("\n");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hostmem, command_hostcmdmem,
			"[offset value]",
			"Print or set value in host command memory",
			NULL);

static int command_access_pnpcfg(int argc, char **argv)
{
	int addr, data;
	char *e;

	if (argc < 2 || argc > 3)
		return EC_ERROR_PARAM_COUNT;

	addr = strtoi(argv[1], &e, 16);
	if (*e)
		return EC_ERROR_PARAM1;

	/* If three args, perform a write. If two args, perform read. */
	if (argc == 3) {
		data = strtoi(argv[2], &e, 16);
		if (*e)
			return EC_ERROR_PARAM1;

		ec2i_write_host_pnpcfg(addr, data);
	} else {
		ec2i_read_host_pnpcfg(addr, &data);
		ccprintf("0x%02x\n", addr);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pnpcfg, command_access_pnpcfg,
			"[addr [data]]",
			"Read or write to PNPCFG registers",
			NULL);
