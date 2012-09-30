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

#if defined(BOARD_daisy) || defined(BOARD_snow) || defined(BOARD_spring)
static const uint32_t ports[] = { GPIO_B, GPIO_C, GPIO_D };
#else
#error "Need to specify GPIO ports used by keyboard"
#endif

/* Provide a default function in case the board doesn't have one */
void __board_keyboard_suppress_noise(void)
{
}

void board_keyboard_suppress_noise(void)
		__attribute__((weak, alias("__board_keyboard_suppress_noise")));

#define KB_FIFO_DEPTH		16	/* FIXME: this is pretty huge */
static uint32_t kb_fifo_start;		/* first entry */
static uint32_t kb_fifo_end;			/* last entry */
static uint32_t kb_fifo_entries;	/* number of existing entries */
static uint8_t kb_fifo[KB_FIFO_DEPTH][KB_OUTPUTS];

/*
 * Our configuration. The debounce parameters are not yet supported.
 */
static struct ec_mkbp_config config = {
	.valid_mask = EC_MKBP_VALID_SCAN_PERIOD | EC_MKBP_VALID_POLL_TIMEOUT |
		EC_MKBP_VALID_MIN_POST_SCAN_DELAY |
		EC_MKBP_VALID_OUTPUT_SETTLE | EC_MKBP_VALID_DEBOUNCE_DOWN |
		EC_MKBP_VALID_DEBOUNCE_UP | EC_MKBP_VALID_DISABLE_WAIT |
		EC_MKBP_VALID_FIFO_MAX_DEPTH,
	.valid_flags = EC_MKBP_FLAGS_ENABLE,
	.flags = EC_MKBP_FLAGS_ENABLE,
	.scan_period_us = 3000,
	.poll_timeout_us = 100 * 1000,
	.min_post_scan_delay_us = 1000,
	.output_settle_us = 50,
	.disable_wait_us = 10 * 1000,
	.fifo_max_depth = KB_FIFO_DEPTH,
};

#ifdef CONFIG_KEYSCAN_SEQ
struct keyscan_item {
	timestamp_t time;	/* timestamp to present this item */
	uint16_t beat;		/* beat number to present this item */
	uint8_t done;		/* 1 if we managed to present this */
	uint8_t scan[KB_OUTPUTS];
};

enum {
	/* Maximum number of scans we can quue up */
	KEYSCAN_MAX_LENGTH		= 25,

	/* Delay after 'start' request before we start emitting scans */
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

/* Set up columns so that we will get an interrupt when any key changed */
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
		udelay(config.output_settle_us);

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
	if (keyscan_seq_upto == -1)
		task_wait_event(-1);

	enter_polling_mode();

	/* Busy polling keyboard state. */
	while (1) {
		int wait_time;

		if (!(config.flags & EC_MKBP_FLAGS_ENABLE))
			break;

		/* If we saw any keys pressed, reset deadline */
		start = get_time();
		if (keys_changed)
			poll_deadline.val = start.val + config.poll_timeout_us;
		else if (timestamp_expired(poll_deadline, &start))
			break;

		/* Scan immediately, with no delay */
		mutex_lock(&scanning_enabled);
		keys_changed = check_keys_changed();
		mutex_unlock(&scanning_enabled);

		/* Wait a bit before scanning again */
		wait_time = config.scan_period_us -
				(get_time().val - start.val);
		if (wait_time < config.min_post_scan_delay_us)
			wait_time = config.min_post_scan_delay_us;
		task_wait_event(wait_time);
	}
	/*
	 * TODO: (crosbug.com/p/7484) A race condition here.
	 *       If a key state is changed here (before interrupt is
	 *       enabled), it will be lost.
	 */
}

