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
#include "gpio.h"
#include "registers.h"
#include "system.h"
#include "timer.h"

#ifdef CR50_RELAXED
#define CPRINTS(format, args...) cprints(CC_TASK, "EC-COMM: " format, ## args)
#else
#define CPRINTS(format, args...)
#endif

/**
 * Context of EC-EFS
 */
static struct ec_comm_context_ {
	uint32_t supported:1;
	uint32_t reserved:31;
} ec_comm_ctx;


int ec_comm_is_supported(void)
{
	return !!ec_comm_ctx.supported;
}

void ec_comm_init(void)
{
	uint32_t sel_backup = GREAD(PINMUX, DIOB3_SEL);
	uint32_t ctl_backup = GREAD(PINMUX, DIOB3_CTL);

	/*
	 * Because GPIO_AP_FLASH_SEL flag is GPIO_OUT_LOW, let's disconnect
	 * output pinmux of DIOB3.
	 */
	GWRITE(PINMUX, DIOB3_SEL, 0);
	/* Configure DIOB3 as DIO_CTL_IE_MASK | GPIO_PULL_UP. */
	GWRITE(PINMUX, DIOB3_CTL, DIO_CTL_IE_MASK | DIO_CTL_PU_MASK);
	udelay(STRAP_PIN_DELAY_USEC);

	/*
	 * Read the level of DIOB3.
	 * Boards supporting EC-EFS have a 1M pull-down on DIOB3, and
	 * The other boards have a 10K pull-down.
	 */
	ec_comm_ctx.supported = !!gpio_get_level(GPIO_AP_FLASH_SELECT);

	/* Recover the DIOB3 pinmux control register value. */
	GWRITE(PINMUX, DIOB3_CTL, ctl_backup);

	if (ec_comm_ctx.supported) {
		/* Connect GPIO_AP_FLASH_SELECT to DIOB4. */
		GWRITE(PINMUX, DIOB4_SEL, GC_PINMUX_GPIO0_GPIO2_SEL);
		GWRITE(PINMUX, GPIO0_GPIO2_SEL, GC_PINMUX_DIOB4_SEL);
	} else {
		/* Recover the DIOB3 pinmux select register value. */
		GWRITE(PINMUX, DIOB3_SEL, sel_backup);
	}
}

/**
 * A console command, printing EC-CR50-Comm status.
 */
static int command_ec_comm(int argc, char **argv)
{
	/*
	 * EC-EFS Context
	 */
	ccprintf("[EC-COMM Context]\n");
	ccprintf("Supported        : %s\n",
		 ec_comm_ctx.supported ? "yes" : "no");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ec_comm, command_ec_comm, NULL,
			"Dump EC-CR50-comm info");
