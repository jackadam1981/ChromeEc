/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "case_closed_debug.h"
#include "console.h"
#include "gpio.h"
#include "rdd.h"
#include "registers.h"
#include "usb_api.h"

static void usart_tx_connect(void)
{
	GWRITE(PINMUX, DIOA7_SEL, GC_PINMUX_UART1_TX_SEL);
	GWRITE(PINMUX, DIOB5_SEL, GC_PINMUX_UART2_TX_SEL);
}

static void usart_tx_disconnect(void)
{
	GWRITE(PINMUX, DIOA7_SEL, GC_PINMUX_DIOA3_SEL_DEFAULT);
	GWRITE(PINMUX, DIOB5_SEL, GC_PINMUX_DIOB5_SEL_DEFAULT);
}

void rdd_attached(void)
{
	/* Indicate case-closed debug mode (active low) */
	gpio_set_level(GPIO_CCD_MODE_L, 0);

	/* Select the CCD PHY */
	usb_select_phy(USB_SEL_PHY1);

	ccd_set_mode(CCD_MODE_ENABLED);
}

void rdd_detached(void)
{
	/* Disconnect from AP and EC UART TX */
	usart_tx_disconnect();

	/* Done with case-closed debug mode */
	gpio_set_level(GPIO_CCD_MODE_L, 1);

	/* Select the AP PHY */
	usb_select_phy(USB_SEL_PHY0);

	ccd_set_mode(CCD_MODE_DISABLED);
}

static int command_ccd(int argc, char **argv)
{
	int enable;

	if (argc > 1) {
		if (!strcasecmp("enable", argv[argc - 1]))
			enable = 1;
		else if (!strcasecmp("disable", argv[argc - 1]))
			enable = 0;

		if (!strcasecmp("uart", argv[1])) {
			if (enable)
				usart_tx_connect();
			else
				usart_tx_disconnect();

			ccprintf("UART %s\n", GREAD(PINMUX, DIOA7_SEL) ==
				GC_PINMUX_DIOA3_SEL_DEFAULT ?
				"disabled" : "enabled");
		} else if (argc == 2) {
			if (enable)
				rdd_attached();
			else
				rdd_detached();
		} else
			return EC_ERROR_PARAM1;
	}

	ccprintf("ccd %s\n", usb_get_phy() == USB_SEL_PHY1 ? "enabled" :
		"disabled");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ccd, command_ccd,
	"[uart] [enable|disable]",
	"Get/set the case closed debug state",
	NULL);
