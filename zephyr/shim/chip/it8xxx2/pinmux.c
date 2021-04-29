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

/*
 * Init priority is behind CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY to overwrite
 * GPIO_INPUT setting of i2c ports.
 */
static int it8xxx2_pinmux_init_latr(const struct device *dev)
{
	ARG_UNUSED(dev);

	const struct device *portb = DEVICE_DT_GET(DT_NODELABEL(pinmuxb));
	const struct device *portc = DEVICE_DT_GET(DT_NODELABEL(pinmuxc));
	const struct device *porte = DEVICE_DT_GET(DT_NODELABEL(pinmuxe));
	const struct device *portf = DEVICE_DT_GET(DT_NODELABEL(pinmuxf));

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
	/* I2C0 CLK */
	pinmux_pin_set(portb, 3, IT8XXX2_PINMUX_FUNC_1);
	/* I2C0 DAT */
	pinmux_pin_set(portb, 4, IT8XXX2_PINMUX_FUNC_1);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
	/* I2C1 CLK */
	pinmux_pin_set(portc, 1, IT8XXX2_PINMUX_FUNC_1);
	/* I2C1 DAT */
	pinmux_pin_set(portc, 2, IT8XXX2_PINMUX_FUNC_1);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c2), okay)
	/* I2C2 CLK */
	pinmux_pin_set(portf, 6, IT8XXX2_PINMUX_FUNC_1);
	/* I2C2 DAT */
	pinmux_pin_set(portf, 7, IT8XXX2_PINMUX_FUNC_1);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c4), okay)
	/* I2C4 CLK */
	pinmux_pin_set(porte, 0, IT8XXX2_PINMUX_FUNC_3);
	/* I2C4 DAT */
	pinmux_pin_set(porte, 7, IT8XXX2_PINMUX_FUNC_3);
#endif

	return 0;
}
SYS_INIT(it8xxx2_pinmux_init_latr, POST_KERNEL, 52);
