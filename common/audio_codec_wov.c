/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "host_command.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "util.h"

static struct {
	struct audio_codec_wov_driver driver;

	uint8_t lang_hash[32];
	uint32_t lang_len;

	struct mutex lock;

	/* shared between host command and task */
	uint8_t wov_enabled;
	uint8_t hotword_detected;
	uint32_t audio_buf_rp, audio_buf_wp;
} priv;

static int check_lang_hash(uint8_t *data, uint32_t len, uint8_t *hash)
{
	/*
	 * Note: sizeof(struct sha256_ctx) = 200 bytes
	 * should put into .bss, or stack is likely to overflow (~640 bytes)
	 */
	static struct sha256_ctx ctx;
	uint8_t *digest;

	DBG("data=%08x len=%d", data, len);

	SHA256_init(&ctx);
	SHA256_update(&ctx, data, len);
	digest = SHA256_final(&ctx);

#ifdef DEBUG_AUDIO_CODEC
	hexdump(digest, 32);
#endif

	if (memcmp(digest, hash, 32) == 0)
		return EC_SUCCESS;

	return EC_ERROR_INVAL;
}

static int wov_set_lang(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang *pp = &p->set_lang_param;

	if (pp->total_len > priv.driver.lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (pp->offset >= priv.driver.lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (pp->len > ARRAY_SIZE(pp->buf))
		return EC_RES_INVALID_PARAM;
	if (pp->offset + pp->len > pp->total_len)
		return EC_RES_INVALID_PARAM;
	if (priv.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	memcpy((uint8_t *)priv.driver.lang_buf_addr + pp->offset,
		pp->buf, pp->len);

	if (pp->offset + pp->len == pp->total_len) {
		if (check_lang_hash((uint8_t *)priv.driver.lang_buf_addr,
				    pp->total_len, pp->hash) != EC_SUCCESS)
			return EC_RES_INVALID_PARAM;

		memcpy(priv.lang_hash, pp->hash, ARRAY_SIZE(priv.lang_hash));
		priv.lang_len = pp->total_len;
	}

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_set_lang_shm(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang_shm *pp = &p->set_lang_shm_param;

	if (pp->total_len > priv.driver.lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (priv.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	if (check_lang_hash((uint8_t *)priv.driver.lang_buf_addr,
			    pp->total_len, pp->hash) != EC_SUCCESS)
		return EC_RES_INVALID_PARAM;

	memcpy(priv.lang_hash, pp->hash, ARRAY_SIZE(priv.lang_hash));
	priv.lang_len = pp->total_len;

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_get_lang(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_lang *r = args->response;

	memcpy(r->hash, priv.lang_hash, ARRAY_SIZE(r->hash));

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int wov_enable(struct host_cmd_handler_args *args)
{
	if (priv.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	if (priv.driver.enable() != EC_SUCCESS)
		return EC_RES_ERROR;

	mutex_lock(&priv.lock);
	priv.wov_enabled = 1;
	priv.hotword_detected = 0;
	priv.audio_buf_rp = priv.audio_buf_wp = 0;
	mutex_unlock(&priv.lock);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_disable(struct host_cmd_handler_args *args)
{
	if (!priv.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	if (priv.driver.disable() != EC_SUCCESS)
		return EC_RES_ERROR;

	mutex_lock(&priv.lock);
	priv.wov_enabled = 0;
	priv.hotword_detected = 0;
	priv.audio_buf_rp = priv.audio_buf_wp = 0;
	mutex_unlock(&priv.lock);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int wov_read_audio(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio *r = args->response;

	if (!priv.wov_enabled)
		return EC_RES_ACCESS_DENIED;
	if (!priv.hotword_detected)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&priv.lock);
	if (priv.audio_buf_rp <= priv.audio_buf_wp)
		r->len = MIN(ARRAY_SIZE(r->buf),
			     priv.audio_buf_wp - priv.audio_buf_rp);
	else
		r->len = MIN(ARRAY_SIZE(r->buf),
			     priv.driver.audio_buf_len - priv.audio_buf_rp);
	mutex_unlock(&priv.lock);

	memcpy(r->buf,
	       (uint8_t *)priv.driver.audio_buf_addr + priv.audio_buf_rp,
	       r->len);

	mutex_lock(&priv.lock);
	priv.audio_buf_rp += r->len;
	if (priv.audio_buf_rp == priv.driver.audio_buf_len)
		priv.audio_buf_rp = 0;
	mutex_unlock(&priv.lock);

	if (!r->len)
		WARN("underrun detected");

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int wov_read_audio_shm(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio_shm *r = args->response;

	if (!priv.wov_enabled)
		return EC_RES_ACCESS_DENIED;
	if (!priv.hotword_detected)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&priv.lock);
	r->offset = priv.audio_buf_rp;
	if (priv.audio_buf_rp <= priv.audio_buf_wp)
		r->len = priv.audio_buf_wp - priv.audio_buf_rp;
	else
		r->len = priv.driver.audio_buf_len - priv.audio_buf_rp;

	priv.audio_buf_rp += r->len;
	if (priv.audio_buf_rp == priv.driver.audio_buf_len)
		priv.audio_buf_rp = 0;
	mutex_unlock(&priv.lock);

	if (!r->len)
		WARN("underrun detected");

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
#endif

static int wov_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;

	DBG("WoV subcommand: %s", strcmd[p->cmd]);

	if (p->cmd < EC_CODEC_WOV_SUBCMD_COUNT)
		return sub_cmds[p->cmd](args);

	WARN("invalid WoV subcommand: %d", p->cmd);
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
int audio_codec_register_wov_driver(struct audio_codec_wov_driver *driver)
{
	if (!driver)
		return EC_SUCCESS;

	if (driver->enable)
		priv.driver.enable = driver->enable;
	if (driver->disable)
		priv.driver.disable = driver->disable;

	if (driver->read)
		priv.driver.read = driver->read;
	if (driver->enable_notifier)
		priv.driver.enable_notifier = driver->enable_notifier;
	if (driver->set_read_notifiee) {
		priv.driver.set_read_notifiee = driver->set_read_notifiee;
		driver->set_read_notifiee(wov_read_cb, NULL);
	}

	if (driver->audio_buf_addr) {
		priv.driver.audio_buf_addr = driver->audio_buf_addr;
		priv.driver.audio_buf_len = driver->audio_buf_len;
		priv.driver.audio_buf_type = driver->audio_buf_type;
	}
	if (driver->lang_buf_addr) {
		priv.driver.lang_buf_addr = driver->lang_buf_addr;
		priv.driver.lang_buf_len = driver->lang_buf_len;
		priv.driver.lang_buf_type = driver->lang_buf_type;
	}

	return EC_SUCCESS;
}

void audio_codec_wov_task(void *arg)
{
	uint32_t n, req;
	uint8_t *p = (uint8_t *)priv.driver.audio_buf_addr;

	if (!priv.driver.read) {
		ERR("read has not registered");
		return;
	}
	if (!priv.driver.enable_notifier) {
		ERR("enable_notifier has not registered");
		return;
	}
	if (!p) {
		ERR("audio buffer has not set");
		return;
	}

	while (1) {
		if (!priv.wov_enabled)
			goto next_round;

		mutex_lock(&priv.lock);
		if (priv.audio_buf_wp >= priv.audio_buf_rp)
			req = priv.driver.audio_buf_len - priv.audio_buf_wp;
		else
			req = priv.audio_buf_rp - priv.audio_buf_wp - 1;
		mutex_unlock(&priv.lock);

		if (!req) {
			/*
			 * `hotword_detected` should also be protected, but we
			 * can simply ignore the case (debug only).
			 */
			if (priv.hotword_detected)
				WARN("overrun detected");
			req = priv.driver.audio_buf_len - priv.audio_buf_wp;
		}

		/* read even if not enabled, in order to consume the buffer */
		n = priv.driver.read(p + priv.audio_buf_wp, req);
		if (n < 0) {
			ERR("failed to read: %d", n);
			break;
		} else if (n == 0) {
			if (priv.driver.enable_notifier() != EC_SUCCESS) {
				ERR("failed to enable_notifier");
				break;
			}

			DBG("no data, sleep");
			task_wait_event(-1);
			continue;
		}

		mutex_lock(&priv.lock);
		priv.audio_buf_wp += n;
		if (priv.audio_buf_wp == priv.driver.audio_buf_len)
			priv.audio_buf_wp = 0;
		mutex_unlock(&priv.lock);

next_round:
		usleep(1000);
	}
}
