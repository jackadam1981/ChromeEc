/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util.h"
#include "console.h"
#include "host_command.h"

#define DEBUG_WOV

#ifdef DEBUG_WOV
#define CPRINTF(format, args...) cprintf(CC_WOV, format, ##args)
#else
#define CPRINTF(format, args...)
#endif /* DEBUG_WOV */

struct wov_ctx {
	uint8_t is_enabled;
	uint8_t is_hotword_detected;
} ctx;

static int get_capabilities(uint32_t *capabilities)
{
	*capabilities = 0
#ifdef CONFIG_EC_CODEC_WOV_CAP_AUDIO_SHM
		| EC_CODEC_WOV_CAP_AUDIO_SHM
#endif
#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
		| EC_CODEC_WOV_CAP_LANG_SHM
#endif
		;

	return EC_RES_SUCCESS;
}

static int enable(void)
{
	if (ctx.is_enabled)
		return EC_ERROR_INVAL;

	/*
	 * return EC_ERROR_INVALID_CONFIG if any prerequisite configuration
	 * is not provided.
	 */

	ctx.is_enabled = 1;
	return EC_RES_SUCCESS;
}

static int disable(void)
{
	if (!ctx.is_enabled)
		return EC_ERROR_INVAL;

	ctx.is_enabled = 0;
	return EC_RES_SUCCESS;
}

static int is_enabled(uint8_t *is_enabled)
{
	*is_enabled = ctx.is_enabled;
	return EC_RES_SUCCESS;
}

static int is_hotword_detected(uint8_t *is_hotword_detected)
{
	if (!ctx.is_enabled)
		return EC_ERROR_INVAL;

	*is_hotword_detected = ctx.is_hotword_detected;
	return EC_RES_SUCCESS;
}

static int read(uint8_t *buf, uint32_t len, uint32_t *n)
{
	const char *dummy_data = "~~DeadBeef~~";
	const uint32_t dummy_data_len = 12;

	if (!ctx.is_enabled)
		return EC_ERROR_INVAL;

	if (!ctx.is_hotword_detected)
		return EC_ERROR_INVAL;

	*n = MIN(len, dummy_data_len);
	strncpy(buf, dummy_data, *n);

	/* TODO: move read head */

	return EC_RES_SUCCESS;
}

static int ec_codec_wov_get_capabilities(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_get_capabilities *r = args->response;

	args->response_size = sizeof(*r);
	return get_capabilities(&r->capabilities);
}

static int ec_codec_wov_enable(struct host_cmd_handler_args *args)
{
	args->response_size = 0;
	return enable();
}

static int ec_codec_wov_disable(struct host_cmd_handler_args *args)
{
	args->response_size = 0;
	return disable();
}

static int ec_codec_wov_is_enabled(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_is_enabled *r = args->response;

	args->response_size = sizeof(*r);
	return is_enabled(&r->is_enabled);
}

static int ec_codec_wov_is_hotword_detected(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_wov_is_hotword_detected *r = args->response;

	args->response_size = sizeof(*r);
	return is_hotword_detected(&r->is_hotword_detected);
}

static int ec_codec_wov_read(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *param =
		(struct ec_param_ec_codec_wov *)args->params;
	struct ec_response_ec_codec_wov_read *r = args->response;

	args->response_size = sizeof(*r);
	return read(r->buf, param->read_param.len, &r->len);
}

static int wov_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_wov *param =
		(struct ec_param_ec_codec_wov *)args->params;

	int (*cmds[])(struct host_cmd_handler_args *) = {
		ec_codec_wov_get_capabilities,
		ec_codec_wov_enable,
		ec_codec_wov_disable,
		ec_codec_wov_is_enabled,
		ec_codec_wov_is_hotword_detected,
		ec_codec_wov_read,
	};

	if (param->cmd < EC_CODEC_WOV_SUBCMD_COUNT)
		return cmds[param->cmd](args);
	else
		return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_WOV, wov_host_command, EC_VER_MASK(0));

