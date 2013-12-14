/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPC module for Chrome EC */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "port80.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define PNPCFG_ADDR_PORT	0x00
#define PNPCFG_DATA_PORT	0x01

/**
 * Write to host PNPCFG registers.
 */
static void ec2i_write_host_pnpcfg(int addr, int data)
{
	/* Disable host access to I-bus. */
	IT83XX_EC2I_LSIOHA |= 0x01;

	/* Enable EC access to I-bus. */
	IT83XX_EC2I_IBCTL = 0x01;

	/* Make sure no writes or reads are in progress. */
	while (IT83XX_EC2I_IBCTL & 0x06) ;

	/* Provide EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x01;

	/* Set address and data. */
	IT83XX_EC2I_IHIOA = addr;
	IT83XX_EC2I_IHD = data;

	/* Wait for write to complete. */
	while (IT83XX_EC2I_IBCTL & 0x04) ;

	/* Disable EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x00;

	/* Disable EC access to I-bus. */
	IT83XX_EC2I_IBCTL &= ~0x01;

	/* Enable host access to I-bus. */
	IT83XX_EC2I_LSIOHA &= ~0x01;
}

/**
 * Read from host PNPCFG registers.
 */
static int ec2i_read_host_pnpcfg(int addr)
{
	int data;

	/* Disable host access to I-bus. */
	IT83XX_EC2I_LSIOHA |= 0x01;

	/* Enable EC access to I-bus. */
	IT83XX_EC2I_IBCTL = 0x01;

	/* Make sure no writes or reads are in progress. */
	while (IT83XX_EC2I_IBCTL & 0x06) ;

	/* Set address. */
	IT83XX_EC2I_IHIOA = addr;

	/* Provide EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x01;

	/* Enable EC access to I-bus and initiate a read. */
	IT83XX_EC2I_IBCTL |= 0x03;

	/* Wait for read to complete. */
	while (IT83XX_EC2I_IBCTL & 0x02) ;

	/* Read the data. */
	data = IT83XX_EC2I_IHD;

	/* Disable EC access to PNPCFG registers. */
	IT83XX_EC2I_IBMAE = 0x00;

	/* Disable EC access to I-bus. */
	IT83XX_EC2I_IBCTL &= ~0x01;

	/* Enable host access to I-bus. */
	IT83XX_EC2I_LSIOHA &= ~0x01;

	return data;
}

void __lpc_port80_interrupt(enum gpio_signal signal)
{
	int addr;

	/* Set active logical device to the RTCT. */
	ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x07);
	ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x10);

	/* Read address of last port 0x80 write to BRAM. */
	ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0xf5);
	addr = ec2i_read_host_pnpcfg(PNPCFG_DATA_PORT);

	/* Read last port 0x80 data and store it. */
	port_80_write(REG8(IT83XX_BRAM_BASE+128+addr));
}

static void lpc_init(void)
{
	/* Enable Debug Port 80h. */
	IT83XX_GCTRL_SPCTRL1 |= 0xc0;

	/*
	 * Specify not using LPCPD# or LPCRST# to reset PNPCFG.
	 * TODO: eventually, we probably want to use LPCPD.
	 */
	IT83XX_GCTRL_RSTS &= ~0x08;
	IT83XX_GPIO_GCR |= 0x06;

	/* Set UART2 as active logical device. */
	ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x07);
	ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x02);

	/* Move IO Descriptor 0 to 0x3f8. */
	ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x60);
	ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x03);
	ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x61);
	ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0xf8);

	/* Activate UART2 Plug and Play. */
	ec2i_write_host_pnpcfg(PNPCFG_ADDR_PORT, 0x30);
	ec2i_write_host_pnpcfg(PNPCFG_DATA_PORT, 0x01);

	/*
	 * Set pins E0 and E7 to be L80HLat so that we can trigger
	 * an interrupt when a port 0x80 command is received.
	 */
	gpio_set_alternate_function(GPIO_E, 1<<0, 1);

	/* Enable port 0x80 interrupts. */
	gpio_enable_interrupt(GPIO_L80HLAT);
}
/*
 * Set prio to higher than default; this way LPC memory mapped data is ready
 * before other inits try to initialize their memmap data.
 */
DECLARE_HOOK(HOOK_INIT, lpc_init, HOOK_PRIO_INIT_LPC);

/*****************************************************************************/
/* Console commands */

static int command_print_bram(int argc, char **argv)
{
	uint8_t val;
	char *e;
	int i;

	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	/* If two args, use second arg to initialize BRAM. */
	if (argc == 2) {
		val = strtoi(argv[1], &e, 16);
		if (*e)
			return EC_ERROR_PARAM1;

		for(i=0; i<192; i++) {
			REG8(IT83XX_BRAM_BASE+i) = val;
		}
	}

	for(i=0; i<192; i++) {
		ccprintf("%02x ", REG8(IT83XX_BRAM_BASE+i));
		if(i%16 == 15) {
			ccprintf("\n");
			cflush();
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(bram, command_print_bram,
			"[val]",
			"Print and/or clear battery backed ram (BRAM)",
			NULL);

static int command_access_pnpcfg(int argc, char **argv)
{
	uint8_t addr, data;
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
		ccprintf("0x%02x\n", ec2i_read_host_pnpcfg(addr));
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pnpcfg, command_access_pnpcfg,
			"[addr [data]]",
			"Read or write to PNPCFG registers",
			NULL);
