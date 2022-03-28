/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for USB-C dual role device emulator
 */

#ifndef __EMUL_TCPCI_PARTNER_DRP_H
#define __EMUL_TCPCI_PARTNER_DRP_H

#include <drivers/emul.h>
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_common.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "usb_pd.h"

/**
 * @brief USB-C dual role device extension backend API
 * @defgroup tcpci_snk_emul USB-C dual role device extension
 * @{
 *
 * USB-C DRP device emulator can be used with TCPCI partner emulator. It is able
 * to switch power role on PR SWAP message. It should be before sink and source
 * extensions on the TCPCI partner emulator extensions list. For each extension
 * with capabilities PDO, for first PDO should be used function
 * @ref tcpci_drp_emul_set_dr_in_first_pdo to select correct flag specific for
 * DRP device.
 */

/** USB-C DRP device extension callbacks */
extern struct tcpci_partner_extension_ops tcpci_drp_emul_ops;

/** Structure describing dual role device emulator data */
struct tcpci_drp_emul_data {
	/** Common extension structure */
	struct tcpci_partner_extension ext;
	/** Controls if device is sink or source */
	bool sink;
	/** If device is during power swap and is expecting PS_RDY message */
	bool in_pwr_swap;
};

/** Initialization of TCPCI DRP extension */
#define INIT_TCPCI_DRP_EMUL					\
	{							\
		.ext = {					\
			.ops = &tcpci_drp_emul_ops,		\
			.next = NULL,				\
		}						\
	}

/** Declaration of TCPCI DRP extension */
#define DECLARE_TCPCI_DRP_EMUL(name)				\
	struct tcpci_drp_emul_data name = INIT_TCPCI_DRP_EMUL

/**
 * @brief Set correct flags for first capabilities PDO to indicate that this
 *        device is power swap capable.
 *
 * @param pdo capability entry to change
 */
void tcpci_drp_emul_set_dr_in_first_pdo(uint32_t *pdo);

/**
 * @}
 */

#endif /* __EMUL_TCPCI_PARTNER_DRP_H */
