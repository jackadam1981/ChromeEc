/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "link_defs.h"

#define CPRINTS(format, args...) cprints(CC_AUDIO_CODEC, format, ## args)

/*
 * Preprocessor cannot see the enumeration "EC_CODEC_SHM_TYPE_SYSTEM_RAM".
 * Need to change the following board settings manually.
 */

/*
 * #if CONFIG_AUDIO_CODEC_WOV_AUDIO_BUF_TYPE != EC_CODEC_SHM_TYPE_SYSTEM_RAM
 * static uint8_t audio_buf[CONFIG_AUDIO_CODEC_WOV_AUDIO_BUF_LEN];
 * uintptr_t audio_codec_wov_audio_buf_addr = (uintptr_t)audio_buf;
 * #else
 * uintptr_t audio_codec_wov_audio_buf_addr;
 * #endif
 */
uintptr_t audio_codec_wov_audio_buf_addr;

/*
 * #if CONFIG_AUDIO_CODEC_WOV_LANG_BUF_TYPE != EC_CODEC_SHM_TYPE_SYSTEM_RAM
 * static uint8_t lang_buf[CONFIG_AUDIO_CODEC_WOV_LANG_BUF_LEN];
 * uintptr_t audio_codec_wov_lang_buf_addr = (uintptr_t)lang_buf;
 * #else
 * uintptr_t audio_codec_wov_lang_buf_addr;
 * #endif
 */
uintptr_t audio_codec_wov_lang_buf_addr;

static void board_wov_init(void)
{
#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_AUDIO,
			EC_CODEC_CAP_WOV_AUDIO_SHM,
			&audio_codec_wov_audio_buf_addr,
			CONFIG_AUDIO_CODEC_WOV_AUDIO_BUF_LEN,
			CONFIG_AUDIO_CODEC_WOV_AUDIO_BUF_TYPE) != EC_SUCCESS)
		CPRINTS("failed to register audio shm");
#endif

#ifdef CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM
	if (audio_codec_register_shm(EC_CODEC_SHM_ID_WOV_LANG,
			EC_CODEC_CAP_WOV_LANG_SHM,
			&audio_codec_wov_lang_buf_addr,
			CONFIG_AUDIO_CODEC_WOV_LANG_BUF_LEN,
			CONFIG_AUDIO_CODEC_WOV_LANG_BUF_TYPE) != EC_SUCCESS)
		CPRINTS("failed to register lang shm");
#endif
}
DECLARE_HOOK(HOOK_INIT, board_wov_init, HOOK_PRIO_DEFAULT);
