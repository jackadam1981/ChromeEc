/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Message FIFO. For now this is only used to store keyboard info.
 */

#include "board.h"
#include "common.h"
#include "console.h"
#include "keyboard_scan.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ## args)

/* FIFO task will normally wake up periodically to interrupt the AP
   if there are entries in the FIFO. */
#define FIFO_LOOP_DELAY		100000	/* 100ms */

#define KB_FIFO_DEPTH		16	/* FIXME: this is pretty huge */

static int kb_fifo_start;		/* first entry */
static int kb_fifo_end;			/* last entry */
static int kb_fifo_entries;		/* number of existing entries */
static struct mutex kb_fifo_mutex;
static uint8_t kb_fifo[KB_FIFO_DEPTH][KB_OUTPUTS];

/**
 * Tell keyboard FIFO task to delay more than usual.
 *
 * This is used by callers to help prevent too many AP interrupts. The
 * FIFO will skip an iteration and allow extra time for the AP transaction
 * to take place.
 */
static int work_in_progress;
void keyboard_fifo_work_in_progress(void)
{
	work_in_progress = 1;
}

static void keyboard_fifo_lock(void)
{
	mutex_lock(&kb_fifo_mutex);
}

static void keyboard_fifo_unlock(void)
{
	mutex_unlock(&kb_fifo_mutex);
}

static int keyboard_fifo_empty(void)
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
		memset(kb_fifo[i], 0, KB_OUTPUTS);
}

/**
  * Push keyboard state into FIFO
  *
  * @return EC_SUCCESS if entry pushed, EC_ERROR_OVERFLOW if FIFO is full
  */
int keyboard_fifo_push(uint8_t *buffp)
{
	int ret = EC_SUCCESS;

	keyboard_fifo_lock();
	if (kb_fifo_entries == KB_FIFO_DEPTH) {
		CPRINTF("%s: FIFO depth reached\n", __func__);
		ret = EC_ERROR_OVERFLOW;
		goto keyboard_fifo_push_done;
	}

	memcpy(kb_fifo[kb_fifo_end], buffp, KB_OUTPUTS);

	if (kb_fifo_end == KB_FIFO_DEPTH - 1)
		kb_fifo_end = 0;
	else
		kb_fifo_end++;

	kb_fifo_entries++;

keyboard_fifo_push_done:
	keyboard_fifo_unlock();
	return ret;
}

/**
  * Pop keyboard state from FIFO
  *
  * @return EC_SUCCESS if entry popped, EC_ERROR_UNKNOWN if FIFO is empty
  */
int keyboard_fifo_pop(uint8_t *buffp)
{
	int ret = EC_SUCCESS;

	keyboard_fifo_lock();
	if (!kb_fifo_entries) {
		CPRINTF("%s: No entries remaining in FIFO\n", __func__);
		/* return empty state */
		memset(buffp, 0, KB_OUTPUTS);
		ret = EC_ERROR_UNKNOWN;
		goto keyboard_fifo_pop_done;
	}

	memcpy(buffp, kb_fifo[kb_fifo_start], KB_OUTPUTS);

	if (kb_fifo_start ==  KB_FIFO_DEPTH - 1)
		kb_fifo_start = 0;
	else
		kb_fifo_start++;

	kb_fifo_entries--;

keyboard_fifo_pop_done:
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

	cprintf(CC_COMMAND, "kb_fifo_start: %d\n", kb_fifo_start);
	cprintf(CC_COMMAND, "kb_fifo_end: %d\n", kb_fifo_end);
	cprintf(CC_COMMAND, "kb_fifo_entries: %d\n", kb_fifo_entries);

	for (i = 0; i < KB_FIFO_DEPTH; i++) {
		int j;
		cprintf(CC_COMMAND, "kb_fifo[%d]: ", i);
		for (j = 0; j < KB_OUTPUTS; j++)
			cprintf(CC_COMMAND, "%02x ", kb_fifo[i][j]);
		cprintf(CC_COMMAND, "\n");
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kb_status, command_kb_status);

static int command_kb_inject(int argc, char **argv)
{
	uint8_t buf[KB_OUTPUTS];
	int num_presses, row, col;
	int i;

	if (argc != 4) {
		cprintf(CC_COMMAND, "usage: kb_inject <row> <col> <num>\n");
		return EC_ERROR_UNKNOWN;
	}

	row = atoi(argv[1]);
	col = atoi(argv[2]);
	num_presses = atoi(argv[3]);

	if ((col < 0) || (col > KB_OUTPUTS)) {
		cprintf(CC_COMMAND, "column must be within 0-%d\n", KB_OUTPUTS);
		return EC_ERROR_UNKNOWN;
	}

	if ((row < 0) || (row > 7)) {
		cprintf(CC_COMMAND, "row must be within 0-7\n");
		return EC_ERROR_UNKNOWN;
	}

	cprintf(CC_COMMAND, "injecting %d keystrokes (r%d c%d, byte %02x)\n",
			num_presses, row, col, 1 << row);
	for (i = 0; i < num_presses; i++) {
		/* press */
		memset(buf, 0, KB_OUTPUTS);
		buf[col] |= 1 << row;
		keyboard_fifo_push(buf);
		board_interrupt_host();

		/* release */
		memset(buf, 0, KB_OUTPUTS);
		keyboard_fifo_push(buf);
		board_interrupt_host();
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kb_inject, command_kb_inject);

void keyboard_fifo_task(void)
{
	while (1) {
		usleep(FIFO_LOOP_DELAY);

		if (work_in_progress) {
			work_in_progress = 0;
			continue;
		}

		if (!keyboard_fifo_empty()) {
			CPRINTF("interrupting host\n");
			board_interrupt_host();
		}
	}
}
