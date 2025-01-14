/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Driver for anx7688 USB-C switch.
 */

#include "builtin/assert.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "anx7688_mux.h"
#include "time.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)

void deferred_anx7688_mux_power(void);
DECLARE_DEFERRED(deferred_anx7688_mux_power);
uint8_t anx7688_mux_power_state = 0;
const struct usb_mux *me_global;

test_export_static int anx7688_write(const struct usb_mux *me, uint32_t address,
	       			     uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, address, reg, val);
}

/**
 * Power on/reset ANX7688
 * ANX7688 needs a reset pulse of 10ms after power enable.
 */
void deferred_anx7688_mux_power(void)
{
	CPRINTS("%s %d", __func__, anx7688_mux_power_state);
	switch (anx7688_mux_power_state) {
	case ANX7688_POWER_STANDBY:
		/*
		 * PWR_EN_L low, RST low
		 * start reset sequence by turning off power enable
		 * and wait for 1ms.
		 */
		gpio_set_level(anx7688_control.reset_n, 0);
		gpio_set_level(anx7688_control.pwr_en, 0);
		gpio_set_level(anx7688_control.pwr3v3, 0);
		anx7688_mux_power_state = ANX7688_POWER_STANDBY;
		break;
	case ANX7688_POWER_INIT:
		gpio_set_level(anx7688_control.pwr3v3, 1);
		anx7688_mux_power_state = ANX7688_POWER_EN;
		hook_call_deferred(&deferred_anx7688_mux_power_data, 10 * MSEC);
		break;
	case ANX7688_POWER_EN:
		gpio_set_level(anx7688_control.pwr_en, 1);
		anx7688_mux_power_state = ANX7688_POWER_RESET;
		hook_call_deferred(&deferred_anx7688_mux_power_data, 10 * MSEC);
		break;
	case ANX7688_POWER_RESET:
		gpio_set_level(anx7688_control.reset_n, 1);
		anx7688_mux_power_state = ANX7688_POWER_ON;
		break;
	case ANX7688_POWER_ON_SET:
		anx7688_mux_power_state = ANX7688_POWER_ON;
		anx7688_write(me_global, ANX7688_I2C_ADDR1_FLAGS,
			      ANX7688_GPIO_MAP5, 0x01);
		break;
	case ANX7688_POWER_ON:
	default:
		break;
	}
}


static void anx7688_power_set(const struct usb_mux *me, bool on)
{
	if (on)
		anx7688_mux_power_state = ANX7688_POWER_INIT;
	else
		anx7688_mux_power_state = ANX7688_POWER_STANDBY;

	hook_call_deferred(&deferred_anx7688_mux_power_data, 1 * MSEC);
}

static int anx7688_init(const struct usb_mux *me)
{
	anx7688_power_set(me, false);
	me_global = me;

	return EC_SUCCESS;
}

static int anx7688_set_state(const struct usb_mux *me, mux_state_t mux_state,
			    bool *ack_required)
{
	/* This driver does not use host command ACKs */
	*ack_required = false;

	CPRINTS("%s mux_state = 0x%x", __func__, mux_state);

	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR0_FLAGS,
			     ANX7688_OCM_CTRL, 0x11));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_ANA_CTRL1, 0x92));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_ANA_CTRL5, 0x41));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_ANA_CTRL2, 0x38));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_GPIO_CTRL0, 0xA7));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_GPIO_CTRL1, 0x2D));
	} else {
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR0_FLAGS,
			     ANX7688_OCM_CTRL, 0x01));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_ANA_CTRL1, 0x61));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_ANA_CTRL5, 0x81));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_ANA_CTRL2, 0x08));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_GPIO_CTRL0, 0x8D));
		RETURN_ERROR(anx7688_write(me, ANX7688_I2C_ADDR1_FLAGS,
			     ANX7688_GPIO_CTRL1, 0x2F));
	}
	anx7688_mux_power_state = ANX7688_POWER_ON_SET;
	hook_call_deferred(&deferred_anx7688_mux_power_data, 500 * MSEC);

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7688_mux_driver = {
	.init = anx7688_init,
	.set  = anx7688_set_state,
};

void anx7688_mux_power_on(const struct usb_mux *me)
{
	anx7688_power_set(me, true);
}

void anx7688_mux_standby(const struct usb_mux *me)
{
	anx7688_power_set(me, false);
}

