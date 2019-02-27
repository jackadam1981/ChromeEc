/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "console.h"
#include "hooks.h"
#include "link_defs.h"

#define CPUTS(outstr) cputs(CC_AUDIO_CODEC, outstr)
#define CPRINTS(format, args...) cprints(CC_AUDIO_CODEC, format, ## args)

static uint8_t audio_buf[65536];

#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
__SECTION(dram.bss)
#endif
static uint8_t lang_buf[68 * 1024];

uintptr_t audio_codec_wov_audio_buf_addr = (uintptr_t)audio_buf;
uint32_t audio_codec_wov_audio_buf_len = ARRAY_SIZE(audio_buf);
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM
uint8_t audio_codec_wov_audio_buf_type = EC_CODEC_SHM_TYPE_EC_RAM;
#endif

uintptr_t audio_codec_wov_lang_buf_addr = (uintptr_t)lang_buf;
uint32_t audio_codec_wov_lang_buf_len = ARRAY_SIZE(lang_buf);
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
uint8_t audio_codec_wov_lang_buf_type = EC_CODEC_SHM_TYPE_SYSTEM_RAM;
#endif

static void board_wov_init(void)
{
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_AUDIO,
			EC_CODEC_CAP_WOV_AUDIO_SHM,
			audio_codec_wov_audio_buf_addr,
			audio_codec_wov_audio_buf_len,
			audio_codec_wov_audio_buf_type) != EC_SUCCESS)
		CPUTS("failed to register audio shm");
#endif
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_LANG,
			EC_CODEC_CAP_WOV_LANG_SHM,
			audio_codec_wov_lang_buf_addr,
			audio_codec_wov_lang_buf_len,
			audio_codec_wov_lang_buf_type) != EC_SUCCESS)
		CPUTS("failed to register lang shm");
#endif
}
DECLARE_HOOK(HOOK_INIT, board_wov_init, HOOK_PRIO_DEFAULT + 1);
