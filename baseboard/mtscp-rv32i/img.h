/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_IMG_SRV_H
#define __CROS_EC_IMG_SRV_H

#include "ipi_chip.h"

struct img_msg {
	unsigned int id;
	unsigned char msg[CONFIG_IPC_SHARED_OBJ_BUF_SIZE];
};

BUILD_ASSERT(member_size(struct img_msg, msg) <= CONFIG_IPC_SHARED_OBJ_BUF_SIZE);

/* Functions provided by private overlay. */
void imgsys_msg_handler(void *data, unsigned int cmd_id);

#endif /* __CROS_EC_CAM_SRV_H */
