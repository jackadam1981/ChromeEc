/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternate Mode Discovery storage, access, and helpers for use within the PD
 * task. Many of the names in usb_pd_discovery.c have pd_* names and are
 * intended to be called from any EC code. The functions declared here are
 * intended to be called only from within the PD task, hence their discovery_*
 * names.
 */

#include "usb_pd_tcpm.h"

#include <stdbool.h>
#include <stdint.h>

bool discovery_is_done(int port);

void discovery_init(int port);

void discovery_vdm_acked(int port, enum tcpci_msg_type type, int vdo_count,
			 uint32_t *vdm);

void discovery_vdm_naked(int port, enum tcpci_msg_type type, uint16_t svid,
			 uint8_t vdm_cmd);

bool discovery_vdm_is_discovery(uint16_t svid, uint8_t cmd);
