/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

extern void ser_init(void);
extern int send_host_char(int c);
extern int send_dbg_char(int c);
extern bool receive_host_char(uint8_t *rx_data);

#endif /* #ifndef SERIAL_H */
