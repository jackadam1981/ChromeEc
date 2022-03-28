/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for USB-C sink device emulator
 */

#ifndef __EMUL_TCPCI_PARTNER_SNK_H
#define __EMUL_TCPCI_PARTNER_SNK_H

#include <drivers/emul.h>
#include "emul/tcpc/emul_tcpci_partner_common.h"
#include "emul/tcpc/emul_tcpci.h"
#include "usb_pd.h"

/**
 * @brief USB-C sink device extension backend API
 * @defgroup tcpci_snk_emul USB-C sink device extension
 * @{
 *
 * USB-C sink device extension can be used with TCPCI partner emulator. It is
 * able to respond to some TCPM messages. It always attach as sink and present
 * sink capabilities constructed from given PDOs.
 */

/** USB-C sink device extension callbacks */
extern struct tcpci_partner_extension_ops tcpci_snk_emul_ops;

/** Structure describing sink device emulator data */
struct tcpci_snk_emul_data {
	/** Common extension structure */
	struct tcpci_partner_extension ext;
	/** Power data objects returned in sink capabilities message */
	uint32_t pdo[PDO_MAX_OBJECTS];
	/** Emulator is waiting for PS RDY message */
	bool wait_for_ps_rdy;
	/** PS RDY was received and PD negotiation is completed */
	bool pd_completed;
};

/** Initialization of TCPCI sink extension */
#define INIT_TCPCI_SNK_EMUL					\
	{							\
		.ext = {					\
			.ops = &tcpci_snk_emul_ops,		\
			.next = NULL,				\
		}						\
	}

/** Declaration of TCPCI sink extension */
#define DECLARE_TCPCI_SNK_EMUL(name)				\
	struct tcpci_snk_emul_data name = INIT_TCPCI_SNK_EMUL

/**
 * @}
 */

#endif /* __EMUL_TCPCI_PARTNER_SNK_H */
