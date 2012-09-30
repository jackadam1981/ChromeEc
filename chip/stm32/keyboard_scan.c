/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Keyboard scanner module for Chrome EC
 *
 * TODO: Finish cleaning up nomenclature (cols/rows/inputs/outputs),
 */

#include "atomic.h"
#include "board.h"
#include "console.h"
#include "gpio.h"
#include "host_command.h"
#include "keyboard.h"
#include "keyboard_scan.h"
#include "registers.h"
#include "system.h"
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

/* 15:14, 12:8, 2 */
#define IRQ_MASK 0xdf04

static struct mutex scanning_enabled;

/* The keyboard state from the last read */
static uint8_t raw_state[KB_OUTPUTS];

/* Key masks for special boot keys */
#define MASK_INDEX_ESC     1
#define MASK_VALUE_ESC     0x02
#define MASK_INDEX_REFRESH 2
#define MASK_VALUE_REFRESH 0x04

/* Key masks and values for warm reboot combination */
#define MASK_INDEX_KEYR		3
#define MASK_VALUE_KEYR		0x80
#define MASK_INDEX_VOL_UP	4
#define MASK_VALUE_VOL_UP	0x01
#define MASK_INDEX_RIGHT_ALT	10
#define MASK_VALUE_RIGHT_ALT	0x01
#define MASK_INDEX_LEFT_ALT	10
#define MASK_VALUE_LEFT_ALT	0x40

struct kbc_gpio {
	int num;		/* logical row or column number */
	uint32_t port;
	int pin;
};

#ifdef CONFIG_KEY_EVENT_EMUL
/* Globals for key event emulation */

static uint8_t bc_memory[EC_MKBP_PROGRAM_MAX_LENGTH];
static uint8_t bc_running;
static int bc_length;
static int bc_next_delay;

static void bc_run_emulation(void);
#endif

#if defined(BOARD_daisy) || defined(BOARD_snow) || defined(BOARD_spring)
static const uint32_t ports[] = { GPIO_B, GPIO_C, GPIO_D };
#else
#error "Need to specify GPIO ports used by keyboard"
#endif

#ifdef CONFIG_KEYSCAN_SEQ
struct keyscan_item {
	timestamp_t time;	/* timestamp to present this item */
	uint16_t beat;		/* beat number to present this item */
	uint8_t done;		/* 1 if we managed to present this */
	uint8_t scan[KB_OUTPUTS];
};

enum {
	KEYSCAN_MAX_LENGTH		= 25,
	KEYSCAN_SEQ_START_DELAY_US	= 10000,
};

static uint8_t keyscan_seq_count;
static int8_t keyscan_seq_upto = -1;
static struct keyscan_item keyscan_items[KEYSCAN_MAX_LENGTH];
struct keyscan_item *keyscan_seq_cur;
#else
#define keyscan_seq_upto	(-1)
#define keyscan_seq_get_next()	NULL
#endif

/* Provide a default function in case the board doesn't have one */
void __board_keyboard_suppress_noise(void)
{
}

void board_keyboard_suppress_noise(void)
		__attribute__((weak, alias("__board_keyboard_suppress_noise")));

#define KB_FIFO_DEPTH		16	/* FIXME: this is pretty huge */
static uint32_t kb_fifo_start;		/* first entry */
static uint32_t kb_fifo_end;		/* last entry */
static uint32_t kb_fifo_entries;	/* number of existing entries */
static uint8_t kb_fifo[KB_FIFO_DEPTH][KB_OUTPUTS];

/*
 * Our configuration. The debounce parameters are not yet supported.
 */
static struct ec_mkbp_config config = {
	.poll_timeout_us = 100 * 1000,
	.scan_period_us = 3000,
	.pre_scan_us = 100,
	.post_scan_relax_us = 1000,
	.column_settle_us = 50,
	.disable_wait_us = 10 * 1000,
	.flags = EC_MKBP_FLAGS_ENABLE,
	.fifo_max_depth = KB_FIFO_DEPTH,
	/* key_mask is set to 0xff on start-up */
};

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
  * Add keyboard state into FIFO
  *
  * @return EC_SUCCESS if entry added, EC_ERROR_OVERFLOW if FIFO is full
  */
