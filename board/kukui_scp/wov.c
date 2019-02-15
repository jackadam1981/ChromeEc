/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util.h"
#include "console.h"
#include "host_command.h"
#include "hooks.h"
#include "link_defs.h"
#include "memmap.h"

#define DEBUG_WOV

#ifdef DEBUG_WOV
#define CPRINTF(format, args...) cprintf(CC_WOV, format, ##args)
#else
#define CPRINTF(format, args...)
#endif /* DEBUG_WOV */

#define BIT(n) (1U << (n))

struct wov_ctx {
	uint32_t capabilities;
	uint8_t wov_enabled;
	uint8_t hotword_detected;

#ifdef CONFIG_EC_CODEC_WOV_CAP_AUDIO_SHM
	uintptr_t audio_shm_addr;
	uint32_t audio_shm_len;
#endif

	char lang_name[20];
#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
	uintptr_t lang_shm_addr;
	uint32_t lang_shm_len;
#endif

	uint8_t dummy_audio_buf[64];
} ctx;

#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
__SECTION(dram)
#endif
uint8_t lang_buf[65535];

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

	strncpy(ctx.lang_name, "N/A", ARRAY_SIZE(ctx.lang_name));
	ctx.lang_name[ARRAY_SIZE(ctx.lang_name) - 1] = 0;

#ifdef CONFIG_EC_CODEC_WOV_CAP_AUDIO_SHM
	ctx.audio_shm_addr = CONFIG_AUDIO_SHM_BASE;
	ctx.audio_shm_len = CONFIG_AUDIO_SHM_SIZE;
#endif
#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
	ctx.lang_shm_addr = (uintptr_t)lang_buf;
	ctx.lang_shm_len = ARRAY_SIZE(lang_buf);
#endif

	strncpy(ctx.dummy_audio_buf, "~~DeadBeef~~",
		ARRAY_SIZE(ctx.dummy_audio_buf));
	ctx.dummy_audio_buf[ARRAY_SIZE(ctx.dummy_audio_buf) - 1] = 0;
}
DECLARE_HOOK(HOOK_INIT, wov_init, HOOK_PRIO_DEFAULT);

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

static enum ec_codec_wov_cap shm_id_cap[] = {
	[EC_CODEC_WOV_SHM_ID_AUDIO] = EC_CODEC_WOV_CAP_AUDIO_SHM,
	[EC_CODEC_WOV_SHM_ID_LANG] = EC_CODEC_WOV_CAP_LANG_SHM,
};

