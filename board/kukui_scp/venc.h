/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCP_VENC_H
#define __CROS_EC_SCP_VENC_H

#include "chip/mt_scp/registers.h"
#include "queue.h"

enum venc_type {
	VENC_H264,
	VENC_MAX,
};

typedef void (*venc_msg_handler)(void *msg);

struct venc_service {
	enum venc_type type;
	unsigned char msg[48];
	venc_msg_handler handler[VENC_MAX];
};

/* Functions provided by private overlay. */
void venc_h264_service_init(void);
void venc_h264_msg_handler(void *data);

#endif /* __CROS_EC_SCP_VENC_H */
