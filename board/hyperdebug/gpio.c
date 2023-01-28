/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug GPIO logic and console commands */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "shared_mem.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/*
 * A cyclic buffer is used to record events (edges) of one or more GPIO
 * signals.  Data is stored in the memory range immediately following this
 * struct.  Each event records the time since the previous event, and the
 * signal that changed (the direction of change is not explicitly recorded).
 */
struct cyclic_buffer_header_t {
	/* Time of the most recent event, updated from interrupt context. */
	volatile uint32_t head_time;
	/* Index at which new records are placed, updated from interrupt. */
	volatile uint32_t head;
	/* Index af oldest record. */
	uint32_t tail;
	/* Size of cyclic byte buffer, head and tail wrap to zero here. */
	uint32_t size;
	/* Time base that the oldest event is relative to. */
	timestamp_t tail_time;
	/* Sticky bit recording if overflow occurred. */
	volatile uint8_t overflow;
	/* Number of signals being monitored in this buffer. */
	uint8_t num_signals;
	/* The number of bits required to represent 0..num_signals-1. */
	uint8_t signal_bits;
	uint8_t fill;
	uint32_t fill2;
};

/*
 * The STM32L5 has 16 edge detection circuits.  Each pin can only be used with
 * one of them.  That is, detector 0 can take its input from one of pins A0,
 * B0, C0, ..., while detector 1 can choose between A1, B1, etc.
 *
 * Information about the current use of each detection circuit is stored in 16
 * "slots" below.
 */
struct monitoring_slot_t {
	/* Link to buffer recording edges of this signal. */
	struct cyclic_buffer_header_t *buffer;
	uint32_t gpio_base;
	uint32_t gpio_pin_mask;
	/* EC enum id of the signal used by this detection slot. */
	int gpio_signal;
	/* Most recently recorded level of the signal. */
	volatile uint32_t head_level;
	/* Level as of the current oldest end (tail) of the recording. */
	uint8_t tail_level;
	/* The index of the signal as used in the recording buffer. */
	uint8_t signal_no;
	uint8_t fill1, fill2;
	uint8_t code[512];
};
struct monitoring_slot_t monitoring_slots[16];

/*
 * Counts unacknowledged buffer overflows.  Whenever non-zero, the red LED
 * will flash.
 */
atomic_t num_cur_error_conditions;

/*
 * Counts the number of cyclic buffers currently in existence, the green LED
 * will flash whenever this is non-zero, indicating the monitoring activity.
 */
int num_cur_monitoring = 0;

__attribute__((noinline)) void overflow(struct monitoring_slot_t *slot)
{
	struct cyclic_buffer_header_t *buffer_header = slot->buffer;
	gpio_disable_interrupt(slot->gpio_signal);
	buffer_header->overflow = 1;
	atomic_add(&num_cur_error_conditions, 1);
}

void gpio_edge(enum gpio_signal signal)
{
}

