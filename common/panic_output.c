/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hooks.h"
#include "host_command.h"
#include "panic.h"
#include "printf.h"
#include "software_panic.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/* Panic data goes at the end of RAM. */
static struct panic_data * const pdata_ptr = PANIC_DATA_PTR;

/**
 * Add a character directly to the UART buffer.
 *
 * @param context	Context; ignored.
 * @param c		Character to write.
 * @return 0 if the character was transmitted, 1 if it was dropped.
 */
static int panic_txchar(void *context, int c)
{
	if (c == '\n')
		panic_txchar(context, '\r');

	/* Wait for space in transmit FIFO */
	while (!uart_tx_ready())
		;

	/* Write the character directly to the transmit FIFO */
	uart_write_char(c);

	return 0;
}

void panic_puts(const char *outstr)
{
	/* Flush the output buffer */
	uart_flush_output();

	/* Put all characters in the output buffer */
	while (*outstr)
		panic_txchar(NULL, *outstr++);

	/* Flush the transmit FIFO */
	uart_tx_flush();
}

void panic_printf(const char *format, ...)
{
	va_list args;

	/* Flush the output buffer */
	uart_flush_output();

	va_start(args, format);
	vfnprintf(panic_txchar, NULL, format, args);
	va_end(args);

	/* Flush the transmit FIFO */
	uart_tx_flush();
}

/**
 * Display a message and reboot
 */
void panic_reboot(void)
{
	panic_puts("\n\nRebooting...\n");
	system_reset(0);
}

#ifdef CONFIG_DEBUG_ASSERT_REBOOTS
#ifdef CONFIG_DEBUG_ASSERT_BRIEF
void panic_assert_fail(const char *fname, int linenum)
{
	panic_printf("\nASSERTION FAILURE at %s:%d\n", fname, linenum);
	__asm__("mov " EXP(SOFTWARE_PANIC_INFO_REG) ", %0\n"
		"ldr " EXP(SOFTWARE_PANIC_REASON_REG) ", ="
		       EXP(ASSERT_PANIC) "\n"
		"bl exception_panic\n"
		: : "r"(linenum));

	panic_reboot();
}
#else
void panic_assert_fail(const char *msg, const char *func, const char *fname,
		       int linenum)
{
	panic_printf("\nASSERTION FAILURE '%s' in %s() at %s:%d\n",
		     msg, func, fname, linenum);
	__asm__("mov " EXP(SOFTWARE_PANIC_INFO_REG) ", %0\n"
		"ldr " EXP(SOFTWARE_PANIC_REASON_REG) ", ="
		       EXP(ASSERT_PANIC) "\n"
		"bl exception_panic\n"
		: : "r"(linenum));

	panic_reboot();
}
#endif
#endif

void panic(const char *msg)
{
	panic_printf("\n** PANIC: %s\n", msg);
	panic_reboot();
}

struct panic_data *panic_get_data(void)
{
	return pdata_ptr->magic == PANIC_DATA_MAGIC ? pdata_ptr : NULL;
}

static void panic_init(void)
{
	uint32_t *lregs = pdata_ptr->cm.regs;

	if (!(system_get_reset_flags() & RESET_FLAG_WATCHDOG))
		return;

	/* Watchdog reset, log panic info */
	pdata_ptr->magic = PANIC_DATA_MAGIC;
	pdata_ptr->struct_size = sizeof(*pdata_ptr);
	pdata_ptr->struct_version = 2;
	pdata_ptr->arch = PANIC_ARCH_CORTEX_M;
	pdata_ptr->flags = 0;
	pdata_ptr->reserved = 0;

	lregs[3] = WATCHDOG_PANIC;
}
DECLARE_HOOK(HOOK_INIT, panic_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Console commands */

static int command_crash(int argc, char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM1;

	if (!strcasecmp(argv[1], "divzero")) {
		int a = 1, b = 0;

		cflush();
		ccprintf("%08x", a / b);
	} else if (!strcasecmp(argv[1], "unaligned")) {
		cflush();
		ccprintf("%08x", *(int *)0xcdef);
	} else if (!strcasecmp(argv[1], "assert")) {
		ASSERT(0);
	} else if (!strcasecmp(argv[1], "watchdog")) {
		while (1) ;
	} else {
		return EC_ERROR_PARAM1;
	}

	/* Everything crashes, so shouldn't get back here */
	return EC_ERROR_UNKNOWN;
}
DECLARE_CONSOLE_COMMAND(crash, command_crash,
			"[divzero | unaligned | assert | watchdog]",
			"Crash the system (for testing)",
			NULL);

static int command_panicinfo(int argc, char **argv)
{
	if (pdata_ptr->magic == PANIC_DATA_MAGIC) {
		ccprintf("Saved panic data:%s\n",
			 (pdata_ptr->flags & PANIC_DATA_FLAG_OLD_CONSOLE ?
			  "" : " (NEW)"));

		panic_data_print(pdata_ptr);

		/* Data has now been printed */
		pdata_ptr->flags |= PANIC_DATA_FLAG_OLD_CONSOLE;
	} else {
		ccprintf("No saved panic data available.\n");
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(panicinfo, command_panicinfo,
			NULL,
			"Print info from a previous panic",
			NULL);

/*****************************************************************************/
/* Host commands */

int host_command_panic_info(struct host_cmd_handler_args *args)
{
	if (pdata_ptr->magic == PANIC_DATA_MAGIC) {
		ASSERT(pdata_ptr->struct_size <= args->response_max);
		memcpy(args->response, pdata_ptr, pdata_ptr->struct_size);
		args->response_size = pdata_ptr->struct_size;

		/* Data has now been returned */
		pdata_ptr->flags |= PANIC_DATA_FLAG_OLD_HOSTCMD;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PANIC_INFO,
		     host_command_panic_info,
		     EC_VER_MASK(0));
