/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "link_defs.h"
#include "hooks.h"

#include "audio_codec.h"

static uint8_t audio_buf[65536];

#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
__SECTION(dram.bss)
#endif
static uint8_t lang_buf[40 * 1024];

static struct audio_codec_wov_driver wov_driver = {
	.audio_buf_addr = (uintptr_t)audio_buf,
	.audio_buf_len = ARRAY_SIZE(audio_buf),
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM
	.audio_buf_type = EC_CODEC_SHM_TYPE_EC_RAM,
#endif

	.lang_buf_addr = (uintptr_t)lang_buf,
	.lang_buf_len = ARRAY_SIZE(lang_buf),
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
	.lang_buf_type = EC_CODEC_SHM_TYPE_SYSTEM_RAM,
#endif
};

static void board_wov_init(void)
{
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_AUDIO,
			EC_CODEC_CAP_WOV_AUDIO_SHM,
			wov_driver.audio_buf_addr, wov_driver.audio_buf_len,
			wov_driver.audio_buf_type) != EC_SUCCESS)
		ERR("failed to register audio shm");
#endif
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_LANG,
			EC_CODEC_CAP_WOV_LANG_SHM,
			wov_driver.lang_buf_addr, wov_driver.lang_buf_len,
			wov_driver.lang_buf_type) != EC_SUCCESS)
		ERR("failed to register lang shm");
#endif
	if (audio_codec_register_wov_driver(&wov_driver) != EC_SUCCESS)
		ERR("failed to register wov_driver");

	DBG("%s", __func__);
}
DECLARE_HOOK(HOOK_INIT, board_wov_init, HOOK_PRIO_DEFAULT + 1);