static int kb_fifo_add(uint8_t *buffp)
{
	int ret = EC_SUCCESS;

	if (kb_fifo_entries == config.fifo_max_depth) {
		CPRINTF("%s: FIFO depth %d reached\n", __func__,
			config.fifo_max_depth);
		ret = EC_ERROR_OVERFLOW;
		goto kb_fifo_push_done;
	}

	memcpy(kb_fifo[kb_fifo_end], buffp, KB_OUTPUTS);

	kb_fifo_end = (kb_fifo_end + 1) % KB_FIFO_DEPTH;

	atomic_add(&kb_fifo_entries, 1);

kb_fifo_push_done:
	return ret;
}

/**
  * Pop keyboard state from FIFO
  *
  * @return EC_SUCCESS if entry popped, EC_ERROR_UNKNOWN if FIFO is empty
  */
static int kb_fifo_remove(uint8_t *buffp)
{
	if (!kb_fifo_entries) {
		/* no entry remaining in FIFO : return last known state */
		int last = (kb_fifo_start + KB_FIFO_DEPTH - 1) % KB_FIFO_DEPTH;
		memcpy(buffp, kb_fifo[last], KB_OUTPUTS);

		/*
		 * Bail out without changing any FIFO indices and let the
		 * caller know something strange happened. The buffer will
		 * will contain the last known state of the keyboard.
		 */
		return EC_ERROR_UNKNOWN;
	}
	memcpy(buffp, kb_fifo[kb_fifo_start], KB_OUTPUTS);

	kb_fifo_start = (kb_fifo_start + 1) % KB_FIFO_DEPTH;

	atomic_sub(&kb_fifo_entries, 1);

	return EC_SUCCESS;
}

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


void setup_interrupts(void)
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

static int check_warm_reboot_keys(void)
{
	if (raw_state[MASK_INDEX_KEYR] == MASK_VALUE_KEYR &&
		  raw_state[MASK_INDEX_VOL_UP] == MASK_VALUE_VOL_UP &&
		  (raw_state[MASK_INDEX_RIGHT_ALT] == MASK_VALUE_RIGHT_ALT ||
		  raw_state[MASK_INDEX_LEFT_ALT] == MASK_VALUE_LEFT_ALT))
		return 1;

	return 0;
}

#ifdef CONFIG_KEYSCAN_SEQ
/**
 * Get the current item in the keyscan sequence
 *
 * This looks at the current time, and returns the correct key scan for that
 * time.
 *
 * @return pointer to keyscan item, or NULL if none
 */
static const struct keyscan_item *keyscan_seq_get(void)
{
	struct keyscan_item *ksi;

	if (keyscan_seq_upto == -1)
		return NULL;

	ksi = &keyscan_items[keyscan_seq_upto];
	while (keyscan_seq_upto < keyscan_seq_count) {
		/*
		 * If we haven't reached the time for the next one, return
		 * this one.
		 */
// 		ccprintf("%T: test upto=%d, now=%u, ksi->time=%u\n",
// 			 keyscan_seq_upto, get_time().le.lo,
// 			 ksi->time.le.lo);
		if (!timestamp_expired(ksi->time, NULL)) {
// 			ccprintf("%T: keyscan_seq upto=%u\n",
// 				 keyscan_seq_upto);
			/* Yippee, we get to present this one! */
			if (keyscan_seq_cur)
				keyscan_seq_cur->done = 1;
			return keyscan_seq_cur;
		}

		keyscan_seq_cur = ksi;
		keyscan_seq_upto++;
		ksi++;
	}

	ccprintf("%T: keyscan_seq done, upto=%d\n", keyscan_seq_upto);
	keyscan_seq_upto = -1;
	return NULL;
}

#endif /* CONFIG_KEYSCAN_SEQ */

