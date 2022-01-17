/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <dt-bindings/pinctrl/it8xxx2-pinctrl.h>
#include <soc.h>

#define SCL 0
#define SDA 1
#define IT8XXX2_I2C_DEV_PINMUX(node, signal)    DEVICE_DT_GET(DT_PHANDLE( \
	DT_PINCTRL_BY_IDX(DT_NODELABEL(node), 0, signal), pinctrls))
#define IT8XXX2_I2C_DEV_PIN(node, signal)       DT_PHA( \
	DT_PINCTRL_BY_IDX(DT_NODELABEL(node), 0, signal), pinctrls, pin)
#define IT8XXX2_I2C_DEV_ALT_FUNC(node, signal)  DT_PHA( \
	DT_PINCTRL_BY_IDX(DT_NODELABEL(node), 0, signal), pinctrls, alt_func)

static int it8xxx2_pinmux_init(const struct device *dev)
{
	ARG_UNUSED(dev);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pinmuxb), okay) && \
	DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
	const struct device *portb = DEVICE_DT_GET(DT_NODELABEL(pinmuxb));

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

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
	/* Pinmux control group */
	const struct device *clk0_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c0, SCL);
	const struct device *dat0_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c0, SDA);
	/* GPIO pin */
	uint8_t clk0_pin = IT8XXX2_I2C_DEV_PIN(i2c0, SCL);
	uint8_t dat0_pin = IT8XXX2_I2C_DEV_PIN(i2c0, SDA);
	/* Alternate function */
	uint8_t clk0_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c0, SCL);
	uint8_t dat0_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c0, SDA);

	/* I2C0 CLK */
	pinmux_pin_set(clk0_pinctrls, clk0_pin, clk0_alt);
	/* I2C0 DAT */
	pinmux_pin_set(dat0_pinctrls, dat0_pin, dat0_alt);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
	/* Pinmux control group */
	const struct device *clk1_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c1, SCL);
	const struct device *dat1_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c1, SDA);
	/* GPIO pin */
	uint8_t clk1_pin = IT8XXX2_I2C_DEV_PIN(i2c1, SCL);
	uint8_t dat1_pin = IT8XXX2_I2C_DEV_PIN(i2c1, SDA);
	/* Alternate function */
	uint8_t clk1_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c1, SCL);
	uint8_t dat1_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c1, SDA);

	/* I2C1 CLK */
	pinmux_pin_set(clk1_pinctrls, clk1_pin, clk1_alt);
	/* I2C1 DAT */
	pinmux_pin_set(dat1_pinctrls, dat1_pin, dat1_alt);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c2), okay)
	/* Pinmux control group */
	const struct device *clk2_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c2, SCL);
	const struct device *dat2_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c2, SDA);
	/* GPIO pin */
	uint8_t clk2_pin = IT8XXX2_I2C_DEV_PIN(i2c2, SCL);
	uint8_t dat2_pin = IT8XXX2_I2C_DEV_PIN(i2c2, SDA);
	/* Alternate function */
	uint8_t clk2_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c2, SCL);
	uint8_t dat2_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c2, SDA);

	/* I2C2 CLK */
	pinmux_pin_set(clk2_pinctrls, clk2_pin, clk2_alt);
	/* I2C2 DAT */
	pinmux_pin_set(dat2_pinctrls, dat2_pin, dat2_alt);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c3), okay)
	/* Pinmux control group */
	const struct device *clk3_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c3, SCL);
	const struct device *dat3_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c3, SDA);
	/* GPIO pin */
	uint8_t clk3_pin = IT8XXX2_I2C_DEV_PIN(i2c3, SCL);
	uint8_t dat3_pin = IT8XXX2_I2C_DEV_PIN(i2c3, SDA);
	/* Alternate function */
	uint8_t clk3_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c3, SCL);
	uint8_t dat3_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c3, SDA);

	/* I2C3 CLK */
	pinmux_pin_set(clk3_pinctrls, clk3_pin, clk3_alt);
	/* I2C3 DAT */
	pinmux_pin_set(dat3_pinctrls, dat3_pin, dat3_alt);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c4), okay)
	/* Pinmux control group */
	const struct device *clk4_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c4, SCL);
	const struct device *dat4_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c4, SDA);
	/* GPIO pin */
	uint8_t clk4_pin = IT8XXX2_I2C_DEV_PIN(i2c4, SCL);
	uint8_t dat4_pin = IT8XXX2_I2C_DEV_PIN(i2c4, SDA);
	/* Alternate function */
	uint8_t clk4_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c4, SCL);
	uint8_t dat4_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c4, SDA);

	/* I2C4 CLK */
	pinmux_pin_set(clk4_pinctrls, clk4_pin, clk4_alt);
	/* I2C4 DAT */
	pinmux_pin_set(dat4_pinctrls, dat4_pin, dat4_alt);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c5), okay)
	/* Pinmux control group */
	const struct device *clk5_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c5, SCL);
	const struct device *dat5_pinctrls = IT8XXX2_I2C_DEV_PINMUX(i2c5, SDA);
	/* GPIO pin */
	uint8_t clk5_pin = IT8XXX2_I2C_DEV_PIN(i2c5, SCL);
	uint8_t dat5_pin = IT8XXX2_I2C_DEV_PIN(i2c5, SDA);
	/* Alternate function */
	uint8_t clk5_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c5, SCL);
	uint8_t dat5_alt = IT8XXX2_I2C_DEV_ALT_FUNC(i2c5, SDA);

	/* I2C5 CLK */
	pinmux_pin_set(clk5_pinctrls, clk5_pin, clk5_alt);
	/* I2C5 DAT */
	pinmux_pin_set(dat5_pinctrls, dat5_pin, dat5_alt);
#endif

	return 0;
}
SYS_INIT(it8xxx2_pinmux_init_latr, POST_KERNEL, 52);