#define GPIO_IRQ_HIGHEST_PRIORITY(no) \
  const struct irq_priority __keep IRQ_PRIORITY(STM32_IRQ_EXTI ## no)			\
		__attribute__((section(".rodata.irqprio"))) = { STM32_IRQ_EXTI ## no, \
								0 }

GPIO_IRQ_HIGHEST_PRIORITY(0);
GPIO_IRQ_HIGHEST_PRIORITY(1);
GPIO_IRQ_HIGHEST_PRIORITY(2);
GPIO_IRQ_HIGHEST_PRIORITY(3);
GPIO_IRQ_HIGHEST_PRIORITY(4);
GPIO_IRQ_HIGHEST_PRIORITY(5);
GPIO_IRQ_HIGHEST_PRIORITY(6);
GPIO_IRQ_HIGHEST_PRIORITY(7);
GPIO_IRQ_HIGHEST_PRIORITY(8);
GPIO_IRQ_HIGHEST_PRIORITY(9);
GPIO_IRQ_HIGHEST_PRIORITY(10);
GPIO_IRQ_HIGHEST_PRIORITY(11);
GPIO_IRQ_HIGHEST_PRIORITY(12);
GPIO_IRQ_HIGHEST_PRIORITY(13);
GPIO_IRQ_HIGHEST_PRIORITY(14);
GPIO_IRQ_HIGHEST_PRIORITY(15);

__attribute((section(".bss.vector_table")))
void (*sram_vtable[125])(void);
extern void (*vectors[125])(void);

#define CORTEX_VTABLE REG32(0xE000ED08)

void edge_int(void);

static void board_gpio_init(void)
{
	/* Mark every slot as unused. */
	for (int i = 0; i < ARRAY_SIZE(monitoring_slots); i++)
		monitoring_slots[i].gpio_signal = GPIO_COUNT;

	memcpy(sram_vtable, vectors, sizeof(sram_vtable));
	for (int i = 0; i < 16; i++) {
		memcpy(monitoring_slots[i].code, (void *)(~1U & (size_t)&edge_int), 512);
		ccprintf("Slot %d code: %p\n", i, &monitoring_slots[i].code);
		sram_vtable[16 + STM32_IRQ_EXTI0 + i] = (void (*)(void))(1U | (size_t)&monitoring_slots[i].code);
	}
	CORTEX_VTABLE = (uint32_t)(sram_vtable);
}
DECLARE_HOOK(HOOK_INIT, board_gpio_init, HOOK_PRIO_DEFAULT);

static void stop_all_gpio_monitoring(void)
{
	struct monitoring_slot_t *slot;
	struct cyclic_buffer_header_t *buffer_header;
	for (int i = 0; i < ARRAY_SIZE(monitoring_slots); i++) {
		slot = monitoring_slots + i;
		if (!slot->buffer)
			continue;

		/* Disable interrupts for all signals feeding into the same
		 * cyclic buffer. */
		buffer_header = slot->buffer;
		for (int j = i; j < ARRAY_SIZE(monitoring_slots); j++) {
			slot = monitoring_slots + i;
			if (slot->buffer != buffer_header)
				continue;
			gpio_disable_interrupt(slot->gpio_signal);
			slot->gpio_signal = GPIO_COUNT;
		}
		/* Deallocate this one cyclic buffer. */
		num_cur_monitoring--;
		if (buffer_header->overflow)
			atomic_sub(&num_cur_error_conditions, 1);
		shared_mem_release((char *)buffer_header);
	}
}

/**
 * Find a GPIO signal by name.
 *
 * This is copied from gpio.c unfortunately, as it is static over there.
 *
 * @param name		Signal name to find
 *
 * @return the signal index, or GPIO_COUNT if no match.
 */
static enum gpio_signal find_signal_by_name(const char *name)
{
	int i;

	if (!name || !*name)
		return GPIO_COUNT;

	for (i = 0; i < GPIO_COUNT; i++)
		if (gpio_is_implemented(i) &&
		    !strcasecmp(name, gpio_get_name(i)))
			return i;

	return GPIO_COUNT;
}

/*
 * Set the mode of a GPIO pin: input/opendrain/pushpull/alternate.
 */
static int command_gpio_mode(int argc, const char **argv)
{
	int gpio;
	int flags;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[1]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	flags = gpio_get_flags(gpio);

	flags = flags & ~(GPIO_INPUT | GPIO_OUTPUT | GPIO_OPEN_DRAIN);
	if (strcasecmp(argv[2], "input") == 0)
		flags |= GPIO_INPUT;
	else if (strcasecmp(argv[2], "opendrain") == 0)
		flags |= GPIO_OUTPUT | GPIO_OPEN_DRAIN;
	else if (strcasecmp(argv[2], "pushpull") == 0)
		flags |= GPIO_OUTPUT;
	else if (strcasecmp(argv[2], "alternate") == 0)
		flags |= GPIO_ALTERNATE;
	else
		return EC_ERROR_PARAM2;

	/* Update GPIO flags. */
	gpio_set_flags(gpio, flags);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpiomode, command_gpio_mode,
			      "name <input | opendrain | pushpull | alternate>",
			      "Set a GPIO mode", CMD_FLAG_RESTRICTED);

/*
 * Set the weak pulling of a GPIO pin: up/down/none.
 */
static int command_gpio_pull_mode(int argc, const char **argv)
{
	int gpio;
	int flags;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[1]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	flags = gpio_get_flags(gpio);

	flags = flags & ~(GPIO_PULL_UP | GPIO_PULL_DOWN);
	if (strcasecmp(argv[2], "none") == 0)
		;
	else if (strcasecmp(argv[2], "up") == 0)
		flags |= GPIO_PULL_UP;
	else if (strcasecmp(argv[2], "down") == 0)
		flags |= GPIO_PULL_DOWN;
	else
		return EC_ERROR_PARAM2;

	/* Update GPIO flags. */
	gpio_set_flags(gpio, flags);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpiopullmode, command_gpio_pull_mode,
			      "name <none | up | down>",
			      "Set a GPIO weak pull mode", CMD_FLAG_RESTRICTED);

static int command_gpio_monitoring_start(int argc, const char **argv)
{
	int gpios[16];
	int gpio_num = argc - 3;
	int i;
	timestamp_t now;
	int rv;
	size_t cyclic_buffer_size = 8192; /* Maybe configuration by parameter */
	struct cyclic_buffer_header_t *buf;
	struct monitoring_slot_t *slot;

	if (gpio_num <= 0 || gpio_num > 16)
		return EC_ERROR_PARAM_COUNT;

	for (i = 0; i < gpio_num; i++) {
		gpios[i] = find_signal_by_name(argv[3 + i]);
		if (gpios[i] == GPIO_COUNT)
			return EC_ERROR_PARAM3 + i; /* May overflow */
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		if (slot->gpio_signal != GPIO_COUNT) {
			ccprintf("Error: Monitoring of %s conflicts with %s\n",
				 argv[3 + i],
				 gpio_list[slot->gpio_signal].name);
			rv = EC_ERROR_PARAM3 + i;
			goto out_partial_cleanup;
		}
		slot->gpio_signal = gpios[i];
	}

	/*
	 * All the requested signals were available for monitoring, and their
	 * slots have been marked as reserved for the respective signal.
	 */
	rv = shared_mem_acquire(sizeof(struct cyclic_buffer_header_t) + cyclic_buffer_size,
				(char **)&buf);
	if (rv != EC_SUCCESS)
		goto out_cleanup;

	buf->head = buf->tail = 0;
	buf->size = cyclic_buffer_size;
	buf->overflow = 0;
	buf->num_signals = gpio_num;
	buf->signal_bits = 0;
	/* Compute how many bits are required to represent 0..gpio_num-1. */
	while ((gpio_num - 1) >> buf->signal_bits)
		buf->signal_bits++;

	for (i = 0; i < gpio_num; i++) {
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		slot->gpio_base = gpio_list[gpios[i]].port;
		slot->gpio_pin_mask = gpio_list[gpios[i]].mask;
		slot->buffer = buf;
		slot->signal_no = i;
	}

	/* Time critical block */
	interrupt_disable();
	now = get_time();
	buf->head_time = now.le.lo;
	for (i = 0; i < gpio_num; i++) {
		int gpio_num = GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		slot = monitoring_slots + gpio_num;
		gpio_enable_interrupt(gpios[i]);
		slot->tail_level = slot->head_level = gpio_get_level(gpios[i]);
		/*
		 * Race condition here!  If three or more edges happen in
		 * rapid succession, we may fail to record some of them, but
		 * we should never over-report edges.
		 *
		 * Since edge interrupts were enabled before the "head_level"
		 * was polled, if an edge happened between the two, then an
		 * interrupt is currently pending, and when handled after this
		 * loop, the logic in the gpio_edge interrupt handler would
		 * wrongly conclude that the signal must have seen two
		 * transitions, in order to end up at the same level as before.
		 * In order to avoid such over-reporting, we clear "pending"
		 * interupt bit below, but only for the direction that goes
		 * "towards" the level measured above.
		 */
		if (slot->head_level)
			STM32_EXTI_RPR = BIT(gpio_num);
		else
			STM32_EXTI_FPR = BIT(gpio_num);
	}
	interrupt_enable();

	buf->tail_time = now;
	num_cur_monitoring++;
	ccprintf("  @%lld\n", buf->tail_time.val);

	/*
	 * Dump the initial level of each input, for the convenience of the
	 * caller.  (Allow makes monitoring useful, even if a signal has no
	 * transitions during the monitoring period.
	 */
	for (i = 0; i < gpio_num; i++) {
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		ccprintf("  %d %s %d\n", i, gpio_list[gpios[i]].name,
			 slot->tail_level);
	}

	return EC_SUCCESS;

out_cleanup:
	i = gpio_num;
out_partial_cleanup:
	while (i-- > 0) {
		monitoring_slots[GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask)]
			.gpio_signal = GPIO_COUNT;
	}
	return rv;
}

