/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_H__
#define __CROS_EC_H__

#include <stdbool.h>
#include <stdint.h>

void timer_handler(void);
int cec_send(uint8_t data[], int8_t byte_len);
int tv_on(int argc, char **argv);
int tv_off(int argc, char **argv);
int cec_init(void);

#endif /* __CROS_EC_H__ */
