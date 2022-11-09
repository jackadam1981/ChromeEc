/**
 * @file
 *
 * @brief Public APIs for the BC1.2 drivers.
 */

/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_USB_C_USBC_BC12_H_
#define ZEPHYR_INCLUDE_DRIVERS_USB_C_USBC_BC12_H_

/* BC1.2 USB charger voltage */
#define BC12_CHARGER_VOLTAGE_MV 5000

/* FIXME - make these Kconfig options */
/* BC1.2 USB charger minimum current */
#define BC12_CHARGER_MIN_CURR_MA 500
/* BC1.2 USB charger maximum current */
#define BC12_CHARGER_MAX_CURR_MA 1500

#define BC12_CURR_MA(val) \
	CLAMP(val, BC12_CHARGER_MIN_CURR_MA, BC12_CHARGER_MAX_CURR_MA)

enum bc12_role {
	BC12_DISCONNECTED,
	BC12_UFP,
	BC12_DFP,
};

enum bc12_type {
	BC12_TYPE_NONE,
	BC12_TYPE_SDP,
	BC12_TYPE_DCP,
	BC12_TYPE_CDP,
	BC12_TYPE_PROPRIETARY,
};

struct bc12_state {
	enum bc12_type type;
	int current; /* Current in mA */
	int voltage; /* Voltage in mA */
};

/**
 * @brief BC1.2 callback for charger configuration
 *
 * @param dev BC1.2 device which is notifying of the new charger state.
 * @param state Current state of the BC1.2 client, including BC1.2 type
 * detected, voltage, and current limits.
 * If NULL, then the partner charger is disconnected or the BC1.2 device is
 * operating in host mode.
 * @param user_data Requester supplied data which is passed along to the
 * callback.
 */
typedef void (*bc12_callback_t)(const struct device *dev,
				struct bc12_state *state, void *user_data);

__subsystem struct bc12_driver_api {
	int (*set_role)(const struct device *dev, enum bc12_role role);
	int (*result_cb)(const struct device *dev, bc12_callback_t cb,
			 void *user_data);
};

/**
 * @brief Set the BC1.2 role.
 *
 * @param dev Pointer to the device structure for the BC1.2 driver instance.
 * @param role New role for the BC1.2 device.
 *
 * @retval 0 If successful.
 * @retval -EIO general input/output error.
 */
__syscall int bc12_set_role(const struct device *dev, enum bc12_role role);

static inline int z_impl_bc12_set_role(const struct device *dev,
				       enum bc12_role role)
{
	const struct bc12_driver_api *api =
		(const struct bc12_driver_api *)dev->api;

	return api->set_role(dev, role);
}

/**
 * @brief Register a callback for BC1.2 results.
 *
 * @param dev Pointer to the device structure for the BC1.2 driver instance.
 * @param cb Function pointer for the result callback.
 * @param user_data Requester supplied data which is passed along to the
 * callback.
 *
 * @retval 0 If successful.
 * @retval -EIO general input/output error.
 */
__syscall int bc12_result_cb(const struct device *dev, bc12_callback_t cb,
			     void *user_data);

static inline int z_impl_bc12_result_cb(const struct device *dev,
					bc12_callback_t cb, void *user_data)
{
	const struct bc12_driver_api *api =
		(const struct bc12_driver_api *)dev->api;

	return api->result_cb(dev, cb, user_data);
}

#endif /* ZEPHYR_INCLUDE_DRIVERS_USB_C_USBC_BC12_H_ */
