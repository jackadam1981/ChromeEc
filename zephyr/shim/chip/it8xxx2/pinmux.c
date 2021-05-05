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

static int it8xxx2_pinmux_init_latr(const struct device *dev)
{
	ARG_UNUSED(dev);


	const struct device *porti = DEVICE_DT_GET(DT_NODELABEL(pinmuxi));


	/*
	 * TODO: This area is common for pinmux. ADC pinmux alternate
	 *       settings cannot be placed in this area.
	 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(adc0), okay)
	/* ADC 0 */
	pinmux_pin_set(porti, 0, IT8XXX2_PINMUX_FUNC_1);
	/* ADC 1 */
	pinmux_pin_set(porti, 1, IT8XXX2_PINMUX_FUNC_1);
	/* ADC 2 */
	pinmux_pin_set(porti, 2, IT8XXX2_PINMUX_FUNC_1);
	/* ADC 3 */
	pinmux_pin_set(porti, 3, IT8XXX2_PINMUX_FUNC_1);
	/* ADC 4 */
	pinmux_pin_set(porti, 4, IT8XXX2_PINMUX_FUNC_1);
	/* ADC 5 */
	pinmux_pin_set(porti, 5, IT8XXX2_PINMUX_FUNC_1);
#endif

	return 0;
}
SYS_INIT(it8xxx2_pinmux_init_latr, POST_KERNEL, 52);