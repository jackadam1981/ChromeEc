/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ISP_P2_SRV_H
#define __CROS_EC_ISP_P2_SRV_H

#include "chip/mt_scp/ipi_chip.h"

struct dip_msg_service {
	unsigned char id;
	unsigned char msg[288];
};

/* Functions provided by private overlay. */
void dip_msg_handler(void *data);

#endif /* __CROS_EC_ISP_P2_SRV_H */
