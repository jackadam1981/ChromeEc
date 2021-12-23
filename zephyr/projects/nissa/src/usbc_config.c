/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usbc_ppc.h"

struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
};
