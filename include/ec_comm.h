/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Header file for the EC command handler
 */

#include "stdint.h"

/**
 * Callback called by EC UART RX handler (i.e. send_data_to_usb)
 *
 * It search in EC UART RX buffer for packet mode request marker (0xec ...).
 * After packet mode is enabled, it copies data to the packet handler's
 * buffer for processing packets.
 *
 * buffer: Buffer containing received data
 * len:    Size of received data
 *
 * Return:
 * If it returns 0, data is forwarded to USB port (if CCD is on). If it returns
 * 1, data is not forwarded.
 */
int packet_mode_is_enabled(uint8_t *buffer, int len);
