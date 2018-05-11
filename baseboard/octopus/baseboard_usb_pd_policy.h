/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Octopus baseboard battery configuration */

#ifndef __CROS_EC_BASEBOARD_USB_PD_H
#define __CROS_EC_BASEBOARD_USB_PD_H

#include "usb_pd.h"

void board_pd_execute_data_swap(int port, int data_role);

#endif /* __CROS_EC_BASEBOARD_USB_PD_H */