static int command_gpio_monitoring_read(int argc, const char **argv)
{
	int gpios[16];
	int gpio_num = argc - 3;
	int i;
	struct cyclic_buffer_header_t *buf = NULL;
	struct monitoring_slot_t *slot;
	int gpio_signals_by_no[16];
	uint8_t signal_bits;
	uint32_t tail, head;
	timestamp_t tail_time, now;

	if (gpio_num <= 0 || gpio_num > 16)
		return EC_ERROR_PARAM_COUNT;

	for (i = 0; i < gpio_num; i++) {
		gpios[i] = find_signal_by_name(argv[3 + i]);
		if (gpios[i] == GPIO_COUNT)
			return EC_ERROR_PARAM3 + i; /* May overflow */
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		if (slot->gpio_signal != gpios[i]) {
			ccprintf("Error: Not monitoring %s\n",
				 gpio_list[gpios[i]].name);
			return EC_ERROR_PARAM3 + i;
		}
		if (buf == NULL) {
			buf = slot->buffer;
		} else if (buf != slot->buffer) {
			ccprintf(
				"Error: Not monitoring %s as part of same groups as %s\n",
				gpio_list[gpios[i]].name,
				gpio_list[gpios[0]].name);
			return EC_ERROR_PARAM3 + i;
		}
		gpio_signals_by_no[slot->signal_no] = gpios[i];
	}
	if (gpio_num != buf->num_signals) {
		ccprintf("Error: Not full set of signals monitored\n");
		return EC_ERROR_INVAL;
	}

	/*
	 * Time critical part, we read current time, before taking a snapshot of
	 * the head pointer as set by the interrupt handler.  This way, we can
	 * guarantee that the transcript will include any edge happening at or
	 * before the `now` timestamp.
	 */
	now = get_time();
	head = buf->head;

	ccprintf("  @%lld\n", now.val);
	signal_bits = buf->signal_bits;
	tail = buf->tail;
	tail_time = buf->tail_time;
	while (tail != head) {
		uint8_t *buffer = (uint8_t *)(buf + 1);
		timestamp_t diff;
		uint8_t byte;
		uint8_t signal_no;
		int shift = 0;
		struct monitoring_slot_t *slot;
		diff.val = 0;
		do {
			byte = buffer[tail++];
			if (tail == buf->size)
				tail = 0;
			diff.val |= (byte & 0x7F) << shift;
			shift += 7;
		} while (byte & 0x80);
		signal_no = diff.val & (0xFF >> (8 - signal_bits));
		diff.val >>= signal_bits;
		tail_time.val += diff.val;
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(
			       gpio_list[gpio_signals_by_no[signal_no]].mask);
		/* To conserve bandwidth, timestamps are relative to `now`. */
		ccprintf("  %d %lld %s\n", signal_no, tail_time.val - now.val,
			 slot->tail_level ? "F" : "R");
		slot->tail_level = !slot->tail_level;
	}
	buf->tail = tail;
	buf->tail_time = tail_time;
	if (buf->overflow) {
		ccprintf("Error: Buffer overflow\n");
	}

	return EC_SUCCESS;
}

