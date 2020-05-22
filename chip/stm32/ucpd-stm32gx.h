/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_UCPD_STM32GX_H
#define __CROS_EC_UCPD_STM32GX_H

#include "usb_pd_tcpm.h"

/* STM32 UCPD driver for Chrome EC */
int stm32gx_ucpd_init(int port);
int stm32gx_ucpd_release(int port);
int stm32gx_ucpd_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
			enum tcpc_cc_voltage_status *cc2);
int stm32gx_ucpd_get_role_control(int port);
int stm32gx_ucpd_set_cc(int port, int cc_pull, int rp);
int stm32gx_ucpd_set_polarity(int port, enum tcpc_cc_polarity polarity);

#endif /* __CROS_EC_UCPD_STM32GX_H */
