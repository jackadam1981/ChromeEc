
#include "host_command.h"

static struct zephyr_shim_host_command_list *host_command_registry;

void zephyr_shim_setup_host_command(
	int command,
	enum ec_status (*routine)(struct host_cmd_handler_args *args),
	int version_mask, struct zephyr_shim_host_command_list *entry)
{
	struct zephyr_shim_host_command_list **loc = &host_command_registry;

	/* Setup the entry. */
	entry->cmd->handler = routine;
	entry->cmd->command = command;
	entry->cmd->version_mask = version_mask;
	entry->next = *loc;

	/* Insert the entry. */
	*loc = entry;
}

struct host_command * zephyr_find_host_command(int command)
{
	struct zephyr_shim_host_command_list *p;
	for (p = host_command_registry; p != NULL; p = p->next) {
		if (p->cmd->command == command) {
			return p->cmd;
		}
	}

	return NULL;
}
