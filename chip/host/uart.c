/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART driver for emulator */

#include <pthread.h>
#include <stdio.h>
#include <termio.h>
#include <unistd.h>

#include "board.h"
#include "config.h"
#include "task.h"
#include "uart.h"

static int stopped;
static int int_disabled;
static int init_done;

static pthread_t input_thread;

static int char_available;
static char cached_char;

static void trigger_interrupt(void)
{
	/*
	 * TODO: Check global interrupt status when we have
	 * interrupt support.
	 */
	if (!int_disabled)
		uart_process();
}

int uart_init_done(void)
{
	return init_done;
}

void uart_tx_start(void)
{
	stopped = 0;
	trigger_interrupt();
}

void uart_tx_stop(void)
{
	stopped = 1;
}

int uart_tx_stopped(void)
{
	return stopped;
}

void uart_tx_flush(void)
{
	/* Nothing */
}

int uart_tx_ready(void)
{
	return 1;
}

int uart_rx_available(void)
{
	return char_available;
}

void uart_write_char(char c)
{
	printf("%c", c);
	fflush(stdout);
}

int uart_read_char(void)
{
	char ret = cached_char;
	char_available = 0;
	return ret;
}

void uart_disable_interrupt(void)
{
	int_disabled = 1;
}

void uart_enable_interrupt(void)
{
	int_disabled = 0;
}

void *uart_monitor_stdin(void *d)
{
	struct termios org_settings, new_settings;

	tcgetattr(0, &org_settings);
	new_settings = org_settings;
	new_settings.c_lflag &= ~(ECHO | ICANON);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;

	printf("Console input initialized\n");
	while (1) {
		tcsetattr(0, TCSANOW, &new_settings);
		read(0, &cached_char, 1);
		tcsetattr(0, TCSANOW, &org_settings);
		char_available = 1;
		/*
		 * TODO: Trigger emulated interrupt when we have
		 * interrupt support. Also, we will need a condition
		 * variable to indicate the character has been read.
		 */
		trigger_interrupt();
	}

	return 0;
}

void uart_init(void)
{
	pthread_create(&input_thread, NULL, uart_monitor_stdin, NULL);
	init_done = 1;
}
