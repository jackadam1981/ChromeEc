/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Parade Tech Type-C port controller */

#ifndef __CROS_EC_USB_PD_TCPM_PS8XXX_H
#define __CROS_EC_USB_PD_TCPM_PS8XXX_H

#if defined(CONFIG_USB_PD_TCPM_PS8751)
#include "ps8751.h"

#define MUX_IN_HPD_ASSERTION_REG PS8751_REG_CTRL_1
#define IN_HPD  PS8751_REG_CTRL_1_HPD
#define HPD_IRQ PS8751_REG_CTRL_1_IRQ
#define FW_VER_REG PS8751_REG_VERSION

#elif defined(CONFIG_USB_PD_TCPM_PS8805)
#include "ps8805.h"

#define MUX_IN_HPD_ASSERTION_REG PS8805_REG_MUX_IN_HPD_ASSERTION
#define IN_HPD  PS8805_REG_MUX_IN_HPD_ASSERTION_IN_HPD
#define HPD_IRQ PS8805_REG_MUX_IN_HPD_ASSERTION_HPD_IRQ
#define FW_VER_REG PS8805_REG_FW_REVISION_ID

#endif /* defined(CONFIG_USB_PD_TCPM_PS8805) */

void ps8xxx_tcpc_update_hpd_status(int port, int hpd_lvl, int hpd_irq);
int ps8xxx_tcpc_get_fw_version(int port, int *version);

#endif /* defined(__CROS_EC_USB_PD_TCPM_PS8XXX_H) */
