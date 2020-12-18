#if !defined(__CROS_EC_HOST_COMMAND_H) || \
	defined(__CROS_EC_ZEPHYR_HOST_COMMANDS_SHIM_H)
#error "This file must only be included from host_commands.h. Include host_commands.h directly."
#endif
#define __CROS_EC_ZEPHYR_HOST_COMMANDS_SHIM_H

struct host_command * zephyr_find_host_command(int command);

struct zephyr_shim_host_command_list {
	struct host_command *cmd;
	struct zephyr_shim_host_command_list *next;
};

/**
 * Runtime helper for DECLARE_HOST_COMMAND setup data.
 *
 * @param routine	The handler for the host command.
 * @param command
 * @param version_mask
 */
void zephyr_shim_setup_host_command(
	int command,
	enum ec_status (*routine)(struct host_cmd_handler_args *args),
	int version_mask, struct zephyr_shim_host_command_list *entry);

/**
 * See include/host_command.h for documentation.
 */
#define DECLARE_HOST_COMMAND(command, routine, version_mask) \
	_DECLARE_HOST_COMMAND_1(command, routine, version_mask, __LINE__)
#define _DECLARE_HOST_COMMAND_1(command, routine, version_mask, line) \
	_DECLARE_HOST_COMMAND_2(command, routine, version_mask, line)
#define _DECLARE_HOST_COMMAND_2(command, routine, version_mask, line)          \
	static int _setup_host_command_##line(const struct device *unused)     \
	{                                                                      \
		ARG_UNUSED(unused);                                            \
		static struct host_command cmd;                            \
		static struct zephyr_shim_host_command_list lst;               \
		lst.cmd = &cmd;                                        \
		zephyr_shim_setup_host_command(command, routine, version_mask, \
					       &lst);                          \
		return 0;                                                      \
	}                                                                      \
	SYS_INIT(_setup_host_command_##line, APPLICATION, 1)
