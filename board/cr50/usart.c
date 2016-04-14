/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "registers.h"
#include "usart.h"

void usart_tx_connect(void)
{
	GWRITE(PINMUX, DIOA7_SEL, GC_PINMUX_UART1_TX_SEL);
	GWRITE(PINMUX, DIOB5_SEL, GC_PINMUX_UART2_TX_SEL);
}

void usart_tx_disconnect(void)
{
	GWRITE(PINMUX, DIOA7_SEL, GC_PINMUX_DIOA3_SEL_DEFAULT);
	GWRITE(PINMUX, DIOB5_SEL, GC_PINMUX_DIOB5_SEL_DEFAULT);
}
