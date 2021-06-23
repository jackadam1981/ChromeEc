/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if !defined(__CROS_EC_HOST_COMMAND_H) || \
	defined(__CROS_EC_ZEPHYR_HOST_COMMAND_H)
#error "This file must only be included from host_command.h. " \
	"Include host_command.h directly"
#endif
#define __CROS_EC_ZEPHYR_HOST_COMMAND_H

#include <init.h>

#ifdef CONFIG_PLATFORM_EC_HOSTCMD

/** Node in a list of host-command handlers */
struct zshim_host_command_node {
	struct host_command cmd;
	struct zshim_host_command_node *next;
};

/**
 * Runtime helper for DECLARE_HOST_COMMAND setup data.
 *
 * @param entry  The statically allocated host command node entry
 */
int zshim_setup_host_command(struct zshim_host_command_node *entry);

/**
 * See include/host_command.h for documentation.
 */
#define DECLARE_HOST_COMMAND(command, routine, version_mask) \
	_DECLARE_HOST_COMMAND_1(command, routine, version_mask, __LINE__)
#define _DECLARE_HOST_COMMAND_1(_command, _routine, _version_mask, line) \
	_DECLARE_HOST_COMMAND_2(_command, _routine, _version_mask, line)
#define _DECLARE_HOST_COMMAND_2(_command, _routine, _version_mask, line)      \
	static struct zshim_host_command_node _hc_lst_##line = { \
		.cmd = { \
			.command = _command, \
			.handler = _routine, \
			.version_mask = _version_mask, \
		}, \
	}; \
	SYS_INIT_ARG(zshim_setup_host_command, &_hc_lst_##line, APPLICATION, 1)
#else /* !CONFIG_PLATFORM_EC_HOSTCMD */
#define DECLARE_HOST_COMMAND(command, routine, version_mask)    \
	enum ec_status (routine)(struct host_cmd_handler_args *args)
#endif /* CONFIG_PLATFORM_EC_HOSTCMD */
