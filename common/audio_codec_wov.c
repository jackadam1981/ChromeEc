/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "console.h"
#include "host_command.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_AUDIO_CODEC, format, ## args)

static uint8_t lang_hash[SHA256_DIGEST_SIZE];
static uint32_t lang_len;

/*
 * The variables below are shared between host command and WoV task.  This lock
 * is designed to protect them.
 */
static struct mutex lock;

/*
 * wov_enabled is shared.
 *
 * host command task:
 *   - is the only writer
 *   - no need to lock if read
 */
static uint8_t wov_enabled;

/*
 * hotword_detected is shared.
 */
static uint8_t hotword_detected;

/*
 * audio_buf_rp and audio_buf_wp are shared.
 *
 * Typical ring-buffer implementation:
 *   If audio_buf_rp == audio_buf_wp, empty.
 *   If (audio_buf_wp + 1) % buf_len == audio_buf_rp, full.
 */
static uint32_t audio_buf_rp, audio_buf_wp;

static int is_buf_full(void)
{
	return ((audio_buf_wp + 1) % audio_codec_wov_audio_buf_len)
		== audio_buf_rp;
}

static int check_lang_buf(uint8_t *data, uint32_t len, const uint8_t *hash)
{
	/*
	 * Note: sizeof(struct sha256_ctx) = 200 bytes
	 * should put into .bss, or stack is likely to overflow (~640 bytes)
	 */
	static struct sha256_ctx ctx;
	uint8_t *digest;
	int i;
	uint8_t *p = (uint8_t *)audio_codec_wov_lang_buf_addr;

	SHA256_init(&ctx);
	SHA256_update(&ctx, data, len);
	digest = SHA256_final(&ctx);

#ifdef DEBUG_AUDIO_CODEC
	CPRINTS("data=%08x len=%d", data, len);
	hexdump(digest, SHA256_DIGEST_SIZE);
#endif

	if (memcmp(digest, hash, SHA256_DIGEST_SIZE) != 0)
		return EC_ERROR_UNKNOWN;

	for (i = len; i < audio_codec_wov_lang_buf_len; ++i)
		if (p[i])
			return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_ACTIVE) && \
    !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_PASSIVE)
