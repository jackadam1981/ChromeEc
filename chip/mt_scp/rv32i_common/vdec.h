/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCP_VDEC_H
#define __CROS_EC_SCP_VDEC_H

#include "compile_time_macros.h"

/*
 * Video decoder supported capability
 */
#define VCODEC_CAPABILITY_4K_DISABLED BIT(4)
#define VDEC_CAP_MM21 BIT(5)
#define VDEC_CAP_MT21C BIT(6)
#define VDEC_CAP_H264_SLICE BIT(8)
#define VDEC_CAP_VP8_FRAME BIT(9)
#define VDEC_CAP_VP9_FRAME BIT(10)

enum vdec_type {
	VDEC_LAT,
	VDEC_CORE,
	VDEC_MAX,
};

struct vdec_msg {
	enum vdec_type type;
	unsigned char msg[48];
};
BUILD_ASSERT(member_size(struct vdec_msg, msg) <=
		CONFIG_IPC_SHARED_OBJ_BUF_SIZE);

/* Functions provided by private overlay. */
void vdec_core_msg_handler(void *msg);
void vdec_msg_handler(void *msg);

/* Get decoder capability. */
int vdec_get_capability(void);

#endif /* __CROS_EC_SCP_VDEC_H */
