/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @file
 * @brief PDC API for USB-C Power Management.
 */
#ifndef __CROS_EC_PDC_POWER_MGNT_H
#define __CROS_EC_PDC_POWER_MGNT_H


bool pm_is_connected(int port);
uint8_t pm_get_usb_pd_port_count(void);
int pm_set_active_charge_port(int charge_port);
enum tcpc_cc_polarity pm_pd_get_polarity(int port);
enum pd_data_role pm_pd_get_data_role(int port);
void pm_request_swap_to_src(int port);
void pm_request_swap_to_snk(int port);
void pm_request_swap_to_ufp(int port);
void pm_request_swap_to_dfp(int port);
void pm_set_new_power_request(int port);
enum pd_power_role pm_get_power_role(int port);
uint8_t pm_get_task_state(int port);
int pm_comm_is_enabled(int port);
bool pm_get_vconn_state(int port);
bool pm_get_partner_dual_role_power(int port);
bool pm_get_partner_data_swap_capable(int port);
bool pm_get_partner_usb_comm_capable(int port);
bool pm_get_partner_unconstr_power(int port);
enum pd_cc_states pm_get_task_cc_state(int port);
bool pm_pd_capable(int port);
uint32_t pm_get_vbus_voltage(int port);
void pm_reset(int port);

#endif /* __CROS_EC_PDC_POWER_MGNT_H */