static int command_gpio_monitoring_stop(int argc, const char **argv)
{
	int gpios[16];
	int gpio_num = argc - 3;
	int i;
	struct cyclic_buffer_header_t *buf = NULL;
	struct monitoring_slot_t *slot;

	if (gpio_num <= 0 || gpio_num > 16)
		return EC_ERROR_PARAM_COUNT;

	for (i = 0; i < gpio_num; i++) {
		gpios[i] = find_signal_by_name(argv[3 + i]);
		if (gpios[i] == GPIO_COUNT)
			return EC_ERROR_PARAM3 + i; /* May overflow */
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		if (slot->gpio_signal != gpios[i]) {
			ccprintf("Error: Not monitoring %s\n",
				 gpio_list[gpios[i]].name);
			return EC_ERROR_PARAM3 + i;
		}
		if (buf == NULL) {
			buf = slot->buffer;
		} else if (buf != slot->buffer) {
			ccprintf(
				"Error: Not monitoring %s as part of same groups as %s\n",
				gpio_list[gpios[i]].name,
				gpio_list[gpios[0]].name);
			return EC_ERROR_PARAM3 + i;
		}
	}
	if (gpio_num != buf->num_signals) {
		ccprintf("Error: Not full set of signals monitored\n");
		return EC_ERROR_INVAL;
	}

	for (i = 0; i < gpio_num; i++) {
		gpio_disable_interrupt(gpios[i]);
	}

	/*
	 * With no more interrupts modifying the buffer, it can be deallocated.
	 */
	num_cur_monitoring--;
	for (i = 0; i < gpio_num; i++) {
		slot = monitoring_slots +
		       GPIO_MASK_TO_NUM(gpio_list[gpios[i]].mask);
		slot->gpio_signal = GPIO_COUNT;
		slot->buffer = NULL;
	}

	if (buf->overflow)
		atomic_sub(&num_cur_error_conditions, 1);

	shared_mem_release((char *)buf);
	return EC_SUCCESS;
}

