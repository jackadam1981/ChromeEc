/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console_util.h"

#include <inttypes.h>

#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(console);

static struct param *find_argument(char *opt, struct command *cmd)
{
	for (int i = 0; i < cmd->params_cnt; i++) {
		if (strcmp(opt, cmd->params[i]->opt) == 0) {
			return cmd->params[i];
		}
	}
	return NULL;
}

static bool is_help(char *opt)
{
	if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
		return true;
	}
	return false;
}

int console_util_count(struct param** params, int cnt) {
	int total = 0;
	for (int i = 0; i < cnt; i++) {
		if (params[i]->found) {
			total += 1;
		}
	}
	return total;
}

void console_util_help(struct command *cmd)
{
	LOG_PRINTK("  %s\n", cmd->help_text);
	for (int i = 0; i < cmd->params_cnt; i++) {
		struct param *arg = cmd->params[i];
		char *type = "unknown";
		switch (arg->type) {
		case PARAM_TYPE_FLAG:
			type = "flag";
			break;
		case PARAM_TYPE_STR:
			type = "str";
			break;
		case PARAM_TYPE_INT:
			type = "int";
			break;
		}
		LOG_PRINTK("    %s %4s %s\n", arg->opt, type, arg->help_text);
	}
}

bool console_util_parse(size_t argc, char **argv, struct command *cmd)
{
	bool valid = true;
	for (int i = 1; i < argc; i++) {
		struct param *arg = find_argument(argv[i], cmd);
		if (arg == NULL) {
			valid = false;
			if (!is_help(argv[i])) {
				LOG_ERR("%s:Unknown flag", argv[i]);
			}
			break;
		}
		if (arg->type == PARAM_TYPE_FLAG) {
			arg->found = true;
			continue;
		}
		i++;
		if (argc <= i) {
			LOG_ERR("%s: Missing required value",
				arg->opt);
			valid = false;
			break;
		}
		char *value = argv[i];
		if (arg->type == PARAM_TYPE_STR) {
			arg->dst_str.dst = value;
			arg->found = true;
		} else if (arg->type == PARAM_TYPE_INT) {
			long result;
			long min = arg->dst_long.min;
			long max = arg->dst_long.max;
			if (!console_util_parse_long(value, &result)) {
				LOG_ERR("%s: Invalid integer %s",
					arg->opt, value);
				valid = false;
			} else if (!IN_RANGE(result, min, max)) {
				LOG_ERR("%s: Integer %ld range [%ld, %ld]",
					arg->opt, result, min, max);
				valid = false;
			} else {
				arg->dst_long.dst = result;
				arg->found = true;
			}
		}
		if (!valid) {
			break;
		}
	}
	if (!valid) {
		console_util_help(cmd);
	}
	return valid;
}

bool console_util_parse_long(const char *input, long *output)
{
	char *endptr = NULL;
	errno = 0;
	long result = strtol(input, &endptr, 0);
	/* Check for errno and ensure we reached the end of the string */
	if (errno || *endptr) {
		/* Integer parsing failed */
		return false;
	}
	*output = result;
	return true;
}
