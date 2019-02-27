/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util.h"
#include "console.h"
#include "host_command.h"
#include "wov.h"
#include "task.h"

#ifdef DEBUG_WOV
#define CPRINTF(format, args...) cprintf(CC_WOV, format, ##args)
#else
#define CPRINTF(format, args...)
#endif /* DEBUG_WOV */

#define BIT(n) (1U << (n))

struct wov_ctx {
	struct wov_driver *wov_driver;

	struct mutex lock;

	uint32_t capabilities;
	uint8_t wov_enabled;
	uint8_t hotword_detected;

	uint32_t audio_buf_rp, audio_buf_wp;

	char lang_name[20];
} ctx;

static void wov_init(void)
{
	ctx.capabilities =
		0
#ifdef CONFIG_EC_CODEC_WOV_CAP_AUDIO_SHM
		| BIT(EC_CODEC_WOV_CAP_AUDIO_SHM)
#endif
#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
		| BIT(EC_CODEC_WOV_CAP_LANG_SHM)
#endif
		;
	ctx.wov_enabled = 0;
	ctx.hotword_detected = 0;

	ctx.audio_buf_rp = ctx.audio_buf_wp = 0;

	strncpy(ctx.lang_name, "none", ARRAY_SIZE(ctx.lang_name));
	ctx.lang_name[ARRAY_SIZE(ctx.lang_name) - 1] = 0;
}

static int capable(uint8_t cap)
{
	return ctx.capabilities & BIT(cap);
}

