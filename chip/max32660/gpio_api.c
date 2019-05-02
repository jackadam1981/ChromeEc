/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 GPIO API for Chrome EC */

/* **** Includes **** */
#include "mxc_config.h"
#include "mxc_assert.h"
#include "gpio_api.h"
//#include "me11_gpio.h"

/**
 * @ingroup gpio
 * @{
 */

/* **** Definitions **** */

/* **** Globals **** */

static void (*callback[MXC_CFG_GPIO_INSTANCES][MXC_CFG_GPIO_PINS_PORT])(void *);
static void *cbparam[MXC_CFG_GPIO_INSTANCES][MXC_CFG_GPIO_PINS_PORT];

/* **** Functions **** */

/* ************************************************************************** */
/*
 *       GPIO_EN  |  GPIO_EN1           |   Function
 *  --------------|---------------------|----------------------
 *     0          |          0          |     Alternative 1
 *     0          |          1          |     Alternative 2
 *     1          |          1          |     Alternative 3
 *     1          |          0          |     GPIO (default)
*/

int GPIO_Config(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	// Set the GPIO type
	switch (cfg->func) {
	case GPIO_FUNC_IN:
		gpio->out_en_clr = cfg->mask;
		gpio->en_set = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_OUT:
		gpio->out_en_set = cfg->mask;
		gpio->en_set = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_ALT1:
		gpio->en_clr = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_ALT2:
		gpio->en_clr = cfg->mask;
		gpio->en1_set = cfg->mask;
		gpio->en2_clr = cfg->mask;
		break;
	case GPIO_FUNC_ALT3:
#if TARGET_NUM == 32660
		gpio->en_set = cfg->mask;
		gpio->en1_set = cfg->mask;
#else
		gpio->en_clr = cfg->mask;
		gpio->en1_clr = cfg->mask;
		gpio->en2_set = cfg->mask;
#endif
		break;
	case GPIO_FUNC_ALT4:
		gpio->en_clr = cfg->mask;
		gpio->en1_set = cfg->mask;
		gpio->en2_set = cfg->mask;
		break;
	default:
		return E_BAD_PARAM;
	}

	// Configure the pad
	switch (cfg->pad) {
	case GPIO_PAD_NONE:
		gpio->pad_cfg1 &= ~cfg->mask;
		gpio->pad_cfg2 &= ~cfg->mask;
#if TARGET_NUM == 32660
		gpio->ps &= ~cfg->mask;
#endif
		break;
	case GPIO_PAD_PULL_UP:
		gpio->pad_cfg1 |= cfg->mask;
		gpio->pad_cfg2 &= ~cfg->mask;
#if TARGET_NUM == 32660
		gpio->ps |= cfg->mask;
#endif
		break;
	case GPIO_PAD_PULL_DOWN:
		gpio->pad_cfg1 &= ~cfg->mask;
		gpio->pad_cfg2 |= cfg->mask;
#if TARGET_NUM == 32660
		gpio->ps &= ~cfg->mask;
#endif
		break;
	default:
		return E_BAD_PARAM;
	}

	return E_NO_ERROR;
}

/* ************************************************************************** */
uint32_t GPIO_InGet(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	return (gpio->in & cfg->mask);
}

/* ************************************************************************** */
void GPIO_OutSet(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->out_set = cfg->mask;
}

/* ************************************************************************** */
void GPIO_OutClr(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->out_clr = cfg->mask;
}

/* ************************************************************************** */
uint32_t GPIO_OutGet(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	return (gpio->out & cfg->mask);
}

/* ************************************************************************** */
void GPIO_OutPut(const gpio_cfg_t *cfg, uint32_t val)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->out = (gpio->out & ~cfg->mask) | (val & cfg->mask);
}

/* ************************************************************************** */
void GPIO_OutToggle(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->out ^= cfg->mask;
}

/* ************************************************************************** */
int GPIO_IntConfig(const gpio_cfg_t *cfg, gpio_int_mode_t mode, gpio_int_pol_t pol)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	switch (mode) {
	case GPIO_INTERRUPT_LEVEL:
		gpio->int_mod &= ~cfg->mask;
		break;
	case GPIO_INTERRUPT_EDGE:
		gpio->int_mod |= cfg->mask;
		break;
	default:
		return E_BAD_PARAM;
	}

	switch (pol) {
	case GPIO_INTERRUPT_FALLING: /* GPIO_INT_HIGH */
		gpio->int_pol &= ~cfg->mask;
		gpio->int_dual_edge &= ~cfg->mask;
		break;
	case GPIO_INTERRUPT_RISING: /* GPIO_INT_LOW */
		gpio->int_pol |= cfg->mask;
		gpio->int_dual_edge &= ~cfg->mask;
		break;
	case GPIO_INTERRUPT_BOTH:
		gpio->int_dual_edge |= cfg->mask;
		break;
	default:
		return E_BAD_PARAM;
	}

	return E_NO_ERROR;
}

/* ************************************************************************** */
void GPIO_IntEnable(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->int_en_set = cfg->mask;
}

/* ************************************************************************** */
void GPIO_IntDisable(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->int_en_clr = cfg->mask;
}

/* ************************************************************************** */
uint32_t GPIO_IntStatus(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	return (gpio->int_stat & cfg->mask);
}

/* ************************************************************************** */
void GPIO_IntClr(const gpio_cfg_t *cfg)
{
	mxc_gpio_regs_t *gpio = MXC_GPIO_GET_GPIO(cfg->port);

	gpio->int_clr = cfg->mask;
}

/* ************************************************************************** */
void GPIO_RegisterCallback(const gpio_cfg_t *cfg, gpio_callback_fn func, void *cbdata)
{
	uint32_t mask;
	unsigned int pin;

	mask = cfg->mask;
	pin = 0;

	while (mask) {
		if (mask & 1) {
			callback[cfg->port][pin] = func;
			cbparam[cfg->port][pin] = cbdata;
		}
		pin++;
		mask >>= 1;
	}
}

/* ************************************************************************** */
void GPIO_Handler(unsigned int port)
{
	uint32_t stat;
	unsigned int pin;
	mxc_gpio_regs_t *gpio;

	MXC_ASSERT(port < MXC_CFG_GPIO_INSTANCES);

	gpio = MXC_GPIO_GET_GPIO(port);

	stat = gpio->int_stat;
	gpio->int_clr = stat;

	pin = 0;

	while (stat) {
		if (stat & 1) {
			callback[port][pin](cbparam[port][pin]);
		}
		pin++;
		stat >>= 1;
	}
}

/**@} end of group gpio */