static int command_gpio_monitoring(int argc, const char **argv)
{
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[2], "start"))
		return command_gpio_monitoring_start(argc, argv);
	if (!strcasecmp(argv[2], "read"))
		return command_gpio_monitoring_read(argc, argv);
	if (!strcasecmp(argv[2], "stop"))
		return command_gpio_monitoring_stop(argc, argv);
	return EC_ERROR_PARAM2;
}

static int command_gpio(int argc, const char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[1], "monitoring"))
		return command_gpio_monitoring(argc, argv);
	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpio, command_gpio,
			      "monitoring start PIN"
			      "\nmonitoring read PIN"
			      "\nmonitoring stop PIN",
			      "GPIO manipulation", CMD_FLAG_RESTRICTED);

static int command_reinit(int argc, const char **argv)
{
	stop_all_gpio_monitoring();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(reinit, command_reinit, "",
			      "Stop any ongoing operation",
			      CMD_FLAG_RESTRICTED);

static void led_tick(void)
{
	/* Indicate ongoing GPIO monitoring by flashing the green LED. */
	if (num_cur_monitoring)
		gpio_set_level(GPIO_NUCLEO_LED1,
			       !gpio_get_level(GPIO_NUCLEO_LED1));
	else
		gpio_set_level(GPIO_NUCLEO_LED1, 1);
	/* Indicate error conditions by flashing red LED. */
	if (atomic_add(&num_cur_error_conditions, 0))
		gpio_set_level(GPIO_NUCLEO_LED3,
			       !gpio_get_level(GPIO_NUCLEO_LED3));
	else
		gpio_set_level(GPIO_NUCLEO_LED3, 0);
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);
