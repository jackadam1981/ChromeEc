/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <devicetree.h>
#include <sys/util.h>
#include "usb_pd_tcpm.h"
#include "usb_pd.h"
#include "usbc/tcpc_it8xxx2.h"
#include "usbc/tcpc_ps8xxx.h"

#if DT_HAS_COMPAT_STATUS_OKAY(PS8XXX_COMPAT) ||               \
	DT_HAS_COMPAT_STATUS_OKAY(IT8XXX2_TCPC_COMPAT)

#define USBC_PORT(id) DT_REG_ADDR(DT_PARENT(id))

#define TCPC_CONFIG(id, fn) [USBC_PORT(id)] = fn(id)

COND_CODE_1(CONFIG_PLATFORM_EC_USB_PD_TCPC_RUNTIME_CONFIG, (), (const))
struct tcpc_config_t tcpc_config[] = {
	DT_FOREACH_STATUS_OKAY_VARGS(IT8XXX2_TCPC_COMPAT, TCPC_CONFIG,
				     TCPC_CONFIG_IT8XXX2)
	DT_FOREACH_STATUS_OKAY_VARGS(PS8XXX_COMPAT, TCPC_CONFIG,
				     TCPC_CONFIG_PS8XXX)
};

#endif /* DT_HAS_COMPAT_STATUS_OKAY */