/* Returns 1 if any key is still pressed. 0 if no key is pressed. */
static int check_keys_changed(void)
{
	int c;
	uint8_t r;
	int change = 0;
	int num_press = 0;
	const struct keyscan_item *item;

	for (c = 0; c < KB_OUTPUTS; c++) {
		uint16_t tmp;

		/* Select column, then wait a bit for it to settle */
		select_column(c);
		udelay(config.column_settle_us);

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

		/* Use simulated keyscan sequence instead if active */
		item = keyscan_seq_get();

		/* Invert it so 0=not pressed, 1=pressed */
		r ^= 0xff;
		if (item)
			r = item->scan[c];
		/* Mask off keys that don't exist so they never show
		 * as pressed */
		r &= config.key_mask[c];

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
	for (c = 0; c < KB_OUTPUTS; c++) {
		if (raw_state[c])
			++num_press;
	}

	if (change) {
		board_keyboard_suppress_noise();

		CPRINTF("[%d keys pressed: ", num_press);
		for (c = 0; c < KB_OUTPUTS; c++) {
			if (raw_state[c])
				CPRINTF(" %02x", raw_state[c]);
			else
				CPUTS(" --");
		}
		CPUTS("]\n");

		if (num_press == 3) {
			if (check_warm_reboot_keys()) {
				keyboard_clear_state();
				system_warm_reboot();
				return 0;
			}
		}

		if (kb_fifo_add(raw_state) == EC_SUCCESS)
			board_interrupt_host(1);
		else
			CPRINTF("dropped keystroke\n");
	}

	return num_press ? 1 : 0;
}


/* Returns non-zero if the user has triggered a recovery reset by pushing
 * Power + Refresh + ESC. */
static int check_recovery_key(void)
{
	int c;

	/* check the recovery key only if we're booting due to a
	 * reset-pin-caused reset. */
	if (!(system_get_reset_flags() & RESET_FLAG_RESET_PIN))
		return 0;

	/* cold boot : Power + Refresh were pressed,
	 * check if ESC is also pressed for recovery. */
	if (!(raw_state[MASK_INDEX_ESC] & MASK_VALUE_ESC))
		return 0;

	/* Make sure only other allowed keys are pressed.  This protects
	 * against accidentally triggering the special key when a cat sits on
	 * your keyboard.  Currently, only the requested key and ESC are
	 * allowed. */
	for (c = 0; c < KB_OUTPUTS; c++) {
		if (raw_state[c] &&
		(c != MASK_INDEX_ESC || raw_state[c] != MASK_VALUE_ESC) &&
		(c != MASK_INDEX_REFRESH || raw_state[c] != MASK_VALUE_REFRESH))
			return 0;  /* Additional disallowed key pressed */
	}

	CPRINTF("Keyboard RECOVERY detected !\n");

	host_set_single_event(EC_HOST_EVENT_KEYBOARD_RECOVERY);

	return 1;
}


int keyboard_scan_init(void)
{
	/* Tri-state (put into Hi-Z) the outputs */
	select_column(COL_TRI_STATE_ALL);

	/* Initialize raw state */
	check_keys_changed();

	/* is recovery key pressed on cold startup ? */
	check_recovery_key();

	return EC_SUCCESS;
}

/* Scan the keyboard until all keys are released */
static void scan_keyboard(void)
{
	timestamp_t poll_deadline, start;
	int keys_changed = 1;

	mutex_lock(&scanning_enabled);
	setup_interrupts();
	mutex_unlock(&scanning_enabled);

	/* Wait until we get an interrupt */
	if (keyscan_seq_upto == -1) {
		task_wait_event(-1);
	}

#ifdef CONFIG_KEY_EVENT_EMUL
	if (bc_running) {
		bc_run_emulation();
		bc_running = 0;
		return;
	}
#endif

	enter_polling_mode();

	usleep(config.pre_scan_us);
	ccprintf("%s: %d\n", __func__, __LINE__);

	/* Busy polling keyboard state. */
	start = get_time();
	do {
		int wait_time;

		/* If we saw any keys pressed, reset deadline */
		if (keys_changed)
			poll_deadline.val = start.val + config.poll_timeout_us;

		/*
		 * Scan immediately, with no delay. We don't seem
		 * to see switch bounce on snow.
		 */
		mutex_lock(&scanning_enabled);
		keys_changed = check_keys_changed();
		mutex_unlock(&scanning_enabled);

		/* wait a bit before scanning again */
		wait_time = config.scan_period_us -
				(get_time().val - start.val);
		if (wait_time < config.post_scan_relax_us) {
			CPRINTF("Key scan relax time enforced\n");
			wait_time = config.post_scan_relax_us;
		}
		usleep(wait_time);
		start = get_time();
	} while (!timestamp_expired(poll_deadline, &start)
		&& (config.flags & EC_MKBP_FLAGS_ENABLE));
	/* TODO: (crosbug.com/p/7484) A race condition here.
	 *       If a key state is changed here (before interrupt is
	 *       enabled), it will be lost.
	 */
}

void keyboard_scan_task(void)
{
	/* to start, allow all keys */
	memset(config.key_mask, 0xff, sizeof(config.key_mask));

	/* Enable interrupts for keyboard matrix inputs */
	gpio_enable_interrupt(GPIO_KB_IN00);
	gpio_enable_interrupt(GPIO_KB_IN01);
	gpio_enable_interrupt(GPIO_KB_IN02);
	gpio_enable_interrupt(GPIO_KB_IN03);
	gpio_enable_interrupt(GPIO_KB_IN04);
	gpio_enable_interrupt(GPIO_KB_IN05);
	gpio_enable_interrupt(GPIO_KB_IN06);
	gpio_enable_interrupt(GPIO_KB_IN07);
	ccprintf("key task\n");

	for (;;) {
		if (config.flags & EC_MKBP_FLAGS_ENABLE) {
			scan_keyboard();
		} else {
			select_column(COL_TRI_STATE_ALL);
			usleep(config.disable_wait_us);
		}
	}
}


void matrix_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_KEYSCAN);
}

