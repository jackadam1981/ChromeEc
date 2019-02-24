/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "util.h"
#include "vdec.h"

void vdec_h264_service_init(void) {
	ccprints("Doing %s", __func__);
}

void vdec_h264_msg_handler(void* data) {
	ccprints("Doing %s %p", __func__, data);
}