static int ec_codec_wov_get_shm_addr(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_get_shm_addr *pp =
		(struct ec_param_ec_codec_wov_get_shm_addr *)&p->set_lang_param;
	struct ec_response_ec_codec_wov_get_shm_addr *r = args->response;
	uint8_t shm_id = pp->shm_id;

	if (shm_id >= EC_CODEC_WOV_SHM_ID_LAST)
		return EC_ERROR_INVAL;
	if (!capable(shm_id_cap[shm_id]))
		return EC_ERROR_INVAL;

	switch (shm_id) {
	case EC_CODEC_WOV_SHM_ID_AUDIO:
		r->phys_addr = (uint64_t)ctx.audio_shm_addr;
		r->len = ctx.audio_shm_len;
		r->type = EC_CODEC_WOV_SHM_TYPE_EC_RAM;
		break;
	case EC_CODEC_WOV_SHM_ID_LANG: {
		uintptr_t ap_addr;

		memmap_scp_cache_to_ap(ctx.lang_shm_addr, &ap_addr);
		r->phys_addr = (uint64_t)ap_addr;
		r->len = ctx.lang_shm_len;
		r->type = EC_CODEC_WOV_SHM_TYPE_SYSTEM_RAM;
		break;
	}
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

	if (pp->total_len > ARRAY_SIZE(lang_buf))
		return EC_ERROR_INVAL;
	if (pp->offset >= ARRAY_SIZE(lang_buf))
		return EC_ERROR_INVAL;
	if (ctx.wov_enabled)
		return EC_ERROR_INVAL;

	strncpy(ctx.lang_name, pp->name, ARRAY_SIZE(ctx.lang_name));
	ctx.lang_name[ARRAY_SIZE(ctx.lang_name) - 1] = 0;
	memcpy(lang_buf + pp->offset, pp->buf,
	       MIN(pp->len, ARRAY_SIZE(lang_buf) - pp->offset));
	args->response_size = 0;

	CPRINTF("TB: %s: %s\n", __func__, ctx.lang_name);
	lang_buf[pp->len] = 0;
	CPRINTF("TB: %s: %s\n", __func__, lang_buf);

	return EC_RES_SUCCESS;
}

static int ec_codec_wov_set_lang_shm(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_param_ec_codec_wov_set_lang_shm *pp =
		(struct ec_param_ec_codec_wov_set_lang_shm *)&p->set_lang_param;

	if (ctx.wov_enabled)
		return EC_ERROR_INVAL;

	strncpy(ctx.lang_name, pp->name, ARRAY_SIZE(ctx.lang_name));
	ctx.lang_name[ARRAY_SIZE(ctx.lang_name) - 1] = 0;
	args->response_size = 0;

	CPRINTF("TB: %s: %s\n", __func__, ctx.lang_name);
	CPRINTF("TB: %s: %s\n", __func__, lang_buf);

	return EC_RES_SUCCESS;
}

static int ec_codec_wov_get_lang(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_lang *r = args->response;

	strncpy(r->name, ctx.lang_name, ARRAY_SIZE(r->name));
	r->name[ARRAY_SIZE(r->name) - 1] = 0;
	args->response_size = sizeof(*r);

	CPRINTF("%s: name=%s\n", __func__, r->name);

	return EC_RES_SUCCESS;
}

static int ec_codec_wov_enable(struct host_cmd_handler_args *args)
{
	if (ctx.wov_enabled)
		return EC_ERROR_INVAL;
	if (strncmp(ctx.lang_name, "en_us", ARRAY_SIZE(ctx.lang_name)) != 0)
		return EC_ERROR_INVALID_CONFIG;

	ctx.wov_enabled = 1;
	args->response_size = 0;

	return EC_RES_SUCCESS;
}

static int ec_codec_wov_disable(struct host_cmd_handler_args *args)
{
	if (!ctx.wov_enabled)
		return EC_ERROR_INVAL;

	ctx.wov_enabled = 0;
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

static int ec_codec_wov_read_audio(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio *r = args->response;

#ifndef DEBUG_WOV
	if (!ctx.wov_enabled)
		return EC_ERROR_INVAL;
	if (!ctx.hotword_detected)
		return EC_ERROR_INVAL;
#endif

	r->len = MIN(ARRAY_SIZE(r->buf), ARRAY_SIZE(ctx.dummy_audio_buf));
	r->len = strnlen(ctx.dummy_audio_buf, 100);
	memcpy(r->buf, ctx.dummy_audio_buf, r->len);
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}

static int ec_codec_wov_read_audio_shm(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_read_audio_shm *r = args->response;
	char *p = (char *)ctx.audio_shm_addr;

#ifndef DEBUG_WOV
	if (!ctx.wov_enabled)
		return EC_ERROR_INVAL;
	if (!ctx.hotword_detected)
		return EC_ERROR_INVAL;
#endif

	p[0] = 'H';
	p[1] = 'e';
	p[2] = 'l';
	p[3] = 'l';
	p[4] = 'o';
	p[5] = 0;
	r->offset = 0;
	r->len = 5;
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
	[EC_CODEC_WOV_READ_AUDIO] = ec_codec_wov_read_audio,
	[EC_CODEC_WOV_READ_AUDIO_SHM] = ec_codec_wov_read_audio_shm,
};

static int wov_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *p =
		(struct ec_param_ec_codec_wov *)args->params;

	if (p->cmd < EC_CODEC_WOV_SUBCMD_COUNT)
		return sub_cmds[p->cmd](args);
	else
		return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_WOV, wov_host_command, EC_VER_MASK(0));
