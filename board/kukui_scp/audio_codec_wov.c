/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "link_defs.h"
#include "memmap.h"
#include "hooks.h"

#include "audio_codec.h"

#ifndef CONFIG_AUDIO_CODEC_WOV_CAP_AUDIO_SHM
static uint8_t audio_buf[65536];
#endif

#ifdef CONFIG_AUDIO_CODEC_WOV_CAP_LANG_SHM
__SECTION(dram)
#endif
static uint8_t lang_buf[40 * 1024];

static struct audio_codec_wov_driver wov_driver = {
#ifdef CONFIG_AUDIO_CODEC_WOV_CAP_AUDIO_SHM
	.audio_buf_addr = CONFIG_AUDIO_SHM_BASE,
	.audio_buf_len = CONFIG_AUDIO_SHM_SIZE,
	.audio_buf_type = EC_CODEC_WOV_SHM_TYPE_EC_RAM,
#else
	.audio_buf_addr = (uintptr_t)audio_buf,
	.audio_buf_len = ARRAY_SIZE(audio_buf),
#endif

	.lang_buf_addr = (uintptr_t)lang_buf,
	.lang_buf_len = ARRAY_SIZE(lang_buf),
#ifdef CONFIG_AUDIO_CODEC_WOV_CAP_LANG_SHM
	.lang_buf_type = EC_CODEC_WOV_SHM_TYPE_SYSTEM_RAM,
#endif
};

static void board_wov_init(void)
{
#ifdef CONFIG_AUDIO_CODEC_WOV_CAP_LANG_SHM
	{
		uintptr_t ap_addr;

		memmap_scp_cache_to_ap((uintptr_t)lang_buf, &ap_addr);
		wov_driver.lang_buf_addr = ap_addr;
	}
#endif

	if (audio_codec_register_wov_driver(&wov_driver) != EC_SUCCESS)
		ERR("failed to register wov_driver\n");
}
DECLARE_HOOK(HOOK_INIT, board_wov_init, HOOK_PRIO_DEFAULT + 1);
