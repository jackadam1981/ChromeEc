/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Keyboard scanner module for Chrome EC
 *
 * TODO: Finish cleaning up nomenclature (cols/rows/inputs/outputs),
 */

#include "board.h"
#include "console.h"
#include "gpio.h"
#include "keyboard.h"
#include "keyboard_scan.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_KEYSCAN, outstr)
#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ## args)

/* used for select_column() */
enum COL_INDEX {
	COL_ASSERT_ALL = -2,
	COL_TRI_STATE_ALL = -1,
	/* 0 ~ 12 for the corresponding column */
};

#define POLLING_MODE_TIMEOUT 100000   /* 100 ms */
#define SCAN_LOOP_DELAY 10000         /*  10 ms */

#define KB_COLS 13

/* 15:14, 12:8, 2 */
#define IRQ_MASK 0xdf04

/* The keyboard state from the last read */
static uint8_t raw_state[KB_COLS];

/* The keyboard state we will return when requested */
//static uint8_t saved_state[KB_COLS];

/* Mask with 1 bits only for keys that actually exist */
static const uint8_t *actual_key_mask;

/* All actual key masks (todo: move to keyboard matrix definition) */
/* TODO: (crosbug.com/p/7485) fill in real key mask with 0-bits for coords that
   aren't keys */
static const uint8_t actual_key_masks[4][KB_COLS] = {
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	{0},
	{0},
	{0},
	};

struct kbc_gpio {
	int num;		/* logical row or column number */
	uint32_t port;
	int pin;
};

#if defined(BOARD_daisy) || defined(BOARD_snow)
static const uint32_t ports[] = { GPIO_B, GPIO_C, GPIO_D };
#else
#error "Need to specify GPIO ports used by keyboard"
#endif

/* Provide a default function in case the board doesn't have one */
void __board_keyboard_scan_ready(void)
{
}

void board_keyboard_scan_ready(void)
		__attribute__((weak, alias("__board_keyboard_scan_ready")));

/*
 * keyboard state FIFO (ring buffer)
 */
#define KB_FIFO_DEPTH	8	/* FIXME: this is pretty huge */
static uint8_t kb_fifo[KB_FIFO_DEPTH][KB_COLS];
static int kb_fifo_start;	/* first entry */
static int kb_fifo_end;		/* last entry */
static int kb_fifo_entries;	/* number of existing entries */
static struct mutex kb_fifo_mutex;

void keyboard_fifo_lock(void)
{
	mutex_lock(&kb_fifo_mutex);
}

void keyboard_fifo_unlock(void)
{
	mutex_unlock(&kb_fifo_mutex);
}

int keyboard_fifo_empty(void)
{
	return kb_fifo_entries ? 0 : 1;
}

/* clear keyboard state variables */
void keyboard_clear_state(void)
{
	int i;

	CPRINTF("clearing keyboard fifo\n");
	kb_fifo_start = 0;
	kb_fifo_end = 0;
	kb_fifo_entries = 0;
	for (i = 0; i < KB_FIFO_DEPTH; i++)
		memset(kb_fifo[i], 0, KB_COLS);
}

/**
  * Push keyboard state into FIFO
  *
  * @return EC error code
  */
static int kb_push(uint8_t *buffp)
{
	int ret = EC_SUCCESS;

//	CPRINTF("kb_push (before), start: %d, end: %d, entries: %d\n",
//			kb_fifo_start, kb_fifo_end, kb_fifo_entries);
	keyboard_fifo_lock();
	if (kb_fifo_entries == KB_FIFO_DEPTH) {
		CPRINTF("%s: FIFO depth reached\n", __func__);
		goto kb_push_done;
	}

	memcpy(kb_fifo[kb_fifo_end], buffp, KB_COLS);

	if (kb_fifo_end == KB_FIFO_DEPTH - 1)
		kb_fifo_end = 0;
	else
		kb_fifo_end++;

	kb_fifo_entries++;

kb_push_done:
//	CPRINTF("kb_push (after), new start: %d, end: %d, entries: %d\n",
//			kb_fifo_start, kb_fifo_end, kb_fifo_entries);
	keyboard_fifo_unlock();
	return ret;
}

/**
  * Pop keyboard state from FIFO
  *
  * @return EC_SUCCESS if entry popped, EC_ERROR_UNKNOWN if FIFO is empty
  */