int keyboard_has_char(void)
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
	return host_get_events() &
	EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_RECOVERY);
}

static int keyboard_get_scan(struct host_cmd_handler_args *args)
{
	kb_fifo_remove(args->response);
	if (!kb_fifo_entries)
		board_interrupt_host(0);

	args->response_size = KB_OUTPUTS;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_MKBP_STATE,
		     keyboard_get_scan,
		     EC_VER_MASK(0));

static int keyboard_get_info(struct host_cmd_handler_args *args)
{
	struct ec_response_mkbp_info *r = args->response;

	r->rows = 8;
	r->cols = KB_OUTPUTS;
	r->switches = 0;

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_MKBP_INFO,
		     keyboard_get_info,
		     EC_VER_MASK(0));

void keyboard_enable_scanning(int enable)
{
	if (enable) {
		mutex_unlock(&scanning_enabled);
		task_wake(TASK_ID_KEYSCAN);
	} else {
		mutex_lock(&scanning_enabled);
		select_column(COL_TRI_STATE_ALL);
	}
}


static int command_keyboard_press(int argc, char **argv)
{
	int r, c, p;
	char *e;

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	c = strtoi(argv[1], &e, 0);
	if (*e || c < 0 || c >= KB_OUTPUTS)
		return EC_ERROR_PARAM1;

	r = strtoi(argv[2], &e, 0);
	if (*e || r < 0 || r >= 8)
		return EC_ERROR_PARAM2;

	p = strtoi(argv[3], &e, 0);
	if (*e || p < 0 || p > 1)
		return EC_ERROR_PARAM3;

	if (p)
		raw_state[c] |= (1 << r);
	else
		raw_state[c] &= ~(1 << r);

	if (kb_fifo_add(raw_state) == EC_SUCCESS)
		board_interrupt_host(1);
	else
		ccprintf("dropped keystroke\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(kbpress, command_keyboard_press,
			"[col] [row] [0 | 1]",
			"Simulate keypress",
			NULL);


#ifdef CONFIG_KEY_EVENT_EMUL
/* Keyboard emulation.
 *
 * NOTE: this raises the stack requirement for the keyboard scan thread from
 * 256 to 512 bytes.
 */

static int bc_add_program(const uint8_t *bytecodes, int length)
{
	/* Copy to program memory if there is enough room */
	if (length + bc_length <= sizeof(bc_memory)) {
		memcpy(bc_memory + bc_length, bytecodes, length);
		bc_length += length;
		return EC_RES_SUCCESS;
	} else {
		CPRINTF("[%T bytecode overflow]\n");
		return EC_RES_ERROR;
	}
}

static int bc_start_program(void)
{
	if (bc_length > 0) {
		bc_running = 1;
		task_wake(TASK_ID_KEYSCAN);
		return EC_RES_SUCCESS;
	} else {
		CPRINTF("[%T empty program]\n");
		return EC_RES_ERROR;
	}
}

void send_emulated_event(void)
{
	if (bc_next_delay > 0)
		usleep(bc_next_delay);
	bc_next_delay = 0;
	if (kb_fifo_add(raw_state) == EC_SUCCESS)
		board_interrupt_host(1);
	else
		CPRINTF("dropped emulated keystroke\n");
}

static const int bc_delays[] = {
	/* Delays in microseconds.  These are roughly exponential, with the
	 * first element used for stress testing.  It's not clear that we need
	 * the large numbers.
	 */
	0,
	1000,		2000,		5000,
	10000,		20000,		50000,
	100000,		200000,		500000,
	1000000,	2000000,	5000000,
	10000000,	20000000,	50000000
};

/* Executes one instruction and advances IP.  Returns 1 on success, 0 on
 * failure.
 *
 * Byte codes:
 *
 * 0RRRCCCC - toggle state of key at row R and column C
 *
 * 1000XXXX - pause for f(x) milliseconds where f(x) is roughly 2^(x-1) (see
 * bc_delays table), but f(0) = 0 for stress testing.
 *
 * 1001SSTT - prefix: repeat the following S+1 instructions T+2 times
 *
 * Before running out of opcodes, leave a few for two-byte opcodes.
 */

static int bc_execute(int *pip);
static int bc_execute(int *pip)
{
	uint8_t instruction;

	if (*pip >= bc_length) {
		CPRINTF("[%T IP overflow]\n");
		return 0;
	}
	CPRINTF("executing %2x at %d\n", bc_memory[*pip], *pip);

	instruction = bc_memory[*pip];

	if ((instruction & 0x80) == 0) {
		/* key toggle event */
		int row = (instruction & 0x70) >> 4;
		int column = instruction & 0xf;
		uint8_t bit = 1 << row;
		raw_state[column] ^= bit;
		send_emulated_event();
		(*pip)++;
		return 1;
	} else {
		switch (instruction & 0xf0) {
		case 0x80:
			/* pause event */
			bc_next_delay += bc_delays[instruction & 0xf];
			(*pip)++;
			return 1;
		case 0x90: {
			/* repeat instruction */
			int n = ((instruction & 0xc) >> 2) + 1;
			int times = (instruction & 0x3) + 2;
			int i, j, oip;
			for (i = 0; i < times; i++) {
				oip = *pip + 1;
				for (j = 0; j < n; j++) {
					if (!bc_execute(&oip))
						return 0;
				}
			}
			*pip = oip;
			return 1;
		}
		default:
			CPRINTF("[%T unknown bytecode 0x%02x]\n", instruction);
			(*pip)++;
			return 1;
		}
	}
}

static void bc_run_emulation(void)
{
	int ip;

	memset(raw_state, 0, sizeof(raw_state));
	send_emulated_event();
	usleep(500 * 1000);

	for (ip = 0; ip < bc_length;) {
		if (!bc_execute(&ip))
			break;
	}
}

/* Through this command the AP can download bytecode sequences
 * and execute them to emulate keyboard events.
 */
static int keyboard_program(struct host_cmd_handler_args *args)
{
	switch (((unsigned char *)args->params)[0]) {

	case EC_MKBP_PROGRAM_CLEAR:
		bc_length = 0;
		return EC_RES_SUCCESS;

	case EC_MKBP_PROGRAM_SEND:
		return bc_add_program(args->params + 1, args->params_size - 1);

	case EC_MKBP_PROGRAM_START:
		return bc_start_program();
	}

	return EC_RES_ERROR;
}
DECLARE_HOST_COMMAND(EC_CMD_MKBP_PROGRAM,
		     keyboard_program,
		     EC_VER_MASK(0));
#endif /* CONFIG_KEY_EVENT_EMUL */

#ifdef CONFIG_KEYSCAN_SEQ
static void keyscan_seq_start(int beat_us)
{
	timestamp_t start;
	int i;

	start = get_time();
	start.val += KEYSCAN_SEQ_START_DELAY_US;
// 	ccprintf("seq_start %u\n", start.le.lo);
	for (i = 0; i < keyscan_seq_count; i++) {
		struct keyscan_item *ksi = &keyscan_items[i];

		ksi->time = start;
		ksi->time.val += ksi->beat * beat_us;
// 		ccprintf("%d: %d, %u\n", i, ksi->beat, ksi->time.le.lo);
	}

	keyscan_seq_upto = 0;
	keyscan_seq_cur = NULL;
	task_wake(TASK_ID_KEYSCAN);
}

static int keyscan_seq_collect(struct ec_result_keyscan_seq_ctrl *resp)
{
	struct keyscan_item *ksi;
	int i;

	resp->num_items = keyscan_seq_count;
	
	for (i = 0, ksi = keyscan_items; i < keyscan_seq_count; i++, ksi++)
		resp->done[i] = ksi->done;

	return sizeof(*resp) + keyscan_seq_count;
}

static int keyscan_seq_ctrl(struct host_cmd_handler_args *args)
{
	struct ec_params_keyscan_seq_ctrl req;

	/* For now we must do our own alignment */
	memcpy(&req, args->params, sizeof(req));
// 	ccprintf("cmd=%d, beat=%u\n", req.cmd, req.beat_us);

	switch (req.cmd) {
	case EC_KEYSCAN_SEQ_CLEAR:
		keyscan_seq_count = 0;
		break;
	case EC_KEYSCAN_SEQ_START:
		keyscan_seq_start(req.beat_us);
		break;
	case EC_KEYSCAN_SEQ_COLLECT:
		args->response_size = keyscan_seq_collect(
			(struct ec_result_keyscan_seq_ctrl *)args->response);
		break;
	default:
		return EC_RES_INVALID_COMMAND;
	}

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_KEYSCAN_SEQ_CTRL,
		     keyscan_seq_ctrl,
		     EC_VER_MASK(0));

static int keyscan_seq_add(struct host_cmd_handler_args *args)
{
	const struct ec_params_keyscan_seq_add *req = args->params;
	int i;

// 	ccprintf("num_items=%d\n", req->num_items);
	for (i = 0; i < req->num_items; i++) {
		const struct ec_params_keyscan_seq_item *item = &req->item[i];
		struct keyscan_item *ksi = &keyscan_items[keyscan_seq_count];
		uint8_t *beatp;

		if (keyscan_seq_count == KEYSCAN_MAX_LENGTH)
			return EC_RES_OVERFLOW;

		beatp = (uint8_t *)&item->beat;
		ksi->beat = *beatp + (beatp[1] << 8);
		ksi->done = 0;
		ksi->time.val = 0;
		memcpy(ksi->scan, item->scan, sizeof(item->scan));
		keyscan_seq_count++;
	}
// 	ccprintf("keyscan_seq_count=%d\n", keyscan_seq_count);

	return 0;
}

DECLARE_HOST_COMMAND(EC_CMD_KEYSCAN_SEQ_ADD,
		     keyscan_seq_add,
		     EC_VER_MASK(0));
#endif

static int host_command_mkbp_config(struct host_cmd_handler_args *args)
{
	const struct ec_params_mkbp_config *req = args->params;
	struct ec_params_mkbp_config *resp = args->response;

	switch (req->cmd) {
	case EC_MKBP_CONFIG_GET:
		resp->cmd = req->cmd;
		memcpy(&resp->config, &config, sizeof(config));
		args->response_size = sizeof(*resp);
		break;
	case EC_MKBP_CONFIG_SET:
		memcpy(&config, &req->config, sizeof(config));

		/* Do some santiy checks that could cause bad things to happen */
		if (config.fifo_max_depth < 1)
			config.fifo_max_depth = 1;
		else if (config.fifo_max_depth > KB_FIFO_DEPTH)
			config.fifo_max_depth = KB_FIFO_DEPTH;
		break;
	default:
		return EC_RES_INVALID_COMMAND;
	}

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_MKBP_CONFIG,
		     host_command_mkbp_config,
		     EC_VER_MASK(0));
