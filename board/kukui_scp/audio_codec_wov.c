/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "console.h"
#include "hooks.h"
#include "link_defs.h"

#define CPRINTS(format, args...) cprints(CC_AUDIO_CODEC, format, ## args)

#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_PASSIVE)
static uint8_t audio_buf[CONFIG_AUDIO_CODEC_WOV_AUDIO_BUF_LEN];
uintptr_t audio_codec_wov_audio_buf_addr = (uintptr_t)audio_buf;
uint32_t audio_codec_wov_audio_buf_len = CONFIG_AUDIO_CODEC_WOV_AUDIO_BUF_LEN;
#else
uintptr_t audio_codec_wov_audio_buf_addr;
uint32_t audio_codec_wov_audio_buf_len;
#endif

#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_PASSIVE)
static uint8_t lang_buf[CONFIG_AUDIO_CODEC_WOV_LANG_BUF_LEN];
uintptr_t audio_codec_wov_lang_buf_addr = (uintptr_t)lang_buf;
uint32_t audio_codec_wov_lang_buf_len = CONFIG_AUDIO_CODEC_WOV_LANG_BUF_LEN;
#else
uintptr_t audio_codec_wov_lang_buf_addr;
uint32_t audio_codec_wov_lang_buf_len;
#endif

static void board_wov_init(void)
{
#if defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE) || \
    defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_PASSIVE)
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_AUDIO,
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE
			EC_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE,
#else
			EC_CODEC_CAP_WOV_AUDIO_SHM_PASSIVE,
#endif
			&audio_codec_wov_audio_buf_addr,
			&audio_codec_wov_audio_buf_len,
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE
			EC_CODEC_SHM_TYPE_EC_RAM
#else
			EC_CODEC_SHM_TYPE_SYSTEM_RAM
#endif
			) != EC_SUCCESS)
		CPRINTS("failed to register audio shm");
#endif

#if defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_ACTIVE) || \
    defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_PASSIVE)
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_LANG,
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_ACTIVE
			EC_CODEC_CAP_WOV_LANG_SHM_ACTIVE,
#else
			EC_CODEC_CAP_WOV_LANG_SHM_PASSIVE,
#endif
			&audio_codec_wov_lang_buf_addr,
			&audio_codec_wov_lang_buf_len,
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_ACTIVE
			EC_CODEC_SHM_TYPE_EC_RAM
#else
			EC_CODEC_SHM_TYPE_SYSTEM_RAM
#endif
			) != EC_SUCCESS)
		CPRINTS("failed to register lang shm");
#endif
}
DECLARE_HOOK(HOOK_INIT, board_wov_init, HOOK_PRIO_DEFAULT);
