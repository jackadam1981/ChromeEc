/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define LOG_LEVEL CONFIG_USB_MUX_MOCK_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(emul_usb_mux_mock);

#include "usb_mux.h"

#include "emul/emul_usb_mux_mock.h"

#define USB_PD_MUX_USB_DP_STATE	(USB_PD_MUX_USB_ENABLED | \
			USB_PD_MUX_DP_ENABLED | USB_PD_MUX_POLARITY_INVERTED | \
			USB_PD_MUX_SAFE_MODE | USB_PD_MUX_TBT_COMPAT_ENABLED | \
			USB_PD_MUX_USB4_ENABLED)

/** Public API for controlling/inspecting this mock */
struct emul_usb_mux_mock_data usb_mux_mock_data[CONFIG_USB_MUX_MOCK_NUM];

/** Check description in emul_usb_mux_mock.h */
void emul_usb_mux_mock_reset(void)
{
	memset(usb_mux_mock_data, 0, sizeof(usb_mux_mock_data));
}

/**
 * @brief Usb mux mock init callback. If provided, alt_drv->init() can replace
 *        this function.
 *
 * @param me Pointer to usb_mux, i2c_addr_flags field select which mock instance
 *           should be used.
 *
 * @return code from usb_mux_mock_data or alt_drv->init()
 */
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

/**
 * @brief Usb mux mock set callback. If provided, alt_drv->set() can replace
 *        this function.
 *
 * @param me Pointer to usb_mux, i2c_addr_flags field select which mock instance
 *           should be used.
 * @param mux_state State to which mux should be set.
 * @param ack_required Pointer where mux should return if ACK from host is
 *                     required. Usb mux mock set it always to false.
 *
 * @return code from usb_mux_mock_data or alt_drv->set()
 */
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

/**
 * @brief Usb mux mock get callback. If provided, alt_drv->get() can replace
 *        this function.
 *
 * @param me Pointer to usb_mux, i2c_addr_flags field select which mock instance
 *           should be used.
 *
 * @return code from usb_mux_mock_data or alt_drv->get()
 */
static int mock_get(const struct usb_mux *me, mux_state_t *mux_state)
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

/**
 * @brief Usb mux mock enter_low_power_mode callback. If provided,
 *        alt_drv->enter_low_power_mode() can replace this function.
 *
 * @param me Pointer to usb_mux, i2c_addr_flags field select which mock instance
 *           should be used.
 *
 * @return code from usb_mux_mock_data or alt_drv->enter_low_power_mode()
 */
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

/**
 * @brief Usb mux mock chipset_reset callback. If provided,
 *        alt_drv->chipset_reset() can replace this function.
 *
 * @param me Pointer to usb_mux, i2c_addr_flags field select which mock instance
 *           should be used.
 *
 * @return code from usb_mux_mock_data or alt_drv->chipset_reset()
 */
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

/**
 * @brief Usb mux mock is_retimer_fw_update_capable callback.
 *
 * @return true
 */
static bool mock_fw_update_cap(void)
{
	return true;
}

/** Check description in emul_usb_mux_mock.h */
/**
 * @brief HPD update function that can be used in usb_mux structure
 *
 * @param me Pointer to usb_mux, i2c_addr_flags field select which mock instance
 *           should be used.
 * @param mux_state State to which mux should be set.
 *
 * @return code from usb_mux_mock_data or alt_drv->set()
 */
void emul_usb_mux_hpd_update(const struct usb_mux *me, mux_state_t mux_state)
{
	int i = me->i2c_addr_flags;

	if (i >= CONFIG_USB_MUX_MOCK_NUM) {
		LOG_ERR("There is no %d instance of usb mux mock!\n", i);

		return;
	}

	usb_mux_mock_data[i].num_hpd_update_calls++;

	/* Current HPD related mux status + existing USB & DP mux status */
	usb_mux_mock_data[i].state &= USB_PD_MUX_USB_DP_STATE;
	usb_mux_mock_data[i].state |= mux_state;
}

const struct usb_mux_driver emul_usb_mux_mock = {
	.init = &mock_init,
	.set = &mock_set,
	.get = &mock_get,
	.enter_low_power_mode = &mock_enter_lpm,
	.chipset_reset = &mock_chipset_reset,
	.is_retimer_fw_update_capable = &mock_fw_update_cap,
};