void keyboard_scan_task(void)
{
	/* Enable interrupts for keyboard matrix inputs */
	gpio_enable_interrupt(GPIO_KB_IN00);
	gpio_enable_interrupt(GPIO_KB_IN01);
	gpio_enable_interrupt(GPIO_KB_IN02);
	gpio_enable_interrupt(GPIO_KB_IN03);
	gpio_enable_interrupt(GPIO_KB_IN04);
	gpio_enable_interrupt(GPIO_KB_IN05);
	gpio_enable_interrupt(GPIO_KB_IN06);
	gpio_enable_interrupt(GPIO_KB_IN07);

	for (;;) {
		if (config.flags & EC_MKBP_FLAGS_ENABLE) {
			scan_keyboard();
		} else {
			select_column(COL_TRI_STATE_ALL);
			task_wait_event(-1);
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
#endif /* CONFIG_KEYSCAN_SEQ */

/**
 * Copy keyscan configuration from one place to another according to flags
 *
 * This is like a structure copy, except that only selected fields are
 * copied.
 *
 * TODO(sjg@chromium.org): Consider making this table drive as ectool.
 *
 * @param src		Source config
 * @param dst		Destination config
 * @param valid_mask	Bits representing which fields to copy - each bit is
 *			from enum mkbp_config_valid
 * @param valie_flags	Bit mask controlling flags to copy. Any 1 bit means
 *			that the corresponding bit in src->flags is copied
 *			over to dst->flags
 */
static void keyscan_copy_config(const struct ec_mkbp_config *src,
				 struct ec_mkbp_config *dst,
				 uint32_t valid_mask, uint8_t valid_flags)
{
	if (valid_mask & EC_MKBP_VALID_SCAN_PERIOD)
		dst->scan_period_us = src->scan_period_us;
	if (valid_mask & EC_MKBP_VALID_POLL_TIMEOUT)
		dst->poll_timeout_us = src->poll_timeout_us;
	if (valid_mask & EC_MKBP_VALID_MIN_POST_SCAN_DELAY)
		dst->min_post_scan_delay_us = src->min_post_scan_delay_us;
	if (valid_mask & EC_MKBP_VALID_OUTPUT_SETTLE)
		dst->output_settle_us = src->output_settle_us;
	if (valid_mask & EC_MKBP_VALID_DEBOUNCE_DOWN)
		dst->debounce_down_us = src->debounce_down_us;
	if (valid_mask & EC_MKBP_VALID_DEBOUNCE_UP)
		dst->debounce_up_us = src->debounce_up_us;
	if (valid_mask & EC_MKBP_VALID_DISABLE_WAIT)
		dst->disable_wait_us = src->disable_wait_us;
	if (valid_mask & EC_MKBP_VALID_FIFO_MAX_DEPTH)
		dst->fifo_max_depth = src->fifo_max_depth;
	dst->flags &= ~valid_flags;
	dst->flags |= src->flags & valid_flags;
}

static int host_command_mkbp_set_config(struct host_cmd_handler_args *args)
{
	const struct ec_params_mkbp_set_config *req = args->params;

	keyscan_copy_config(&req->config, &config,
			    config.valid_mask & req->config.valid_mask,
			    config.valid_flags & req->config.valid_flags);

	/*
	 * Do some sanity checks that could cause bad things to
	 * happen.
	 */
	if (config.fifo_max_depth > KB_FIFO_DEPTH)
		config.fifo_max_depth = KB_FIFO_DEPTH;

	return EC_RES_SUCCESS;
}

static int host_command_mkbp_get_config(struct host_cmd_handler_args *args)
{
	struct ec_response_mkbp_get_config *resp = args->response;

	memcpy(&resp->config, &config, sizeof(config));
	args->response_size = sizeof(*resp);

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_MKBP_SET_CONFIG,
		     host_command_mkbp_set_config,
		     EC_VER_MASK(0));

DECLARE_HOST_COMMAND(EC_CMD_MKBP_GET_CONFIG,
		     host_command_mkbp_get_config,
		     EC_VER_MASK(0));
