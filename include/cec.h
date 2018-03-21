/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_H__
#define __CROS_EC_H__

#include <stdint.h>

int cec_init(void);
int cec_send(uint8_t data[], int8_t byte_len);

#endif /* __CROS_EC_H__ */
