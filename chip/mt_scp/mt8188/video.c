/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "video.h"

uint32_t video_get_enc_capability(void)
{
	return VENC_CAP_4K;
}

uint32_t video_get_dec_capability(void)
{
	/*
	 * (b/332235023): Using hardware vdec for >= 2K content consumes
	 * excessive memory and causes system hang and performance degrade.
	 * Disable 4K capability temporarily until the issue is fixed.
	 */
	return VDEC_CAP_MM21 | VDEC_CAP_IS_SUPPORT_10BIT | VDEC_CAP_H264_SLICE |
	       VDEC_CAP_VP8_FRAME | VDEC_CAP_VP9_FRAME | VDEC_CAP_AV1_FRAME |
	       VDEC_CAP_HEVC_SLICE | VDEC_CAP_4K_DISABLED;
}
