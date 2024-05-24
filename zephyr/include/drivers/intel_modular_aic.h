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

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _INTEL_MODULAR_AIC_H_ */
