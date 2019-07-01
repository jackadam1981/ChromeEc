/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "console.h"
#include "host_command.h"
#include "hotword_dsp_api.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_AUDIO_CODEC, outstr)
#define CPRINTS(format, args...) cprints(CC_AUDIO_CODEC, format, ## args)

static uint8_t lang_hash[SHA256_DIGEST_SIZE];
static uint32_t lang_len;

/*
 * Some states are shared between host command and WoV task.  This lock is
 * designed to protect them.
 */
static struct mutex lock;

/* shared between host command and task */
static uint8_t wov_enabled;
static uint8_t hotword_detected;
static uint32_t audio_buf_rp, audio_buf_wp;

/* only used by host command */
static uint8_t speech_lib_loaded;

static int check_lang_hash(uint8_t *data, uint32_t len, uint8_t *hash)
{
	/*
	 * Note: sizeof(struct sha256_ctx) = 200 bytes
	 * should put into .bss, or stack is likely to overflow (~640 bytes)
	 */
	static struct sha256_ctx ctx;
	uint8_t *digest;

	SHA256_init(&ctx);
	SHA256_update(&ctx, data, len);
	digest = SHA256_final(&ctx);

#ifdef DEBUG_AUDIO_CODEC
	CPRINTS("data=%08x len=%d", data, len);
	hexdump(digest, SHA256_DIGEST_SIZE);
#endif

	if (memcmp(digest, hash, SHA256_DIGEST_SIZE) == 0)
		return EC_SUCCESS;

	return EC_ERROR_INVAL;
}

