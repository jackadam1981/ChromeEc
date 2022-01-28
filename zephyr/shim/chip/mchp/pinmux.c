/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <soc.h>

/*
 * MCHP UART and eSPI drivers have not been updated to configure their pins.
 * GPIO_0105 Func1 UART0_RX
 * GPIO_0104 Func1 UART0_TX
 *
 * eSPI pins.
 * GPIO_0061 Func1 ESPI_RESET#
 * GPIO_0063 Func1 ESPI_ALERT#
 * GPIO_0066 Func1 ESPI_CS#
 * GPIO_0065 Func1 ESPI_CLK
 * GPIO_0070 Func1 ESPI_IO0
 * GPIO_0071 Func1 ESPI_IO1
 * GPIO_0072 Func1 ESPI_IO2
 * GPIO_0073 Func1 ESPI_IO3
 *
 * TODO - Do we need other attributes for some of these pins?
 * OpenDrain, drive strength adjustment
 * eSPI data width configuration one, two, or four lanes?
 */
static int xec_pinmux_init(const struct device *dev)
{
	ARG_UNUSED(dev);

#if 0 /* Trying out PINCTRL drivers: ADC, eSPI, I2C, KSCAN, PWM, SPI, UART */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmux_000_036), okay)
	const struct device *porta =
		DEVICE_DT_GET(DT_NODELABEL(pinmux_000_036));
	if (!porta)
		return -ENODEV;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmux_040_076), okay)
	const struct device *portb =
		DEVICE_DT_GET(DT_NODELABEL(pinmux_040_076));
	if (!portb)
		return -ENODEV;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmux_100_136), okay)
	const struct device *portc =
		DEVICE_DT_GET(DT_NODELABEL(pinmux_100_136));
	if (!portc)
		return -ENODEV;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmux_140_176), okay)
	const struct device *portd =
		DEVICE_DT_GET(DT_NODELABEL(pinmux_140_176));
	if (!portd)
		return -ENODEV;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmux_200_236), okay)
	const struct device *porte =
		DEVICE_DT_GET(DT_NODELABEL(pinmux_200_236));
	if (!porte)
		return -ENODEV;
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart0), okay)
	/* GPIO_0105 F1 is UART0_RX, GPIO_0104 F1 is UART0_TX */
	pinmux_pin_set(portc, MCHP_GPIO_105, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portc, MCHP_GPIO_104, MCHP_GPIO_CTRL_MUX_F1);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
	/* GPIO_0171 F1 is UART1_RX, GPIO_0170 F1 is UART1_TX */
	pinmux_pin_set(portd, MCHP_GPIO_171, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portd, MCHP_GPIO_170, MCHP_GPIO_CTRL_MUX_F1);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(espi0), okay)
	pinmux_pin_set(portb, MCHP_GPIO_061, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_063, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_066, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_065, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_070, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_071, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_072, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portb, MCHP_GPIO_073, MCHP_GPIO_CTRL_MUX_F1);
#endif

/* ADC channels: 0, 3, 4, 5 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(adc0), okay)

	pinmux_pin_set(porte, MCHP_GPIO_200, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porte, MCHP_GPIO_203, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porte, MCHP_GPIO_204, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porte, MCHP_GPIO_205, MCHP_GPIO_CTRL_MUX_F1);
#endif

/* keyscan: ksi 0-7, kso 0-1, 3-17 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(cros_kb_raw), okay)

	/* TODO - Internal pulls for any keyscan pins? */

	pinmux_pin_set(porta, MCHP_GPIO_017, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_020, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_021, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_026, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_027, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_030, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_031, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(porta, MCHP_GPIO_032, MCHP_GPIO_CTRL_MUX_F1);

	pinmux_pin_set(portb, MCHP_GPIO_040, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portb, MCHP_GPIO_045, MCHP_GPIO_CTRL_MUX_F1);

	if (!(IS_ENABLED(CONFIG_PLATFORM_EC_KEYBOARD_COL2_INVERTED)))
		pinmux_pin_set(portb, MCHP_GPIO_046, MCHP_GPIO_CTRL_MUX_F1);

	pinmux_pin_set(portb, MCHP_GPIO_047, MCHP_GPIO_CTRL_MUX_F1);

	pinmux_pin_set(portc, MCHP_GPIO_107, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_112, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portc, MCHP_GPIO_113, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portc, MCHP_GPIO_120, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portc, MCHP_GPIO_121, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_122, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_123, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_124, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_125, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_126, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portc, MCHP_GPIO_132, MCHP_GPIO_CTRL_MUX_F2);

	pinmux_pin_set(portd, MCHP_GPIO_152, MCHP_GPIO_CTRL_MUX_F1);
	pinmux_pin_set(portd, MCHP_GPIO_151, MCHP_GPIO_CTRL_MUX_F2);
	pinmux_pin_set(portd, MCHP_GPIO_140, MCHP_GPIO_CTRL_MUX_F3);
#endif
#endif /* 0 */
	return 0;
}
SYS_INIT(xec_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