int handle_get_capabilities(int argc, char **argv)
{
	int ret = EC_ERROR_INVAL;
	uint32_t cap;

	if (argc != 2)
		goto leave;

	ret = get_capabilities(&cap);
	if (ret)
		goto leave;

	ccprintf("Capabilities:\n");
	if (cap & EC_CODEC_WOV_CAP_AUDIO_SHM)
		ccprintf("  EC_CODEC_WOV_CAP_AUDIO_SHM\n");
	if (cap & EC_CODEC_WOV_CAP_LANG_SHM)
		ccprintf("  EC_CODEC_WOV_CAP_LANG_SHM\n");
leave:
	return ret;
}

int handle_enable(int argc, char **argv)
{
	int ret;

	if (argc != 2)
		return EC_ERROR_INVAL;

	ret = enable();
	if (ret == EC_ERROR_INVAL)
		ccprintf("Already enabled!\n");
	else if (ret == EC_ERROR_INVALID_CONFIG)
		ccprintf("Period bytes has not set!\n");
	else
		ccprintf("Success!\n");
	return EC_SUCCESS;
}

int handle_disable(int argc, char **argv)
{
	if (argc != 2)
		return EC_ERROR_INVAL;

	if (disable() == EC_ERROR_INVAL)
		ccprintf("Not enabled yet!\n");
	else
		ccprintf("Success!\n");
	return EC_SUCCESS;
}

int handle_is_enabled(int argc, char **argv)
{
	int ret = EC_ERROR_INVAL;
	uint8_t enabled;

	if (argc != 2)
		goto leave;

	ret = is_enabled(&enabled);
	if (ret)
		goto leave;

	ccprintf("%d\n", enabled);
leave:
	return ret;
}

int handle_is_hotword_detected(int argc, char **argv)
{
	int ret = EC_ERROR_INVAL;
	uint8_t detected;

	if (argc != 2)
		goto leave;

	ret = is_hotword_detected(&detected);
	if (ret) {
		ccprintf("Not enabled yet!\n");
		ret = EC_SUCCESS;
		goto leave;
	}

	ccprintf("%d\n", detected);
leave:
	return ret;
}

#ifdef DEBUG_WOV
int handle_set_hotword_detected(int argc, char **argv)
{
	int ret = EC_ERROR_INVAL;
	int detected;

	if (argc != 3)
		goto leave;

	detected = !!atoi(argv[2]);
	ctx.is_hotword_detected = detected;
	ret = EC_SUCCESS;
	ccprintf("Success!\n");
leave:
	return ret;
}
#endif

int handle_read(int argc, char **argv)
{
	int ret = EC_ERROR_INVAL;
	uint8_t buf[128];
	uint32_t n;

	if (argc != 2)
		goto leave;

	ret = read(buf, sizeof(buf), &n);
	if (ret) {
		ccprintf("Either not enabled or hotword not detected\n");
		ret = EC_SUCCESS;
		goto leave;
	}

	buf[n] = 0;
	ccprintf("[%s]\n", buf);
leave:
	return ret;
}

static int wov_console_command(int argc, char **argv)
{
	struct {
		char *cmd;
		int (*exec)(int, char **);
	} cmds[] = {
		{"get_capabilities",	handle_get_capabilities},
		{"enable",		handle_enable},
		{"disable",		handle_disable},
		{"is_enabled",		handle_is_enabled},
		{"is_hotword_detected",	handle_is_hotword_detected},
#ifdef DEBUG_WOV
		{"set_hotword_detected",handle_set_hotword_detected},
#endif
		{"read",		handle_read},
		{NULL, NULL},
	};

	if (argc >= 2) {
		size_t i;

		for (i = 0; cmds[i].cmd; ++i)
			if (strcasecmp(cmds[i].cmd, argv[1]) == 0)
				return cmds[i].exec(argc, argv);
	}

	return EC_ERROR_INVAL;
}

DECLARE_CONSOLE_COMMAND(wov, wov_console_command,
		"[command]\n"
		"\n"
		"command:\n"
		"  get_capabilities\n"
		"  enable\n"
		"  disable\n"
		"  is_enabled\n"
		"  is_hotword_detected\n"
		"  read\n",
		"wov command");
