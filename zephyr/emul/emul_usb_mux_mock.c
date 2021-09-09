/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define LOG_LEVEL CONFIG_USB_MUX_MOCK_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(emul_usb_mux_mock);

#include <device.h>
#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

#include "usb_mux.h"

#include "emul/emul_usb_mux_mock.h"

/* Public API for controlling/inspecting this mock */
struct emul_usb_mux_mock_data usb_mux_mock_data[CONFIG_USB_MUX_MOCK_NUM];

void emul_usb_mux_mock_reset(void)
{
	memset(usb_mux_mock_data, 0, sizeof(usb_mux_mock_data));
}

static int mock_init(const struct usb_mux *me)
{
	int i = me->i2c_addr_flags;

	if (i >= CONFIG_USB_MUX_MOCK_NUM) {
		LOG_ERR("There is no %d instance of usb mux mock!\n", i);

		return EC_ERROR_INVAL;
	}

	usb_mux_mock_data[i].num_init_calls++;

	if (usb_mux_mock_data[i].alt_drv != NULL &&
	    usb_mux_mock_data[i].alt_drv->init != NULL) {
		return usb_mux_mock_data[i].alt_drv->init(me);
	}

	return usb_mux_mock_data[i].init_ret;
}

static int mock_set(const struct usb_mux *me, mux_state_t mux_state,
		    bool *ack_required)
{
	int i = me->i2c_addr_flags;

	if (i >= CONFIG_USB_MUX_MOCK_NUM) {
		LOG_ERR("There is no %d instance of usb mux mock!\n", i);

		return EC_ERROR_INVAL;
	}

	usb_mux_mock_data[i].num_set_calls++;

	if (usb_mux_mock_data[i].alt_drv != NULL &&
	    usb_mux_mock_data[i].alt_drv->set != NULL) {
		return usb_mux_mock_data[i].alt_drv->set(me, mux_state,
							 ack_required);
	}

	/* Mock does not use host command ACKs */
	*ack_required = false;

	usb_mux_mock_data[i].state = mux_state;

	return usb_mux_mock_data[i].set_ret;
}

int mock_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int i = me->i2c_addr_flags;

	if (i >= CONFIG_USB_MUX_MOCK_NUM) {
		LOG_ERR("There is no %d instance of usb mux mock!\n", i);

		return EC_ERROR_INVAL;
	}

	usb_mux_mock_data[i].num_get_calls++;

	if (usb_mux_mock_data[i].alt_drv != NULL &&
	    usb_mux_mock_data[i].alt_drv->get != NULL) {
		return usb_mux_mock_data[i].alt_drv->get(me, mux_state);
	}

	*mux_state = usb_mux_mock_data[i].state;

	return usb_mux_mock_data[i].get_ret;
}

static int mock_enter_lpm(const struct usb_mux *me)
{
	int i = me->i2c_addr_flags;

	if (i >= CONFIG_USB_MUX_MOCK_NUM) {
		LOG_ERR("There is no %d instance of usb mux mock!\n", i);

		return EC_ERROR_INVAL;
	}

	usb_mux_mock_data[i].num_enter_lpm_calls++;

	if (usb_mux_mock_data[i].alt_drv != NULL &&
	    usb_mux_mock_data[i].alt_drv->enter_low_power_mode != NULL) {
		return usb_mux_mock_data[i].alt_drv->enter_low_power_mode(me);
	}

	return usb_mux_mock_data[i].enter_lpm_ret;
}

static int mock_chipset_reset(const struct usb_mux *me)
{
	int i = me->i2c_addr_flags;

	if (i >= CONFIG_USB_MUX_MOCK_NUM) {
		LOG_ERR("There is no %d instance of usb mux mock!\n", i);

		return EC_ERROR_INVAL;
	}

	usb_mux_mock_data[i].num_chipset_reset_calls++;

	if (usb_mux_mock_data[i].alt_drv != NULL &&
	    usb_mux_mock_data[i].alt_drv->chipset_reset != NULL) {
		return usb_mux_mock_data[i].alt_drv->chipset_reset(me);
	}

	return usb_mux_mock_data[i].chipset_reset_ret;
}

static bool mock_fw_update_cap(void)
{
	return true;
}

const struct usb_mux_driver emul_usb_mux_mock = {
	.init = &mock_init,
	.set = &mock_set,
	.get = &mock_get,
	.enter_low_power_mode = &mock_enter_lpm,
	.chipset_reset = &mock_chipset_reset,
	.is_retimer_fw_update_capable = &mock_fw_update_cap,
};