static int kb_pop(uint8_t *buffp)
{
	int ret = EC_SUCCESS;

//	CPRINTF("kb_pop (before): start: %d, end: %d, entries: %d\n",
//			kb_fifo_start, kb_fifo_end, kb_fifo_entries);
	keyboard_fifo_lock();
	if (!kb_fifo_entries) {
		CPRINTF("%s: No entries remaining in FIFO\n", __func__);
		/* return empty state */
		memset(buffp, 0, KB_COLS);
		ret = EC_ERROR_UNKNOWN;
		goto kb_pop_done;
	}

	memcpy(buffp, kb_fifo[kb_fifo_start], KB_COLS);

	if (kb_fifo_start ==  KB_FIFO_DEPTH - 1)
		kb_fifo_start = 0;
	else
		kb_fifo_start++;

	kb_fifo_entries--;

kb_pop_done:
//	CPRINTF("kb_pop (after), new start: %d, end: %d, entries: %d\n",
//			kb_fifo_start, kb_fifo_end, kb_fifo_entries);
	keyboard_fifo_unlock();
	return ret;
}

static int command_kb_clear(int argc, char **argv)
{
	keyboard_clear_state();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kb_clear, command_kb_clear);

static int command_kb_status(int argc, char **argv)
{
	int i;

	CPRINTF("kb_fifo_start: %d\n", kb_fifo_start);
	CPRINTF("kb_fifo_end: %d\n", kb_fifo_end);
	CPRINTF("kb_fifo_entries: %d\n", kb_fifo_entries);

	for (i = 0; i < KB_FIFO_DEPTH; i++) {
		int j;
		CPRINTF("kb_fifo[%d]: ", i);
		for (j = 0; j < KB_COLS; j++) {
			CPRINTF("%02x ", kb_fifo[i][j]);
		}
		CPRINTF("\n");
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kb_status, command_kb_status);

static int command_kb_inject(int argc, char **argv)
{
	uint8_t buf[KB_COLS];
	int num_presses, row, col;
	int i;

	if (argc != 4) {
		CPRINTF("usage: kb_inject <row> <col> <num_presses>\n");
		return EC_ERROR_UNKNOWN;
	}

	row = atoi(argv[1]);
	col = atoi(argv[2]);
	num_presses = atoi(argv[3]);

	if ((col < 0) || (col > KB_COLS)) {
		CPRINTF("column must be within 0-%d\n", KB_COLS);
		return EC_ERROR_UNKNOWN;
	}

	if ((row < 0) || (row > 7)) {
		CPRINTF("row must be within 0-7\n");
		return EC_ERROR_UNKNOWN;
	}

	CPRINTF("injecting %d keystrokes (r%d c%d, byte %02x)\n",
	        num_presses, row, col, 1 << row);
	for (i = 0; i < num_presses; i++) {
		/* press */
		memset(buf, 0, KB_COLS);
		buf[col] |= 1 << row;
		kb_push(buf);
		board_interrupt_host();

		/* release */
		memset(buf, 0, KB_COLS);
		kb_push(buf);
		board_interrupt_host();
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kb_inject, command_kb_inject);


static void select_column(int col)
{
	int i, done = 0;

	for (i = 0; i < ARRAY_SIZE(ports); i++) {
		uint32_t bsrr = 0;
		int j;

		for (j = GPIO_KB_OUT00; j <= GPIO_KB_OUT12; j++) {
			if (gpio_list[j].port != ports[i])
				continue;

			if (col == COL_ASSERT_ALL) {
				/* drive low (clear output data) */
				bsrr |= gpio_list[j].mask << 16;
			} else if (col == COL_TRI_STATE_ALL) {
				/* put column in hi-Z state (set output data) */
				bsrr |= gpio_list[j].mask;
			} else {
				/* drive specified column low, others => hi-Z */
				if (j - GPIO_KB_OUT00 == col) {
					/* to avoid conflict, tri-state all
					 * columns first, then assert column */
					select_column(COL_TRI_STATE_ALL);
					bsrr |= gpio_list[j].mask << 16;
					done = 1;
					break;
				}
			}
		}

		if (bsrr)
			STM32_GPIO_BSRR_OFF(ports[i]) = bsrr;

		if (done)
			break;
	}
}


int keyboard_scan_init(void)
{
	int i;

	/* Tri-state (put into Hi-Z) the outputs */
	select_column(COL_TRI_STATE_ALL);

	/* initialize raw state since host may request it before
	 * a key has been pressed (e.g. during keyboard driver init) */
	for (i = 0; i < ARRAY_SIZE(raw_state); i++)
		raw_state[i] = 0x00;

	/* TODO: method to set which keyboard we have, so we set the actual
	 * key mask properly */
	actual_key_mask = actual_key_masks[0];

	return EC_SUCCESS;
}


void wait_for_interrupt(void)
{
	uint32_t pr_before, pr_after;

	/* Assert all outputs would trigger un-wanted interrupts.
	 * Clear them before enable interrupt. */
	pr_before = STM32_EXTI_PR;
	select_column(COL_ASSERT_ALL);
	pr_after = STM32_EXTI_PR;
	STM32_EXTI_PR |= ((pr_after & ~pr_before) & IRQ_MASK);

	STM32_EXTI_IMR |= IRQ_MASK;	/* 1: unmask interrupt */
}


void enter_polling_mode(void)
{
	STM32_EXTI_IMR &= ~IRQ_MASK;	/* 0: mask interrupts */
	select_column(COL_TRI_STATE_ALL);
}


/* Returns 1 if any key is still pressed. 0 if no key is pressed. */
static int check_keys_changed(void)
{
	int c;
	uint8_t r;
	int change = 0;
	int num_press = 0;

	for (c = 0; c < KB_COLS; c++) {
		uint16_t tmp;

		/* Select column, then wait a bit for it to settle */
		select_column(c);
		udelay(50);

		r = 0;
		tmp = STM32_GPIO_IDR(C);
		/* KB_COL00:04 = PC8:12 */
		if (tmp & (1 << 8))
			r |= 1 << 0;
		if (tmp & (1 << 9))
			r |= 1 << 1;
		if (tmp & (1 << 10))
			r |= 1 << 2;
		if (tmp & (1 << 11))
			r |= 1 << 3;
		if (tmp & (1 << 12))
			r |= 1 << 4;
		/* KB_COL05:06 = PC14:15 */
		if (tmp & (1 << 14))
			r |= 1 << 5;
		if (tmp & (1 << 15))
			r |= 1 << 6;

		tmp = STM32_GPIO_IDR(D);
		/* KB_COL07 = PD2 */
		if (tmp & (1 << 2))
			r |= 1 << 7;

		/* Invert it so 0=not pressed, 1=pressed */
		r ^= 0xff;
		/* Mask off keys that don't exist so they never show
		 * as pressed */
		r &= actual_key_mask[c];

#ifdef OR_WITH_CURRENT_STATE_FOR_TESTING
		/* KLUDGE - or current state in, so we can make sure
		 * all the lines are hooked up */
		r |= raw_state[c];
#endif

		/* Check for changes */
		if (r != raw_state[c]) {
			raw_state[c] = r;
			change = 1;
		}
	}
	select_column(COL_TRI_STATE_ALL);

	/* Count number of key pressed */
	for (c = 0; c < KB_COLS; c++) {
		if (raw_state[c])
			++num_press;
	}

	if (change) {
		int tries = 5;

		CPRINTF("[%d keys pressed: ", num_press);
		for (c = 0; c < KB_COLS; c++) {
			if (raw_state[c])
				CPRINTF(" %02x", raw_state[c]);
			else
				CPUTS(" --");
		}
		CPUTS("]\n");

		while (tries) {
			if (kb_push(raw_state) == EC_SUCCESS)
				break;
			tries--;
			usleep(10000);
		}
		if (!tries) {
			CPRINTF("dropped keystroke\n");
		} else {
			board_keyboard_scan_ready();
			board_interrupt_host();
		}
	}

	return num_press ? 1 : 0;
}


void keyboard_scan_task(void)
{
	int key_press_timer = 0;

	/* Enable interrupts for keyboard matrix inputs */
	gpio_enable_interrupt(GPIO_KB_IN00);
	gpio_enable_interrupt(GPIO_KB_IN01);
	gpio_enable_interrupt(GPIO_KB_IN02);
	gpio_enable_interrupt(GPIO_KB_IN03);
	gpio_enable_interrupt(GPIO_KB_IN04);
	gpio_enable_interrupt(GPIO_KB_IN05);
	gpio_enable_interrupt(GPIO_KB_IN06);
	gpio_enable_interrupt(GPIO_KB_IN07);

	while (1) {
		wait_for_interrupt();
		task_wait_event(-1);

		enter_polling_mode();
		/* Busy polling keyboard state. */
		while (1) {
			/* sleep for debounce. */
			usleep(SCAN_LOOP_DELAY);
			/* Check for keys down */
			if (check_keys_changed()) {
				key_press_timer = 0;
			} else {
				if (++key_press_timer >=
				    (POLLING_MODE_TIMEOUT / SCAN_LOOP_DELAY)) {
					key_press_timer = 0;
					break;  /* exit the while loop */
				}
			}
		}

		/* TODO: (crosbug.com/p/7484) A race condition here.
		 *       If a key state is changed here (before interrupt is
		 *       enabled), it will be lost.
		 */
	}
}


void matrix_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_KEYSCAN);
}

int keyboard_has_char()
{
	/* TODO: needs to be implemented */
	return 0;
}

void keyboard_put_char(uint8_t chr, int send_irq)
{
	/* TODO: needs to be implemented */
}

int keyboard_scan_recovery_pressed(void)
{
	/* TODO: (crosbug.com/p/8573) needs to be implemented */
	return 0;
}

static uint8_t tmp[KB_COLS];
int keyboard_get_scan(uint8_t **buffp, int max_bytes)
{
#if 0
	*buffp = saved_state;
	return sizeof(saved_state);
#endif
	kb_pop(tmp);
	*buffp = tmp;
	return KB_COLS;
}
