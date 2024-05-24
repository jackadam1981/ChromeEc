/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _INTEL_MODULAR_AIC_H_
#define _INTEL_MODULAR_AIC_H_

#include <errno.h>
#include <stddef.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type for port reference
 */
typedef void *intel_modular_aic_port_t;

/**
 * @brief Port type enumeration
 */
enum intel_modular_aic_port_type {
	INTEL_MODULAR_AIC_PORT_TYPE_FIXED,
	INTEL_MODULAR_AIC_PORT_TYPE_MODULAR,
};

/**
 * @brief Port connection type enumeration
 */
enum intel_modular_aic_port_conn_type {
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_RESERVED,
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_USBA,
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_USBC,
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_HDMI,
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_DP,
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_EDP,
	INTEL_MODULAR_AIC_PORT_CONN_TYPE_MAX,
};

/**
 * @brief Port retimer status enumeration, valid when port
 * connection type is type-C
 */
enum intel_modular_aic_port_retimer_status {
	INTEL_MODULAR_AIC_PORT_RETIMER_NOT_PRESENT,
	INTEL_MODULAR_AIC_PORT_RETIMER_SINGLE,
	INTEL_MODULAR_AIC_PORT_RETIMER_DUAL,
	INTEL_MODULAR_AIC_PORT_RETIMER_RESERVED,
};

/**
 * @brief Port PD vendor controller type enumeration, valid when port
 * connection type is type-C
 */
enum intel_modular_aic_port_pd_cntrl_type {
	INTEL_MODULAR_AIC_PORT_PD_CNTRL_UNDEFINED,
	INTEL_MODULAR_AIC_PORT_PD_CNTRL_TI,
	INTEL_MODULAR_AIC_PORT_PD_CNTRL_CYPRESS,
	INTEL_MODULAR_AIC_PORT_PD_CNTRL_REALTEK,
	INTEL_MODULAR_AIC_PORT_PD_CNTRL_GOTHIC_BRIDGE,
	INTEL_MODULAR_AIC_PORT_PD_CNTRL_RESERVED,
};

/**
 * @brief Port TBT status enumeration, valid when port connection type is type-C
 */
enum intel_modular_aic_port_tbt_status {
	INTEL_MODULAR_AIC_PORT_TBT_NOT_CAPABLE,
	INTEL_MODULAR_AIC_PORT_TBT_CAPABLE,
};

/**
 * @brief Port data speed enumeration
 */
enum intel_modular_aic_port_data_speed {
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_UNDEFINED,
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_6GBPS,
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_12GBPS,
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_20GBPS,
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_40GBPS,
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_80GBPS,
	INTEL_MODULAR_AIC_PORT_DATA_SPEED_RESERVED,
};

/**
 * @brief Get number of ports available fo this slot
 *
 * @param slot Structure pointer of modular AIC slot
 *
 * @return Positive number for amount for ports connected to this slot
 *         Negative number for error
 */
int intel_modular_aic_slot_get_avail_ports(const struct device *slot);

/**
 * @brief Get reference of connected port
 *
 * @param slot Structure pointer of modular AIC slot
 * @param port_id Port index
 *
 * @return On success, returns reference of port located at specified index
 *         On error return NULL
 */
intel_modular_aic_port_t
intel_modular_aic_slot_get_port(const struct device *slot, uint8_t port_id);

/**
 * @brief Get connected port properties in raw format
 *
 * @param port Reference of port
 * @param props Variable holding propertied upon success
 *
 * @return 0 On success, negative number otherwise
 */
int intel_modular_aic_port_get_raw_properties(intel_modular_aic_port_t port,
					      uint16_t *props);

/**
 * @brief Check if device port is present
 *
 * @param port Reference of port
 *
 * @return true if device is present, false otherwise
 */
bool intel_modular_aic_port_is_present(intel_modular_aic_port_t port);

/**
 * @brief Get port type
 *
 * @param port Reference of port
 *
 * @return Modular AIC port type
 */
enum intel_modular_aic_port_type
intel_modular_aic_port_get_type(intel_modular_aic_port_t port);

/**
 * @brief Get port connection type
 *
 * @param port Reference of port
 *
 * @return Modular AIC port connection type
 */
enum intel_modular_aic_port_conn_type
intel_modular_aic_port_get_conn_type(intel_modular_aic_port_t port);

/**
 * @brief Get port retimer status
 *
 * Result of this function is only valid if port connection type is
 * INTEL_MODULAR_AIC_PORT_CONN_TYPE_USBC
 *
 * @param port Reference of port
 *
 * @return Modular AIC port retimer status
 */
enum intel_modular_aic_port_retimer_status
intel_modular_aic_port_get_retimer_status(intel_modular_aic_port_t port);

/**
 * @brief Get port PD controller type
 *
 * Result of this function is only valid if port connection type is
 * INTEL_MODULAR_AIC_PORT_CONN_TYPE_USBC
 *
 * @param port Reference of port
 *
 * @return Modular AIC port PD controller type
 */
enum intel_modular_aic_port_pd_cntrl_type
intel_modular_aic_port_get_pd_cntrl_type(intel_modular_aic_port_t port);

/**
 * @brief Get port TBT status
 *
 * Result of this function is only valid if port connection type is
 * INTEL_MODULAR_AIC_PORT_CONN_TYPE_USBC
 *
 * @param port Reference of port
 *
 * @return Modular AIC port TBT status
 */
enum intel_modular_aic_port_tbt_status
intel_modular_aic_port_get_tbt_status(intel_modular_aic_port_t port);

/**
 * @brief Get port data speed
 *
 * @param port Reference of port
 *
 * @return Modular AIC port data speed
 */
enum intel_modular_aic_port_data_speed
intel_modular_aic_port_get_data_speed(intel_modular_aic_port_t port);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _INTEL_MODULAR_AIC_H_ */
