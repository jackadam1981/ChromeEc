/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Drive a GPIB power supply */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "printf.h"
#include "registers.h"
#include "timer.h"
#include "util.h"

/* Use USART1 */
#define UARTG 1
/* Max supported baud rate is 9600 bds */
#define GPIB_BAUD_RATE 9600

static int gpib_txchar(void *context, int c)
{
	/* Wait for space to transmit */
	while (!(STM32_USART_SR(UARTG) & STM32_USART_SR_TXE))
		;
	STM32_USART_TDR(UARTG) = c;

	return 0;
}

static void gpib_printf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfnprintf(gpib_txchar, NULL, format, args);
	va_end(args);
}

static int gpib_read_output(char *buffer, int cnt)
{
	int i = 0;
	timestamp_t deadline;

	deadline.val = get_time().val + 10000;
	while ((i < cnt - 1) && (get_time().val < deadline.val)) {
		while(!(STM32_USART_SR(UARTG) & STM32_USART_SR_RXNE) &&
			(get_time().val < deadline.val))
			;
		if (get_time().val >= deadline.val)
			break;
		buffer[i++] = STM32_USART_RDR(UARTG);
	}
	buffer[i] = '\0';

	return i;
}

void gpib_set_voltage(int mv)
{
	int volt = mv/1000;
	int remain = mv - volt*1000;
	gpib_printf(":chan2:volt %d.%03d\n", volt, remain);
}

void gpib_set_current(int ma)
{
	int amp = ma/1000;
	int remain = ma - amp*1000;

	if (amp > 2) {
		/* Parallel coupling */
		gpib_printf(":outp:couple:tracking 2\n");
		gpib_printf(":chan1:curr 2.0\n");
		amp -= 2;
	} else {
		gpib_printf(":outp:couple:tracking 0\n");
	}

	gpib_printf(":chan2:curr %d.%03d\n", amp, remain);
}

void gpib_set_output(int en)
{
	gpib_printf(":outp:state %d\n", !!en);
}

static void gpib_init(void)
{
	/* Enable USART1 clock */
	STM32_RCC_APB2ENR |= 1 << 14;

	/* Pin muxing already done by the main UART driver */

	/* set baudrate */
	STM32_USART_BRR(UARTG) =
		DIV_ROUND_NEAREST(CPU_CLOCK, GPIB_BAUD_RATE);
	/* UART enabled, 8 Data bits, oversampling x16, no parity */
	STM32_USART_CR1(UARTG) =
		STM32_USART_CR1_UE | STM32_USART_CR1_TE | STM32_USART_CR1_RE;
	/* 1 stop bit, no fancy stuff */
	STM32_USART_CR2(UARTG) = 0x0000;
	/* DMA disabled, special modes disabled, error interrupt disabled */
	STM32_USART_CR3(UARTG) = 0x0000;

	/* initialize power supply */
	gpib_set_output(0);
	gpib_printf(":outp:couple:tracking 0\n");
	gpib_printf(":chan1:volt 0.0\n");
	gpib_printf(":chan3:volt 0.0\n");
	gpib_set_voltage(5000);
	gpib_set_current(100);
	gpib_set_output(1);
}
DECLARE_HOOK(HOOK_INIT, gpib_init, HOOK_PRIO_DEFAULT);

static int command_gpib(int argc, char **argv)
{
	static char buf[128];
	if (argc < 2) {
		/* dump version */
		gpib_printf("*idn?\n");
	} else {
		gpib_printf("%s\n",argv[1]);
	}
	gpib_read_output(buf, sizeof(buf));
	ccprintf("|%s\n", buf);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(gpib, command_gpib,
			"[command]",
			"send GPIB command",
			NULL);
