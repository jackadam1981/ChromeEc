/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "injector.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"
#include "watchdog.h"

/*
 * CCx Resistors control definition
 *
 * Resistor control GPIOs :
 * USB_DUT_CC1_RA       C7
 * USB_DUT_CC1_RPUSB    C3
 * USB_DUT_CC1_RP1A5    C15
 * USB_DUT_CC1_RP3A0    C14
 * USB_DUT_CC2_RPUSB    B0
 * USB_DUT_CC1_RD       C6
 * USB_DUT_CC2_RD       B1
 * USB_DUT_CC2_RA       B2
 * USB_DUT_CC2_RP1A5    C1
 * USB_DUT_CC2_RP3A0    C8
 */
static const struct res_cfg {
	const char *name;
	struct config {
		enum gpio_signal signal;
		uint32_t flags;
	} cfgs[2];
} res_cfg[] = {
	[INJ_RES_NONE] = { "NONE" },
	[INJ_RES_RA] = { "RA",
			 { { GPIO_USB_DUT_CC1_RA, GPIO_OUT_LOW },
			   { GPIO_USB_DUT_CC2_RA, GPIO_OUT_LOW } } },
	[INJ_RES_RD] = { "RD",
			 { { GPIO_USB_DUT_CC1_RD, GPIO_OUT_LOW },
			   { GPIO_USB_DUT_CC2_RD, GPIO_OUT_LOW } } },
	[INJ_RES_RPUSB] = { "RPUSB",
			    { { GPIO_USB_DUT_CC1_RPUSB, GPIO_OUT_HIGH },
			      { GPIO_USB_DUT_CC2_RPUSB, GPIO_OUT_HIGH } } },
	[INJ_RES_RP1A5] = { "RP1A5",
			    { { GPIO_USB_DUT_CC1_RP1A5, GPIO_OUT_HIGH },
			      { GPIO_USB_DUT_CC2_RP1A5, GPIO_OUT_HIGH } } },
	[INJ_RES_RP3A0] = { "RP3A0",
			    { { GPIO_USB_DUT_CC1_RP3A0, GPIO_OUT_HIGH },
			      { GPIO_USB_DUT_CC2_RP3A0, GPIO_OUT_HIGH } } },
};

/* ------ Helper functions ------ */

static void set_resistor(int pol, enum inj_res res)
{
	/* reset everything on one CC to high impedance */
	gpio_set_flags(res_cfg[INJ_RES_RA].cfgs[pol].signal, GPIO_INPUT);
	gpio_set_flags(res_cfg[INJ_RES_RD].cfgs[pol].signal, GPIO_INPUT);
	gpio_set_flags(res_cfg[INJ_RES_RPUSB].cfgs[pol].signal, GPIO_INPUT);
	gpio_set_flags(res_cfg[INJ_RES_RP1A5].cfgs[pol].signal, GPIO_INPUT);
	gpio_set_flags(res_cfg[INJ_RES_RP3A0].cfgs[pol].signal, GPIO_INPUT);

	/* connect the resistor if needed */
	if (res != INJ_RES_NONE)
		gpio_set_flags(res_cfg[res].cfgs[pol].signal,
			       res_cfg[res].cfgs[pol].flags);
}

/* ------ Console commands ------ */

static int cmd_resistor(int argc, const char **argv)
{
	int p, r;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	for (p = 0; p < 2; p++) {
		int is_set = 0;

		for (r = 0; r < ARRAY_SIZE(res_cfg); r++)
			if (strcasecmp(res_cfg[r].name, argv[p]) == 0) {
				set_resistor(p, r);
				is_set = 1;
				break;
			}
		/* Unknown name : set to No resistor */
		if (!is_set)
			set_resistor(p, INJ_RES_NONE);
	}
	return EC_SUCCESS;
}

static int command_tw(int argc, const char **argv)
{
	if (!strncasecmp(argv[1], "resistor", 3))
		return cmd_resistor(argc - 2, argv + 2);
	else
		return EC_ERROR_PARAM1;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(twinkie, command_tw, "[resistor]",
			"Manual Twinkie tweaking");