static int ec_codec_wov_get_capabilities(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_capabilities *r = args->response;

	r->capabilities = ctx.capabilities;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_get_shm_addr(struct host_cmd_handler_args *args)
{
	static const enum ec_codec_wov_cap shm_id_cap[] = {
		[EC_CODEC_WOV_SHM_ID_AUDIO] = EC_CODEC_WOV_CAP_AUDIO_SHM,
		[EC_CODEC_WOV_SHM_ID_LANG] = EC_CODEC_WOV_CAP_LANG_SHM,
	};

	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_get_shm_addr *pp =
		(struct ec_param_ec_codec_wov_get_shm_addr *)&p->set_lang_param;
	struct ec_response_ec_codec_wov_get_shm_addr *r = args->response;
	uint8_t shm_id = pp->shm_id;

	if (shm_id >= EC_CODEC_WOV_SHM_ID_LAST)
		return EC_RES_INVALID_PARAM;
	if (!capable(shm_id_cap[shm_id]))
		return EC_RES_INVALID_PARAM;

	switch (shm_id) {
	case EC_CODEC_WOV_SHM_ID_AUDIO:
		r->phys_addr = (uint64_t)ctx.wov_driver->audio_buf_addr;
		r->len = ctx.wov_driver->audio_buf_len;
		r->type = ctx.wov_driver->audio_buf_type;
		break;
	case EC_CODEC_WOV_SHM_ID_LANG:
		r->phys_addr = (uint64_t)ctx.wov_driver->lang_buf_addr;
		r->len = ctx.wov_driver->lang_buf_len;
		r->type = ctx.wov_driver->lang_buf_type;
		break;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_set_lang(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang *pp =
		(struct ec_param_ec_codec_wov_set_lang *)&p->set_lang_param;

	if (pp->total_len > ctx.wov_driver->lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (pp->offset >= ctx.wov_driver->lang_buf_len)
		return EC_RES_INVALID_PARAM;
	if (ctx.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	strncpy(ctx.lang_name, pp->name, ARRAY_SIZE(ctx.lang_name));
	ctx.lang_name[ARRAY_SIZE(ctx.lang_name) - 1] = 0;
	memcpy((uint8_t *)ctx.wov_driver->lang_buf_addr + pp->offset, pp->buf,
	       MIN(pp->len, ctx.wov_driver->lang_buf_len - pp->offset));

	CPRINTF("%s: %s\n", __func__, ctx.lang_name);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_set_lang_shm(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang_shm *pp =
		(struct ec_param_ec_codec_wov_set_lang_shm *)&p->set_lang_param;

	if (ctx.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	strncpy(ctx.lang_name, pp->name, ARRAY_SIZE(ctx.lang_name));
	ctx.lang_name[ARRAY_SIZE(ctx.lang_name) - 1] = 0;

	CPRINTF("%s: %s\n", __func__, ctx.lang_name);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_get_lang(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_lang *r = args->response;

	strncpy(r->name, ctx.lang_name, ARRAY_SIZE(r->name));
	r->name[ARRAY_SIZE(r->name) - 1] = 0;

	CPRINTF("%s: %s\n", __func__, r->name);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_enable(struct host_cmd_handler_args *args)
{
	if (ctx.wov_enabled)
		return EC_RES_ACCESS_DENIED;
	/* TODO: to have an allow list */
	if (strncmp(ctx.lang_name, "en_us", ARRAY_SIZE(ctx.lang_name)) != 0)
		return EC_RES_INVALID_PARAM;

	/* TODO: call GoogleHotwordDspInit */

	if (ctx.wov_driver->enable() != EC_SUCCESS)
		return EC_RES_ERROR;

	mutex_lock(&ctx.lock);
	ctx.wov_enabled = 1;
	ctx.hotword_detected = 0;
	ctx.audio_buf_rp = ctx.audio_buf_wp = 0;
	mutex_unlock(&ctx.lock);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_disable(struct host_cmd_handler_args *args)
{
	if (!ctx.wov_enabled)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&ctx.lock);
	ctx.wov_enabled = 0;
	ctx.hotword_detected = 0;
	ctx.audio_buf_rp = ctx.audio_buf_wp = 0;
	mutex_unlock(&ctx.lock);

	if (ctx.wov_driver->disable() != EC_SUCCESS)
		return EC_RES_ERROR;

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_wov_is_enabled(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_is_enabled *r = args->response;

	r->enabled = ctx.wov_enabled;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_is_hotword_detected(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_is_hotword_detected *r = args->response;

	if (!ctx.wov_enabled)
		return EC_ERROR_INVAL;

	r->detected = ctx.hotword_detected;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_reset(struct host_cmd_handler_args *args)
{
	/* TODO: call GoogleHotwordDspReset() */

	mutex_lock(&ctx.lock);
	ctx.hotword_detected = 0;
	mutex_unlock(&ctx.lock);

	args->response_size = 0;
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_read_audio(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio *r = args->response;

	if (!ctx.wov_enabled)
		return EC_RES_ACCESS_DENIED;
	if (!ctx.hotword_detected)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&ctx.lock);
	if (ctx.audio_buf_rp <= ctx.audio_buf_wp)
		r->len = MIN(ARRAY_SIZE(r->buf),
			     ctx.audio_buf_wp - ctx.audio_buf_rp);
	else
		r->len = MIN(ARRAY_SIZE(r->buf),
			     ctx.wov_driver->audio_buf_len - ctx.audio_buf_rp);
	mutex_unlock(&ctx.lock);

	memcpy(r->buf,
	       (uint8_t *)ctx.wov_driver->audio_buf_addr + ctx.audio_buf_rp,
	       r->len);

	mutex_lock(&ctx.lock);
	ctx.audio_buf_rp += r->len;
	if (ctx.audio_buf_rp == ctx.wov_driver->audio_buf_len)
		ctx.audio_buf_rp = 0;
	mutex_unlock(&ctx.lock);

	if (!r->len)
		CPRINTF("underrun detected\n");

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int ec_codec_wov_read_audio_shm(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio_shm *r = args->response;

	if (!ctx.wov_enabled)
		return EC_RES_ACCESS_DENIED;
	if (!ctx.hotword_detected)
		return EC_RES_ACCESS_DENIED;

	mutex_lock(&ctx.lock);
	r->offset = ctx.audio_buf_rp;
	if (ctx.audio_buf_rp <= ctx.audio_buf_wp)
		r->len = ctx.audio_buf_wp - ctx.audio_buf_rp;
	else
		r->len = ctx.wov_driver->audio_buf_len - ctx.audio_buf_rp;

	ctx.audio_buf_rp += r->len;
	if (ctx.audio_buf_rp == ctx.wov_driver->audio_buf_len)
		ctx.audio_buf_rp = 0;
	mutex_unlock(&ctx.lock);

	if (!r->len)
		CPRINTF("underrun detected\n");

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int (*sub_cmds[])(struct host_cmd_handler_args *) = {
	[EC_CODEC_WOV_GET_CAPABILITIES] = ec_codec_wov_get_capabilities,
	[EC_CODEC_WOV_GET_SHM_ADDR] = ec_codec_wov_get_shm_addr,
	[EC_CODEC_WOV_SET_LANG] = ec_codec_wov_set_lang,
	[EC_CODEC_WOV_SET_LANG_SHM] = ec_codec_wov_set_lang_shm,
	[EC_CODEC_WOV_GET_LANG] = ec_codec_wov_get_lang,
	[EC_CODEC_WOV_ENABLE] = ec_codec_wov_enable,
	[EC_CODEC_WOV_DISABLE] = ec_codec_wov_disable,
	[EC_CODEC_WOV_IS_ENABLED] = ec_codec_wov_wov_is_enabled,
	[EC_CODEC_WOV_IS_HOTWORD_DETECTED] = ec_codec_wov_is_hotword_detected,
	[EC_CODEC_WOV_RESET] = ec_codec_wov_reset,
	[EC_CODEC_WOV_READ_AUDIO] = ec_codec_wov_read_audio,
	[EC_CODEC_WOV_READ_AUDIO_SHM] = ec_codec_wov_read_audio_shm,
};

static int wov_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;

	if (!ctx.wov_driver)
		return EC_RES_ERROR;

	if (p->cmd < EC_CODEC_WOV_SUBCMD_COUNT)
		return sub_cmds[p->cmd](args);

	return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_WOV, wov_host_command, EC_VER_MASK(0));

static void wov_read_cb(void *priv_data)
{
	/* Note: in interrupt context */

	/* Note: task defined in ec.tasklist must use the name "WOV" */
	task_wake(TASK_ID_WOV);
}

/*
 * Exported interface
 */
int wov_register_driver(struct wov_driver *driver)
{
	if (ctx.wov_driver)
		return EC_ERROR_ACCESS_DENIED;

	ctx.wov_driver = driver;
	ctx.wov_driver->set_callback(wov_read_cb, NULL);

	return EC_SUCCESS;
}

void wov_task(void)
{
	uint32_t n, req;
	uint8_t *p;

	if (!ctx.wov_driver) {
		ccprintf("wov_driver has not registered\n");
		return;
	}

	wov_init();

	p = (uint8_t *)ctx.wov_driver->audio_buf_addr;

	while (1) {
		mutex_lock(&ctx.lock);
		if (ctx.audio_buf_wp >= ctx.audio_buf_rp)
			req = ctx.wov_driver->audio_buf_len - ctx.audio_buf_wp;
		else
			req = ctx.audio_buf_rp - ctx.audio_buf_wp - 1;
		mutex_unlock(&ctx.lock);

		if (!req) {
			/*
			 * `hotword_detected` should also be protected, but we
			 * can simply ignore the case (debug only).
			 */
			if (ctx.hotword_detected)
				CPRINTF("overrun detected\n");
			req = ctx.wov_driver->audio_buf_len - ctx.audio_buf_wp;
		}

		/* read even if not enabled, in order to consume the buffer */
		n = ctx.wov_driver->read(p + ctx.audio_buf_wp, req);
		if (n < 0) {
			ccprintf("failed to wov_driver->read: %d", n);
			break;
		} else if (n == 0) {
			/* TODO: will it miss latest wakeup? */
			task_wait_event(-1);
			continue;
		}

		mutex_lock(&ctx.lock);
		if (ctx.wov_enabled) {
			/* TODO: call GoogleHotwordDspProcess (incremental?) */
			/* host_set_single_event(EC_HOST_EVENT_WOV); if detected */

			ctx.audio_buf_wp += n;
			if (ctx.audio_buf_wp == ctx.wov_driver->audio_buf_len)
				ctx.audio_buf_wp = 0;
			mutex_unlock(&ctx.lock);
		}
		mutex_unlock(&ctx.lock);
	}
}
