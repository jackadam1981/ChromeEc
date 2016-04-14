/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "registers.h"
#include "usart.h"
#include "usb_api.h"

void rdd_attached(void)
{
	/* Select the CCD PHY */
	usb_select_phy(USB_SEL_PHY1);

	/* Connect to selected phy */
	usb_init();
}

void rdd_detached(void)
{
#ifdef CONFIG_STREAM_USART
	/* Disconnect from AP and EC UART TX */
	usart_tx_disconnect();
#endif

	/* Select the AP PHY */
	usb_select_phy(USB_SEL_PHY0);

	/* Connect to selected phy */
	usb_init();
}
