/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"

static struct zshim_host_command_node *host_command_head;

int zshim_setup_host_command(struct zshim_host_command_node *entry)
{
	struct zshim_host_command_node **loc = &host_command_head;

	/* Setup the entry */
	entry->next = *loc;

	/* Insert the entry */
	*loc = entry;

	return 0;
}

struct host_command *zephyr_find_host_command(int command)
{
	struct zshim_host_command_node *p;

	for (p = host_command_head; p != NULL; p = p->next) {
		if (p->cmd.command == command)
			return &p->cmd;
	}

	return NULL;
}
