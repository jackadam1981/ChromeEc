/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _MODULAR_AIC_H_
#define _MODULAR_AIC_H_

#include <errno.h>
#include <stddef.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type for port reference
 */
typedef void *modular_aic_port_t;

/**
 * @brief Port type enumeration
 */
enum modular_aic_port_type {
	MODULAR_AIC_PORT_TYPE_FIXED,
	MODULAR_AIC_PORT_TYPE_MODULAR,
};

/**
 * @brief Port connection type enumeration
 */
enum modular_aic_port_conn_type {
	MODULAR_AIC_PORT_CONN_TYPE_RESERVED,
	MODULAR_AIC_PORT_CONN_TYPE_TYPEA,
	MODULAR_AIC_PORT_CONN_TYPE_TYPEC,
	MODULAR_AIC_PORT_CONN_TYPE_HDMI,
	MODULAR_AIC_PORT_CONN_TYPE_DP,
	MODULAR_AIC_PORT_CONN_TYPE_EDP,
	MODULAR_AIC_PORT_CONN_TYPE_MAX,
};

/**
 * @brief Port retimer status enumeration, valid when port
 * connection type is type-C
 */
enum modular_aic_port_retimer_status {
	MODULAR_AIC_PORT_RETIMER_NOT_PRESENT,
	MODULAR_AIC_PORT_RETIMER_SINGLE,
	MODULAR_AIC_PORT_RETIMER_DUAL,
	MODULAR_AIC_PORT_RETIMER_RESERVED,
};

/**
 * @brief Port PD vendor controller type enumeration, valid when port
 * connection type is type-C
 */
enum modular_aic_port_pd_cntrl_type {
	MODULAR_AIC_PORT_PD_CNTRL_UNDEFINED,
	MODULAR_AIC_PORT_PD_CNTRL_TI,
	MODULAR_AIC_PORT_PD_CNTRL_CYPRESS,
	MODULAR_AIC_PORT_PD_CNTRL_REALTEK,
	MODULAR_AIC_PORT_PD_CNTRL_GOTHIC_BRIDGE,
	MODULAR_AIC_PORT_PD_CNTRL_RESERVED,
};

/**
 * @brief Port TBT status enumeration, valid when port connection type is type-C
 */
enum modular_aic_port_tbt_status {
	MODULAR_AIC_PORT_TBT_NOT_CAPABLE,
	MODULAR_AIC_PORT_TBT_CAPABLE,
};

/**
 * @brief Port data speed enumeration
 */
enum modular_aic_port_data_speed {
	MODULAR_AIC_PORT_DATA_SPEED_UNDEFINED,
	MODULAR_AIC_PORT_DATA_SPEED_6GBPS,
	MODULAR_AIC_PORT_DATA_SPEED_12GBPS,
	MODULAR_AIC_PORT_DATA_SPEED_20GBPS,
	MODULAR_AIC_PORT_DATA_SPEED_40GBPS,
	MODULAR_AIC_PORT_DATA_SPEED_80GBPS,
	MODULAR_AIC_PORT_DATA_SPEED_RESERVED,
};

/**
 * @brief Get number of ports available fo this slot
 *
 * @param slot Structure pointer of modular AIC slot
 *
 * @return Positive number for amount for ports connected to this slot
 *         Negative number for error
 */
int modular_aic_slot_get_avail_ports(const struct device *slot);

/**
 * @brief Get reference of connected port
 *
 * @param slot Structure pointer of modular AIC slot
 * @param port_id Port index
 *
 * @return On success, returns reference of port located at specified index
 *         On error return NULL
 */
modular_aic_port_t modular_aic_slot_get_port(const struct device *slot,
					     uint8_t port_id);

/**
 * @brief Get connected port properties in raw format
 *
 * @param port Reference of port
 * @param props Variable holding propertied upon success
 *
 * @return 0 On succes, negative number otherwise
 */
int modular_aic_port_get_raw_properties(modular_aic_port_t port,
					uint16_t *props);

/**
 * @brief Check if device port is present
 *
 * @param port Reference of port
 *
 * @return true if device is present, false otherwise
 */
bool modular_aic_port_is_present(modular_aic_port_t port);

/**
 * @brief Get port type
 *
 * @param port Reference of port
 *
 * @return Modular AIC port type
 */
enum modular_aic_port_type modular_aic_port_get_type(modular_aic_port_t port);

/**
 * @brief Get port connection type
 *
 * @param port Reference of port
 *
 * @return Modular AIC port connection type
 */
enum modular_aic_port_conn_type
modular_aic_port_get_conn_type(modular_aic_port_t port);

/**
 * @brief Get port retimer status
 *
 * Result of this function is only valid if port connection type is
 * MODULAR_AIC_PORT_CONN_TYPE_TYPEC
 *
 * @param port Reference of port
 *
 * @return Modular AIC port retimer status
 */
enum modular_aic_port_retimer_status
modular_aic_port_get_retimer_status(modular_aic_port_t port);

/**
 * @brief Get port PD controller type
 *
 * Result of this function is only valid if port connection type is
 * MODULAR_AIC_PORT_CONN_TYPE_TYPEC
 *
 * @param port Reference of port
 *
 * @return Modular AIC port PD controller type
 */
enum modular_aic_port_pd_cntrl_type
modular_aic_port_get_pd_cntrl_type(modular_aic_port_t port);

/**
 * @brief Get port TBT status
 *
 * Result of this function is only valid if port connection type is
 * MODULAR_AIC_PORT_CONN_TYPE_TYPEC
 *
 * @param port Reference of port
 *
 * @return Modular AIC port TBT status
 */
enum modular_aic_port_tbt_status
modular_aic_port_get_tbt_status(modular_aic_port_t port);

/**
 * @brief Get port data speed
 *
 * @param port Reference of port
 *
 * @return Modular AIC port data speed
 */
enum modular_aic_port_data_speed
modular_aic_port_get_data_speed(modular_aic_port_t port);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _MODULAR_AIC_H_ */