static int wov_set_lang(struct host_cmd_handler_args *args)
{
	const struct ec_param_ec_codec_wov *p = args->params;
	const struct ec_param_ec_codec_wov_set_lang *pp = &p->set_lang_param;

	if (pp->total_len > audio_codec_wov_lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (pp->offset >= audio_codec_wov_lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (pp->len > ARRAY_SIZE(pp->buf))
		return EC_RES_INVALID_PARAM;
	if (pp->offset + pp->len > pp->total_len)
		return EC_RES_INVALID_PARAM;
	if (wov_enabled)
		return EC_RES_BUSY;

	if (!pp->offset)
		memset((uint8_t *)audio_codec_wov_lang_buf_addr,
		       0, audio_codec_wov_lang_buf_len);

	memcpy((uint8_t *)audio_codec_wov_lang_buf_addr + pp->offset,
		pp->buf, pp->len);

	if (pp->offset + pp->len == pp->total_len) {
		if (check_lang_buf((uint8_t *)audio_codec_wov_lang_buf_addr,
				   pp->total_len, pp->hash) != EC_SUCCESS)
			return EC_RES_ERROR;

		memcpy(lang_hash, pp->hash, sizeof(lang_hash));
		lang_len = pp->total_len;
	}

	args->response_size = 0;
	return EC_RES_SUCCESS;
}
#else
static int wov_set_lang_shm(struct host_cmd_handler_args *args)
{
	const struct ec_param_ec_codec_wov *p = args->params;
	const struct ec_param_ec_codec_wov_set_lang_shm *pp =
			&p->set_lang_shm_param;

	if (pp->total_len > audio_codec_wov_lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (wov_enabled)
		return EC_RES_BUSY;

	if (check_lang_buf((uint8_t *)audio_codec_wov_lang_buf_addr,
			   pp->total_len, pp->hash) != EC_SUCCESS)
		return EC_RES_ERROR;

	memcpy(lang_hash, pp->hash, sizeof(lang_hash));
	lang_len = pp->total_len;

	args->response_size = 0;
	return EC_RES_SUCCESS;
}
#endif /* !CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_* */

static int wov_get_lang(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_lang *r = args->response;

	memcpy(r->hash, lang_hash, sizeof(r->hash));

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int wov_enable(struct host_cmd_handler_args *args)
{
	if (wov_enabled)
		return EC_RES_BUSY;

	if (audio_codec_wov_enable() != EC_SUCCESS)
		return EC_RES_ERROR;

	mutex_lock(&lock);
	wov_enabled = 1;
	hotword_detected = 0;
	audio_buf_rp = audio_buf_wp = 0;
	mutex_unlock(&lock);

#ifdef HAS_TASK_WOV
	task_wake(TASK_ID_WOV);
#endif

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_disable(struct host_cmd_handler_args *args)
{
	if (!wov_enabled)
		return EC_RES_BUSY;

	if (audio_codec_wov_disable() != EC_SUCCESS)
		return EC_RES_ERROR;

	mutex_lock(&lock);
	wov_enabled = 0;
	hotword_detected = 0;
	audio_buf_rp = audio_buf_wp = 0;
	mutex_unlock(&lock);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE) && \
    !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_PASSIVE)
static int wov_read_audio(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio *r = args->response;
	uint8_t *p;

	if (!wov_enabled)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&lock);
	if (!hotword_detected) {
		mutex_unlock(&lock);
		return EC_RES_ACCESS_DENIED;
	}

	if (audio_buf_rp <= audio_buf_wp)
		r->len = audio_buf_wp - audio_buf_rp;
	else
		r->len = audio_codec_wov_audio_buf_len - audio_buf_rp;
	r->len = MIN(sizeof(r->buf), r->len);

	p = (uint8_t *)audio_codec_wov_audio_buf_addr + audio_buf_rp;

	audio_buf_rp += r->len;
	if (audio_buf_rp == audio_codec_wov_audio_buf_len)
		audio_buf_rp = 0;
	mutex_unlock(&lock);

#ifdef DEBUG_AUDIO_CODEC
	if (!r->len)
		CPRINTS("underrun detected");
#endif
	memcpy(r->buf, p, r->len);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
#else
static int wov_read_audio_shm(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio_shm *r = args->response;

	if (!wov_enabled)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&lock);
	if (!hotword_detected) {
		mutex_unlock(&lock);
		return EC_RES_ACCESS_DENIED;
	}

	r->offset = audio_buf_rp;
	if (audio_buf_rp <= audio_buf_wp)
		r->len = audio_buf_wp - audio_buf_rp;
	else
		r->len = audio_codec_wov_audio_buf_len - audio_buf_rp;

	audio_buf_rp += r->len;
	if (audio_buf_rp == audio_codec_wov_audio_buf_len)
		audio_buf_rp = 0;
	mutex_unlock(&lock);

#ifdef DEBUG_AUDIO_CODEC
	if (!r->len)
		CPRINTS("underrun detected");
#endif

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
#endif /* !CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_* */

static int (*sub_cmds[])(struct host_cmd_handler_args *) = {
#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_ACTIVE) && \
    !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_PASSIVE)
	[EC_CODEC_WOV_SET_LANG] = wov_set_lang,
#else
	[EC_CODEC_WOV_SET_LANG_SHM] = wov_set_lang_shm,
#endif
	[EC_CODEC_WOV_GET_LANG] = wov_get_lang,
	[EC_CODEC_WOV_ENABLE] = wov_enable,
	[EC_CODEC_WOV_DISABLE] = wov_disable,
#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE) && \
    !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_PASSIVE)
	[EC_CODEC_WOV_READ_AUDIO] = wov_read_audio,
#else
	[EC_CODEC_WOV_READ_AUDIO_SHM] = wov_read_audio_shm,
#endif
};

#ifdef DEBUG_AUDIO_CODEC
static char *strcmd[EC_CODEC_WOV_SUBCMD_COUNT] = {
#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_ACTIVE) && \
    !defined(CONFIG_AUDIO_CODEC_CAP_WOV_LANG_SHM_PASSIVE)
	[EC_CODEC_WOV_SET_LANG] = "EC_CODEC_WOV_SET_LANG",
#else
	[EC_CODEC_WOV_SET_LANG_SHM] = "EC_CODEC_WOV_SET_LANG_SHM",
#endif
	[EC_CODEC_WOV_GET_LANG] = "EC_CODEC_WOV_GET_LANG",
	[EC_CODEC_WOV_ENABLE] = "EC_CODEC_WOV_ENABLE",
	[EC_CODEC_WOV_DISABLE] = "EC_CODEC_WOV_DISABLE",
#if !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_ACTIVE) && \
    !defined(CONFIG_AUDIO_CODEC_CAP_WOV_AUDIO_SHM_PASSIVE)
	[EC_CODEC_WOV_READ_AUDIO] = "EC_CODEC_WOV_READ_AUDIO",
#else
	[EC_CODEC_WOV_READ_AUDIO_SHM] = "EC_CODEC_WOV_READ_AUDIO_SHM",
#endif
};
BUILD_ASSERT(ARRAY_SIZE(sub_cmds) == ARRAY_SIZE(strcmd));
#endif

static int wov_host_command(struct host_cmd_handler_args *args)
{
	const struct ec_param_ec_codec_wov *p = args->params;

#ifdef DEBUG_AUDIO_CODEC
	CPRINTS("WoV subcommand: %s", strcmd[p->cmd]);
#endif

	if (p->cmd < EC_CODEC_WOV_SUBCMD_COUNT && sub_cmds[p->cmd])
		return sub_cmds[p->cmd](args);

	return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_WOV, wov_host_command, EC_VER_MASK(0));

/*
 * Exported interfaces.
 */
void audio_codec_wov_task(void *arg)
{
	uint32_t n, req;
	uint8_t *p;

	while (1) {
		mutex_lock(&lock);
		if (!wov_enabled) {
			mutex_unlock(&lock);
			task_wait_event(-1);
			continue;
		}

#ifdef DEBUG_AUDIO_CODEC
		if (hotword_detected && is_buf_full())
			CPRINTS("overrun detected");
#endif

		if (!hotword_detected ||
				is_buf_full() ||
				audio_buf_wp >= audio_buf_rp)
			req = audio_codec_wov_audio_buf_len - audio_buf_wp;
		else
			req = audio_buf_rp - audio_buf_wp - 1;

		p = (uint8_t *)audio_codec_wov_audio_buf_addr + audio_buf_wp;
		mutex_unlock(&lock);

		n = audio_codec_wov_read(p, req);
		if (n < 0) {
			CPRINTS("failed to read: %d", n);
			break;
		} else if (n == 0) {
			if (audio_codec_wov_enable_notifier() != EC_SUCCESS) {
				CPRINTS("failed to enable_notifier");
				break;
			}

			task_wait_event(-1);
			continue;
		}

		mutex_lock(&lock);
		audio_buf_wp += n;
		if (audio_buf_wp == audio_codec_wov_audio_buf_len)
			audio_buf_wp = 0;
		mutex_unlock(&lock);

		usleep(1000);
	}
}
