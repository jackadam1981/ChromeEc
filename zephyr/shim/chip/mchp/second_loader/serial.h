/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

void serial_init(void);
int serial_send_host_char(int c);
bool serial_receive_host_char(uint8_t *rx_data);

#endif /* #ifndef SERIAL_H */
