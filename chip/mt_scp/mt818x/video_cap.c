/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "video_cap.h"

uint32_t video_get_enc_capability(void)
{
	/* not support 4K */
	return 0;
}

uint32_t video_get_dec_capability(void)
{
#ifdef HAVE_PRIVATE_MT8183
	return VCODEC_CAPABILITY_4K_DISABLED;
#else
	return VDEC_CAP_MM21 | VDEC_CAP_4K_DISABLED |
		VDEC_CAP_H264_SLICE | VDEC_CAP_VP8_FRAME |
		VDEC_CAP_VP9_FRAME;
#endif
}
