/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <soc.h>

static int it8xxx2_pinmux_init(const struct device *dev)
{
	ARG_UNUSED(dev);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmuxb), okay)
	const struct device *portb = DEVICE_DT_GET(DT_NODELABEL(pinmuxb));

	__ASSERT_NO_MSG(device_is_ready(portb));
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
	/* SIN0 */
	pinmux_pin_set(portb, 0, IT8XXX2_PINMUX_FUNC_3);
	/* SOUT0 */
	pinmux_pin_set(portb, 1, IT8XXX2_PINMUX_FUNC_3);
#endif

	return 0;
}

SYS_INIT(it8xxx2_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
