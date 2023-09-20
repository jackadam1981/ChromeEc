/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @file
 * @brief PDC thread for USB-C Power Management.
 */
#ifndef __CROS_EC_PDC_POWER_MGNT_H
#define __CROS_EC_PDC_POWER_MGNT_H

enum attached_state_t {
	UNATTACHED,
	SNK_ATTACHED,
	SRC_ATTACHED,
};


void tcpm_reset(void);
uint8_t tcpm_get_port_num(void);
void tcpm_reset_port(int port);

void tcpm_request_data_swap(int port);
enum attached_state_t tcpm_get_task_state(int port);
const char *tcpm_get_task_state_name(int port);
int tcpm_comm_is_enabled(int port);
bool tcpm_get_vconn_state(int port);
bool tcpm_get_partner_dual_role_power(int port);
bool tcpm_get_partner_data_swap_capable(int port);
bool tcpm_get_partner_usb_comm_capable(int port);
bool tcpm_get_partner_unconstr_power(int port);
bool tcpm_pd_capable(int port);
uint32_t tcpm_get_vbus_voltage(int port);
enum pd_power_role tcpm_get_power_role(int port);
enum pd_data_role tcpm_get_data_role(int port);

#endif /* __CROS_EC_PDC_POWER_MGNT_H */

