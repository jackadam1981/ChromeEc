/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "cpu.h"
#include "panic.h"
#include "printf.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

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

void report_panic(uint32_t *regs, uint32_t itype)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
	panic_printf("=== EXCEP: ITYPE=%x ===\n", itype);
	panic_printf("R0  %08x R1  %08x R2  %08x R3  %08x\n",
		     regs[0], regs[1], regs[2], regs[3]);
	panic_printf("R4  %08x R5  %08x R6  %08x R7  %08x\n",
		     regs[4], regs[5], regs[6], regs[7]);
	panic_printf("R8  %08x R9  %08x R10 %08x R15 %08x\n",
		     regs[8], regs[9], regs[10], regs[11]);
	panic_printf("FP  %08x GP  %08x LP  %08x SP  %08x\n",
		     regs[12], regs[13], regs[14], regs[15]);
	panic_printf("IPC %08x IPSW   %05x\n", regs[16], regs[17]);
	if (regs[17] & 8) { /* 2nd level exception */
		uint32_t oipc;

		asm volatile("mfsr %0, $OIPC":"=r"(oipc));
		panic_printf("OIPC %08x\n", oipc);
	}

	panic_reboot();
}

#ifdef CONFIG_DEBUG_ASSERT_REBOOTS
void panic_assert_fail(const char *msg, const char *func, const char *fname,
		       int linenum)
{
	panic_printf("\nASSERTION FAILURE '%s' in %s() at %s:%d\n",
		     msg, func, fname, linenum);

	panic_reboot();
}
#endif

void panic(const char *msg)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

struct panic_data *panic_get_data(void)
{
	return NULL;
}

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
	} else {
		return EC_ERROR_PARAM1;
	}

	/* Everything crashes, so shouldn't get back here */
	return EC_ERROR_UNKNOWN;
}
DECLARE_CONSOLE_COMMAND(crash, command_crash,
			"[divzero | unaligned]",
			"Crash the system (for testing)",
			NULL);
