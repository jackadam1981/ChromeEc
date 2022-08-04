/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ZEPHYR_CONSOLE_SHIM_H
#define __CROS_EC_ZEPHYR_CONSOLE_SHIM_H

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#define Z_CC_COMMAND command
#define Z_CC_ACCEL accel
#define Z_CC_AUDIO_CODEC audio_codec
#define Z_CC_BLUETOOTH_LE bluetooth_le
#define Z_CC_BLUETOOTH_LL bluetooth_ll
#define Z_CC_BLUETOOTH_HCI bluetooth_hci
#define Z_CC_CEC cec
#define Z_CC_CHARGER charger
#define Z_CC_CHIPSET chipset
#define Z_CC_CLOCK clock
#define Z_CC_DMA dma
#define Z_CC_EVENTS events
#define Z_CC_FP fingerprint
#define Z_CC_GESTURE gesture
#define Z_CC_GPIO gpio
#define Z_CC_HOSTCMD hostcmd
#define Z_CC_I2C i2c
#define Z_CC_IPI ipi
#define Z_CC_KEYBOARD keyboard
#define Z_CC_KEYSCAN keyscan
#define Z_CC_LIDANGLE lidangle
#define Z_CC_LOGOLED logoled
#define Z_CC_LIGHTBAR lightbar
#define Z_CC_RGBKBD rgbkbd
#define Z_CC_LPC lpc
#define Z_CC_MOTION_LID motionlid
#define Z_CC_MOTION_SENSE motionsense
#define Z_CC_PD_HOST_CMD pdhostcm
#define Z_CC_PORT80 port80
#define Z_CC_PS2 ps2
#define Z_CC_PCHG pchg
#define Z_CC_PWM pwm
#define Z_CC_SPI spi
#define Z_CC_SWITCH switch
#define Z_CC_SYSTEM system
#define Z_CC_TASK task
#define Z_CC_TOUCHPAD touchpad
#define Z_CC_DPTF dptf
#define Z_CC_ALS als
#define Z_CC_THERMAL thermal
#define Z_CC_USB usb
#define Z_CC_USBCHARGE usbcharge
#define Z_CC_USBPD usbpd
#define Z_CC_VBOOT vboot
#define Z_CC_HOOK hook
#define Z_CC_GPU gpu

struct zephyr_console_command {
	/* Handler for the command.  argv[0] will be the command name. */
	int (*handler)(int argc, char **argv);
#ifdef CONFIG_SHELL_HELP
	/* Description of args */
	const char *argdesc;
	/* Short help for command */
	const char *help;
#endif
};

#ifdef CONFIG_SHELL_HELP
#define _HELP_ARGS(A, H) .argdesc = A, .help = H,
#else
#define _HELP_ARGS(A, H)
#endif

/**
 * zshim_run_ec_console_command() - Dispatch a CrOS EC console command
 * using Zephyr's shell
 *
 * @command:		Pointer to a struct zephyr_console_command
 * @argc:		The number of command line arguments.
 * @argv:		The NULL-terminated list of arguments.
 *
 * Return: the return value from the handler.
 */
int zshim_run_ec_console_command(const struct zephyr_console_command *command,
				 size_t argc, char **argv);

/* Internal wrappers for DECLARE_CONSOLE_COMMAND_* macros. */
#define _ZEPHYR_SHELL_COMMAND_SHIM_2(NAME, ROUTINE_ID, ARGDESC, HELP,       \
				     WRAPPER_ID, ENTRY_ID)                  \
	static const struct zephyr_console_command ENTRY_ID = {             \
		.handler = ROUTINE_ID, _HELP_ARGS(ARGDESC, HELP)            \
	};                                                                  \
	static int WRAPPER_ID(const struct shell *shell, size_t argc,       \
			      char **argv)                                  \
	{                                                                   \
		return zshim_run_ec_console_command(&ENTRY_ID, argc, argv); \
	}                                                                   \
	SHELL_CMD_ARG_REGISTER(NAME, NULL, HELP, WRAPPER_ID, 0,             \
			       SHELL_OPT_ARG_MAX)

#define _ZEPHYR_SHELL_COMMAND_SHIM(NAME, ROUTINE_ID, ARGDESC, HELP)        \
	_ZEPHYR_SHELL_COMMAND_SHIM_2(NAME, ROUTINE_ID, ARGDESC, HELP,      \
				     UTIL_CAT(zshim_wrapper_, ROUTINE_ID), \
				     UTIL_CAT(zshim_entry_, ROUTINE_ID))

/* These macros mirror the macros provided by the CrOS EC. */
#define DECLARE_CONSOLE_COMMAND(NAME, ROUTINE, ARGDESC, HELP) \
	_ZEPHYR_SHELL_COMMAND_SHIM(NAME, ROUTINE, ARGDESC, HELP)

/*
 * TODO(jrosenth): implement flags and restricted commands?  We just
 * discard this in the shim layer for now.
 */
#define DECLARE_CONSOLE_COMMAND_FLAGS(NAME, ROUTINE, ARGDESC, HELP, FLAGS) \
	_ZEPHYR_SHELL_COMMAND_SHIM(NAME, ROUTINE, ARGDESC, HELP)
#define DECLARE_SAFE_CONSOLE_COMMAND(NAME, ROUTINE, ARGDESC, HELP) \
	_ZEPHYR_SHELL_COMMAND_SHIM(NAME, ROUTINE, ARGDESC, HELP)

/**
 * console_buf_notify_chars() - Notify the console host command buffer
 * of bytes on the console.
 *
 * @s:			The pointer to the string.
 * @len:		The size of the string.
 */
void console_buf_notify_chars(const char *s, size_t len);

#endif /* __CROS_EC_ZEPHYR_CONSOLE_SHIM_H */