static int wov_set_lang(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang *pp = &p->set_lang_param;

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

	memcpy((uint8_t *)audio_codec_wov_lang_buf_addr + pp->offset,
		pp->buf, pp->len);

	if (pp->offset + pp->len == pp->total_len) {
		if (check_lang_hash((uint8_t *)audio_codec_wov_lang_buf_addr,
				    pp->total_len, pp->hash) != EC_SUCCESS)
			return EC_RES_INVALID_PARAM;

		memcpy(lang_hash, pp->hash, ARRAY_SIZE(lang_hash));
		lang_len = pp->total_len;
		speech_lib_loaded = 0;
	}

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_set_lang_shm(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang_shm *pp = &p->set_lang_shm_param;

	if (pp->total_len > audio_codec_wov_lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (wov_enabled)
		return EC_RES_BUSY;

	if (check_lang_hash((uint8_t *)audio_codec_wov_lang_buf_addr,
			    pp->total_len, pp->hash) != EC_SUCCESS)
		return EC_RES_INVALID_PARAM;

	memcpy(lang_hash, pp->hash, ARRAY_SIZE(lang_hash));
	lang_len = pp->total_len;
	speech_lib_loaded = 0;

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_get_lang(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_lang *r = args->response;

	memcpy(r->hash, lang_hash, ARRAY_SIZE(r->hash));

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int wov_enable(struct host_cmd_handler_args *args)
{
	if (wov_enabled)
		return EC_RES_BUSY;

	if (audio_codec_wov_enable() != EC_SUCCESS)
		return EC_RES_ERROR;

	if (!speech_lib_loaded) {
		if (!GoogleHotwordDspInit(
				(void *)audio_codec_wov_lang_buf_addr))
			return EC_RES_ERROR;
		speech_lib_loaded = 1;
	} else {
		GoogleHotwordDspReset();
	}

	mutex_lock(&lock);
	wov_enabled = 1;
	hotword_detected = 0;
	audio_buf_rp = audio_buf_wp = 0;
	mutex_unlock(&lock);

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

static int wov_read_audio(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio *r = args->response;

	if (!wov_enabled)
		return EC_RES_ACCESS_DENIED;
	if (!hotword_detected)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&lock);
	if (audio_buf_rp <= audio_buf_wp)
		r->len = MIN(ARRAY_SIZE(r->buf),
			     audio_buf_wp - audio_buf_rp);
	else
		r->len = MIN(ARRAY_SIZE(r->buf),
			     audio_codec_wov_audio_buf_len - audio_buf_rp);
	mutex_unlock(&lock);

	memcpy(r->buf,
	       (uint8_t *)audio_codec_wov_audio_buf_addr + audio_buf_rp,
	       r->len);

	mutex_lock(&lock);
	audio_buf_rp += r->len;
	if (audio_buf_rp == audio_codec_wov_audio_buf_len)
		audio_buf_rp = 0;
	mutex_unlock(&lock);

	if (!r->len)
		CPUTS("underrun detected");

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int wov_read_audio_shm(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio_shm *r = args->response;

	if (!wov_enabled)
		return EC_RES_ACCESS_DENIED;
	if (!hotword_detected)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&lock);
	r->offset = audio_buf_rp;
	if (audio_buf_rp <= audio_buf_wp)
		r->len = audio_buf_wp - audio_buf_rp;
	else
		r->len = audio_codec_wov_audio_buf_len - audio_buf_rp;

	audio_buf_rp += r->len;
	if (audio_buf_rp == audio_codec_wov_audio_buf_len)
		audio_buf_rp = 0;
	mutex_unlock(&lock);

	if (!r->len)
		CPUTS("underrun detected");

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int (*sub_cmds[])(struct host_cmd_handler_args *) = {
	[EC_CODEC_WOV_SET_LANG] = wov_set_lang,
	[EC_CODEC_WOV_SET_LANG_SHM] = wov_set_lang_shm,
	[EC_CODEC_WOV_GET_LANG] = wov_get_lang,
	[EC_CODEC_WOV_ENABLE] = wov_enable,
	[EC_CODEC_WOV_DISABLE] = wov_disable,
	[EC_CODEC_WOV_READ_AUDIO] = wov_read_audio,
	[EC_CODEC_WOV_READ_AUDIO_SHM] = wov_read_audio_shm,
};

#ifdef DEBUG_AUDIO_CODEC
static char *strcmd[EC_CODEC_WOV_SUBCMD_COUNT] = {
	[EC_CODEC_WOV_SET_LANG] = "EC_CODEC_WOV_SET_LANG",
	[EC_CODEC_WOV_SET_LANG_SHM] = "EC_CODEC_WOV_SET_LANG_SHM",
	[EC_CODEC_WOV_GET_LANG] = "EC_CODEC_WOV_GET_LANG",
	[EC_CODEC_WOV_ENABLE] = "EC_CODEC_WOV_ENABLE",
	[EC_CODEC_WOV_DISABLE] = "EC_CODEC_WOV_DISABLE",
	[EC_CODEC_WOV_READ_AUDIO] = "EC_CODEC_WOV_READ_AUDIO",
	[EC_CODEC_WOV_READ_AUDIO_SHM] = "EC_CODEC_WOV_READ_AUDIO_SHM",
};
BUILD_ASSERT(ARRAY_SIZE(sub_cmds) == ARRAY_SIZE(strcmd));
#endif

static int wov_host_command(struct host_cmd_handler_args *args)
{
	const struct ec_param_ec_codec_wov *p = args->params;

#ifdef DEBUG_AUDIO_CODEC
	CPRINTS("WoV subcommand: %s", strcmd[p->cmd]);
#endif

	if (p->cmd < EC_CODEC_WOV_SUBCMD_COUNT)
		return sub_cmds[p->cmd](args);

	return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_WOV, wov_host_command, EC_VER_MASK(0));

static void wov_read_cb(void *priv_data)
{
	/* Note: in interrupt context */

	/* ec.tasklist in board-specific must use the name "WOV" */
#ifdef HAS_TASK_WOV
	task_wake(TASK_ID_WOV);
#endif
}

/*
 * Exported interfaces.
 */
void audio_codec_wov_task(void *arg)
{
	uint32_t n, req;
	uint8_t *p = (uint8_t *)audio_codec_wov_audio_buf_addr;
	int r;

	audio_codec_wov_set_read_notifiee(wov_read_cb, NULL);

	while (1) {
		if (!wov_enabled)
			goto next_round;

		mutex_lock(&lock);
		if (audio_buf_wp >= audio_buf_rp)
			req = audio_codec_wov_audio_buf_len - audio_buf_wp;
		else
			req = audio_buf_rp - audio_buf_wp - 1;
		mutex_unlock(&lock);

		if (!req) {
			/*
			 * `hotword_detected` should also be protected, but we
			 * can simply ignore the case (debug only).
			 */
			if (hotword_detected)
				CPUTS("overrun detected");
			req = audio_codec_wov_audio_buf_len - audio_buf_wp;
		}

		/* read even if not enabled, in order to consume the buffer */
		n = audio_codec_wov_read(p + audio_buf_wp, req);
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

		if (GoogleHotwordDspProcess(p + audio_buf_wp, n / 2, &r)) {
			CPUTS("hotword detected");
			hotword_detected = 1;
			host_set_single_event(EC_HOST_EVENT_WOV);
			GoogleHotwordDspReset();
		}

		mutex_lock(&lock);
		audio_buf_wp += n;
		if (audio_buf_wp == audio_codec_wov_audio_buf_len)
			audio_buf_wp = 0;
		mutex_unlock(&lock);

next_round:
		usleep(1000);
	}
}
