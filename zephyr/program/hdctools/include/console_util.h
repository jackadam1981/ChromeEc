/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_CONSOLE_UTIL_H__
#define __CROS_EC_STARFISH_CONSOLE_UTIL_H__

#include <stdbool.h>
#include <stdlib.h>

/* Identifies how to process the parameter. */
enum param_type {
	PARAM_TYPE_FLAG,
	PARAM_TYPE_STR,
	PARAM_TYPE_INT,
};

/* Storage container for the string type parameters. */
struct param_str {
	char *dst;
};

/* Storage container for the integer type parameters. */
struct param_long {
	long dst;
	long min;
	long max;
};

/* Stores an parameter which acts as a union type */
struct param {
	const char *opt;
	const char *help_text;
	enum param_type type;
	bool found;
	union {
		struct param_str dst_str;
		struct param_long dst_long;
	};
};

/*
 * Defines a command which can include 0 or more arguments.
 *
 * CMD_PARSER() is a helper macro which converts an array of arguments
 *    into a completed command.
 * CMD_HELP() as a helper macro allows for the creation of 0 length arguments
 *    which allow the same parsing logic to handle simple help messages.
 *
 *
 * console_util_parse() is used with this to parse command arguments and
 * validate the result.
 * console_util_help() produces a formatted help message.
 */
struct command {
	/* Help text for the describing the command. */
	const char *help_text;
	/* Number of parameters in the command. */
	int params_cnt;
	/* Pointer to parameter array. */
	struct param **params;
};

#define COUNT_FIELDS(params_array)  \
	return console_util_count(params_array, ARRAY_SIZE(params_array))

/**
 * Counts the total number of params found
 * @param  params [description]
 * @param  cnt    [description]
 * @return        [description]
 */
int console_util_count(struct param** params, int cnt);

/*
 * Defines a new command with parameters.
 *
 * @param  help       Help String to describe the command
 * @param  params_array Array of arguments provided to the command.
 * @return            Returns an initialized struct command
 */
#define CMD_PARSER(help_text_, params_array)            \
	{                                               \
		.help_text = help_text_,                \
		.params_cnt = ARRAY_SIZE(params_array), \
		.params = params_array,                 \
	}

/*
 * Defines a empty command with no parameters.
 *
 * @param  help Help String to describe the command
 * @return      Returns an initialized struct command
 */
#define CMD_HELP(help_text_)                                              \
	{                                                                 \
		.help_text = help_text_, .params_cnt = 0, .params = NULL, \
	}

/*
 * Helper to define a new flag parameter which stores true.
 *
 * @param  opt  Option name
 * @param  help Help String to describe the command
 * @return      Returns an initialized struct param
 */
#define PARAM_PARSER_FLAG(opt_, help_text_)                                    \
	{                                                                      \
		.opt = opt_, .help_text = help_text_, .type = PARAM_TYPE_FLAG, \
		.found = false,                                                \
	}

/*
 * Helper to define a new flag parameter which stores a string of text.
 *
 * @param  opt  Option name
 * @param  help Help String to describe the command
 * @return      Returns an initialized struct param
 */
#define PARAM_PARSER_STR(opt_, help_text_)                                    \
	{                                                                     \
		.opt = opt_, .help_text = help_text_, .type = PARAM_TYPE_STR, \
		.found = false,                                               \
	}

/*
 * Helper to define a new flag parameter which stores an integer and validates
 * the range.
 *
 * @param  opt       Option name
 * @param  help_text Help String to describe the command
 * @param  min       Min valid integer value
 * @param  max       Max valid integer value
 * @return           Returns an initialized struct param
 */
#define PARAM_PARSER_LONG(opt_, help_text_, min_, max_)                       \
	{                                                                     \
		.opt = opt_, .help_text = help_text_, .type = PARAM_TYPE_INT, \
		.found = false, .dst_long = { .min = min_, .max = max_ },     \
	}

/*
 * Prints a formatted help message for the command to the console.
 *
 * @param cmd Command structure we are printing.
 */
void console_util_help(struct command *cmd);

/*
 * Parse command line arguments and save the results
 *
 *
 *
 * @param  argc Number of arguments provided
 * @param  argv Argument array
 * @param   cmd Command structure we are printing.
 * @return      True if parsing had no errors
 */
bool console_util_parse(size_t argc, char **argv, struct command *cmd);

/*
 * Parses a string to integer value and checks errors.
 *
 * @param  input  Input string
 * @param  output Output numeric value
 * @return        True if parsing detected no errors
 */
bool console_util_parse_long(const char *input, long *output);

#endif /* __CROS_EC_STARFISH_CONSOLE_UTIL_H__ */
