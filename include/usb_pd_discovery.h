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

/**
 * Checks whether port and cable discovery have gotten as far as they can.
 * Discovery is considered "done" if it completes or if it fails before it is
 * complete.
 *
 * @param port USB-C port number
 * @return true if discovery done; false otherwise
 */
bool discovery_is_done(int port);

/**
 * Initializes the discovery state machine. This function should be called after
 * attach and before running discovery.
 *
 * @param port USB-C port number
 */
void discovery_init(int port);

/*
 * Handles received discovery VDM ACKs.
 *
 * @param port      USB-C port number
 * @param type      Transmit type (SOP, SOP') for received ACK
 * @param vdo_count The number of VDOs in the ACK VDM
 * @param vdm       VDM from ACK
 */
void discovery_vdm_acked(int port, enum tcpci_msg_type type, int vdo_count,
			 uint32_t *vdm);

/**
 * Handles NAKed (or Not Supported or timed out) DisplayPort VDM requests.
 *
 * @param port    USB-C port number
 * @param type    Transmit type (SOP, SOP') for request
 * @param svid    The SVID of the request
 * @param vdm_cmd The VDM command of the request
 */
void discovery_vdm_naked(int port, enum tcpci_msg_type type, uint16_t svid,
			 uint8_t vdm_cmd);

/**
 * Checks whether a VDM is a discovery VDM. Discovery VDMs may have the USB SID
 * (Discover Identity and Discover SVIDs) or the VID of an alt mode (Discover
 * Modes).
 *
 * @param svid The SVID field of a VDM header
 * @param cmd  The command field of a VDM header
 * @return     true if the VDM is a discovery VDM; false otherwise
 */
bool discovery_vdm_is_discovery(uint16_t svid, uint8_t cmd);
