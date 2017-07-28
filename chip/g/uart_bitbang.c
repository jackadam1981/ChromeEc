/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "pmu.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart_bitbang.h"
#include "uartn.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

#define BITBANG_DEBUG 0 /* Set to 1 to enable debug counters and logs. */

/* Support the "standard" baud rates. */
#define IS_BAUD_RATE_SUPPORTED(rate) \
	((rate == 1200) || (rate == 2400) || (rate == 4800) || (rate == 9600) \
	|| (rate == 19200) || (rate == 38400) || (rate == 57600) || \
	 (rate == 115200))

#define RX_BUF_SIZE 8
#define BUF_NEXT(idx) ((idx+1) % RX_BUF_SIZE)

#define TIMEUS_CLK_FREQ 24 /* units: MHz */

/* Flag indicating whether bit banging is enabled or not. */
static uint8_t bitbang_enabled;
static int rx_buf[RX_BUF_SIZE];

/* Current bitbang context */
static uint32_t bit_period_ticks;
static uint32_t set_baud_rate;
static uint8_t set_parity;

static volatile uint16_t *rx_gpio_reg;
static uint16_t rx_gpio_mask;

static struct {
	uint8_t head;
	uint8_t tail;
} htp;

#if BITBANG_DEBUG
/* debug counters and log */
#define DISCARD_LOG 8
static int read_char_cnt;
static int rx_buff_inserted_cnt;
static int rx_buff_rx_char_cnt;
static int stop_bit_err_cnt;
static int parity_err_cnt;
static int parity_err_discard[DISCARD_LOG];
static int parity_discard_idx;
static int stop_bit_discard[DISCARD_LOG];
static int stop_bit_discard_idx;
#endif /* BITBANG_DEBUG */

static int is_uart_allowed(int uart)
{
	return uart == bitbang_config.uart;
}

int uart_bitbang_is_enabled(int uart)
{
	return (is_uart_allowed(uart) && !!bitbang_enabled);
}

int uart_bitbang_enable(int uart, int baud_rate, int parity)
{
	const struct gpio_info *rx_gpio = gpio_list + bitbang_config.rx_gpio;

	/* We only want to bit bang 1 UART at a time. */
	if (bitbang_enabled)
		return EC_ERROR_BUSY;

	if (!is_uart_allowed(uart)) {
		CPRINTF("bit bang config not found for UART%d", uart);
		return EC_ERROR_INVAL;
	}

	/* Check desired properties. */
	if (!IS_BAUD_RATE_SUPPORTED(baud_rate)) {
		CPRINTF("Err: invalid baud rate (%d)", baud_rate);
		return EC_ERROR_INVAL;
	}
	set_baud_rate = baud_rate;

	switch (parity) {
	case 0:
	case 1:
	case 2:
		break;

	default:
		CPRINTF("Err: invalid parity '%d'. (0:N, 1:O, 2:E)", parity);
		return EC_ERROR_INVAL;
	};
	set_parity = parity;

	/* Select the GPIOs instead of the UART block. */
	uartn_tx_disconnect(bitbang_config.uart);
	REG32(bitbang_config.tx_pinmux_reg) =
		bitbang_config.tx_pinmux_regval;
	gpio_set_flags(bitbang_config.tx_gpio, GPIO_OUT_HIGH);
	REG32(bitbang_config.rx_pinmux_reg) =
		bitbang_config.rx_pinmux_regval;
	gpio_set_flags(bitbang_config.rx_gpio, GPIO_INPUT);

	rx_gpio_reg = &GR_GPIO_DATAIN(rx_gpio->port);
	rx_gpio_mask = rx_gpio->mask;

	/* Bump GPIO IRQ priority so that it can preempt other ISRs. */
	task_set_irq_priority(rx_gpio->port == 0 ? GC_IRQNUM_GPIO0_GPIOCOMBINT :
						   GC_IRQNUM_GPIO1_GPIOCOMBINT,
						   0);

	/*
	 * Ungate the microsecond timer so that we can use it.  This is needed
	 * for accurate framing if using faster baud rates.
	 */
	pmu_clock_en(PERIPH_TIMEUS);
	GR_TIMEUS_EN(0) = 0;
	GR_TIMEUS_MAXVAL(0) = 0xFFFFFFFF;
	GR_TIMEUS_EN(0) = 1;

	/* Save context information. */
	bit_period_ticks = TIMEUS_CLK_FREQ * (1 * SECOND) / set_baud_rate;

	/* Register the function pointers. */
	uartn_funcs[uart]._rx_available = _uart_bitbang_rx_available;
	uartn_funcs[uart]._write_char = _uart_bitbang_write_char;
	uartn_funcs[uart]._read_char = _uart_bitbang_read_char;

	/* Do not allow deep sleep, wake latency may cause missed Rx bytes. */
	disable_sleep(SLEEP_MASK_UART);

	bitbang_enabled = 1;
	gpio_enable_interrupt(bitbang_config.rx_gpio);
	ccprintf("successfully enabled\n");
	return EC_SUCCESS;
}

int uart_bitbang_disable(int uart)
{
	const struct gpio_info *rx_gpio = gpio_list + bitbang_config.rx_gpio;

	if (!uart_bitbang_is_enabled(uart))
		return EC_SUCCESS;

	/*
	 * This is safe because if the UART was not specified in the config, we
	 * would have already returned.
	 */
	bitbang_enabled = 0;
	gpio_reset(bitbang_config.tx_gpio);
	gpio_reset(bitbang_config.rx_gpio);

	/* Restore IRQ priority back to default. */
	task_set_irq_priority(rx_gpio->port == 0 ? GC_IRQNUM_GPIO0_GPIOCOMBINT :
						   GC_IRQNUM_GPIO1_GPIOCOMBINT,
						   GPIO_IRQ_PRIORITY);

	/* Unregister the function pointers. */
	uartn_funcs[uart]._rx_available = _uartn_rx_available;
	uartn_funcs[uart]._write_char = _uartn_write_char;
	uartn_funcs[uart]._read_char = _uartn_read_char;

	/* Gate the microsecond timer since we're done with it. */
	pmu_clock_dis(PERIPH_TIMEUS);

	/* Re-allow deep sleep. */
	enable_sleep(SLEEP_MASK_UART);

	/* Reconnect the GPIO to the UART block. */
	gpio_disable_interrupt(bitbang_config.rx_gpio);
	uartn_tx_connect(uart);
	return EC_SUCCESS;
}

/*
 * Wait for the timer reach 'ticks' after t0. To prevent overflow-related
 * bugs, t0 must represent a time in the past.
 */
static void wait_deadline(uint32_t t0, uint32_t ticks)
{
	while ((GR_TIMEUS_CUR_MAJOR(0) - t0) < ticks)
		;
}

void uart_bitbang_write_char(int uart, char c)
{
	int val;
	int ones = 0;
	int i;
	uint32_t t0;

	if (!uart_bitbang_is_enabled(uart))
		return;

	interrupt_disable();

	/* Start bit. */
	t0 = GR_TIMEUS_CUR_MAJOR(0);
	gpio_set_level(bitbang_config.tx_gpio, 0);

	/* 8 data bits. */
	for (i = 0; i < 8; i++) {
		val = (c & (1 << i));
		/* Count 1's in order to handle parity bit. */
		if (val)
			ones++;
		wait_deadline(t0, bit_period_ticks);
		gpio_set_level(bitbang_config.tx_gpio, val);
		t0 += bit_period_ticks;
	}

	/* Optional parity. */
	if (set_parity) {
		wait_deadline(t0, bit_period_ticks);
		gpio_set_level(bitbang_config.tx_gpio,
			       (ones + set_parity) & 0x1);
		t0 += bit_period_ticks;
	}

	/* 1 stop bit. */
	wait_deadline(t0, bit_period_ticks);
	gpio_set_level(bitbang_config.tx_gpio, 1);
	interrupt_enable();
}

/* Get logic level of Rx GPIO pin - a faster version of gpio_get_level(). */
static inline int uart_bitbang_get_rx_level(void)
{
	return *rx_gpio_reg & rx_gpio_mask;
}

int uart_bitbang_receive_char(int uart)
{
	uint8_t rx_char;
	int i;
	int rv;
	int ones;
	int parity_bit;
	int stop_bit;
	uint32_t t0 = GR_TIMEUS_CUR_MAJOR(0);

#if BITBANG_DEBUG
	rx_buff_rx_char_cnt++;
#endif /* BITBANG_DEBUG */

	/* Wait 1 bit period for the start bit. */
	wait_deadline(t0, bit_period_ticks - 48);
	t0 += (bit_period_ticks - 48);

	rv = EC_SUCCESS;
	rx_char = 0;
	ones = 0;

	/* 8 data bits. */
	for (i = 0; i < 8; i++) {
		if (uart_bitbang_get_rx_level()) {
			ones++;
			rx_char |= (1 << i);
		}
		wait_deadline(t0, bit_period_ticks);
		t0 += bit_period_ticks;
	}

	/* optional parity or stop bit. */
	parity_bit = uart_bitbang_get_rx_level();

	if (set_parity) {
		wait_deadline(t0, bit_period_ticks);
		stop_bit = uart_bitbang_get_rx_level();
		/* Verify parity */
		rv = (ones + parity_bit + set_parity) & 0x1 ?
				EC_ERROR_CRC : EC_SUCCESS;
	} else {
		/* If there's no parity, that _was_ the stop bit. */
		stop_bit = parity_bit;
	}

#if BITBANG_DEBUG
	if (rv != EC_SUCCESS) {
		parity_err_cnt++;
		parity_err_discard[parity_discard_idx] = rx_char;
		parity_discard_idx = (parity_discard_idx + 1) % DISCARD_LOG;
	}
#endif /* BITBANG_DEBUG */

	/* Check that the stop bit is valid. */
	if (!stop_bit) {
		rv = EC_ERROR_CRC;
#if BITBANG_DEBUG
		stop_bit_err_cnt++;
		stop_bit_discard[stop_bit_discard_idx] = rx_char;
		stop_bit_discard_idx = (stop_bit_discard_idx + 1) % DISCARD_LOG;
#endif /* BITBANG_DEBUG */
	}

	if (rv != EC_SUCCESS)
		return rv;

	/* Place the received char in the RX buffer. */
	if (BUF_NEXT(htp.tail) != htp.head) {
		rx_buf[htp.tail] = rx_char;
		htp.tail = BUF_NEXT(htp.tail);
#if BITBANG_DEBUG
		rx_buff_inserted_cnt++;
#endif /* BITBANG_DEBUG */
	}

	return EC_SUCCESS;
}

#ifdef CONFIG_GPIO_INTERRUPT_CUSTOM
int gpio_interrupt_custom(int gpio_port)
{
	const struct gpio_info *g = gpio_list + bitbang_config.rx_gpio;
	uint16_t pending;

	/* If the interrupt is not from our Rx pin bank, run normal ISR */
	if (gpio_port != g->port)
		return 0;

	/* If we don't have an Rx pin interrupt, run normal ISR */
	pending = GR_GPIO_CLRINTSTAT(g->port);
	if (!(pending & (g->mask)))
		return 0;

	do {
		/* Read UART byte from Rx pin and clear interrupt status */
		uart_bitbang_receive_char(UART_EC);
		GR_GPIO_CLRINTSTAT(g->port) = pending;
	/*
	 * If our Rx pin is low, we just got a new start bit, so read
	 * another byte.
	 */
	} while (!uart_bitbang_get_rx_level());

	return 1;
}
#endif

int uart_bitbang_read_char(int uart)
{
	int c;
	uint8_t head;

	if (!is_uart_allowed(uart))
		return 0;

	head = htp.head;
	c = rx_buf[head];

	if (head != htp.tail)
		htp.head = BUF_NEXT(head);

#if BITBANG_DEBUG
	read_char_cnt++;
#endif /* BITBANG_DEBUG */
	return c;
}

int uart_bitbang_is_char_available(int uart)
{
	if (!is_uart_allowed(uart))
		return 0;

	return htp.head != htp.tail;
}

#if BITBANG_DEBUG
static int write_test_pattern(int uart, int pattern_idx)
{
	if (!uart_bitbang_is_enabled(uart)) {
		ccprintf("bit banging mode not enabled for UART%d\n", uart);
		return EC_ERROR_INVAL;
	}

	switch (pattern_idx) {
	case 0:
		uartn_write_char(uart, 'a');
		uartn_write_char(uart, 'b');
		uartn_write_char(uart, 'c');
		uartn_write_char(uart, '\n');
		ccprintf("wrote: 'abc\\n'\n");
		break;

	case 1:
		uartn_write_char(uart, 0xAA);
		uartn_write_char(uart, 0xCC);
		uartn_write_char(uart, 0x55);
		ccprintf("wrote: '0xAA 0xCC 0x55'\n");
		break;

	default:
		ccprintf("unknown test pattern\n");
		return EC_ERROR_INVAL;
	};

	return EC_SUCCESS;
}
#endif /* BITBANG_DEBUG */

static int command_bitbang(int argc, char **argv)
{
	int uart;
	int baud_rate;
	int parity;

	if (argc > 1) {
		uart = atoi(argv[1]);
		if (argc == 3) {
			if (!strcasecmp("disable", argv[2]))
				return uart_bitbang_disable(uart);
			else
				return EC_ERROR_PARAM2;
		}

		if (argc == 4) {
#if BITBANG_DEBUG
			if (!strncasecmp("test", argv[2], 4))
				return write_test_pattern(uart, atoi(argv[3]));
#endif /* BITBANG_DEBUG */

			baud_rate = atoi(argv[2]);
			if (!strcasecmp("odd", argv[3]))
				parity = 1;
			else if (!strcasecmp("even", argv[3]))
				parity = 2;
			else if (!strcasecmp("none", argv[3]))
				parity = 0;
			else
				return EC_ERROR_PARAM3;

			return uart_bitbang_enable(uart, baud_rate, parity);
		}

		return EC_ERROR_PARAM_COUNT;
	}

	if (!uart_bitbang_is_enabled(bitbang_config.uart)) {
		ccprintf("bit banging mode disabled.\n");
	} else {
		ccprintf("baud rate - parity\n");
		ccprintf("  %6d    ", set_baud_rate);
		switch (set_parity) {
		case 1:
			ccprintf("odd\n");
			break;

		case 2:
			ccprintf("even\n");
			break;

		case 0:
		default:
			ccprintf("none\n");
		break;
		};
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(bitbang, command_bitbang,
			"<uart> <baud_rate> <odd,even,none> | <uart> disable "
#if BITBANG_DEBUG
			"| <uart> test <0, 1>"
#endif /* BITBANG_DEBUG */
			, "set bit bang mode");

#if BITBANG_DEBUG
static int command_bitbang_dump_stats(int argc, char **argv)
{
	int i;

	if (argc == 2) {
		/* Clear the counters. */
		if (!strncasecmp(argv[1], "clear", 5)) {
			parity_err_cnt = 0;
			stop_bit_err_cnt = 0;
			rx_buff_rx_char_cnt = 0;
			read_char_cnt = 0;
			rx_buff_inserted_cnt = 0;
			return EC_SUCCESS;
		}
		return EC_ERROR_PARAM1;
	}

	ccprintf("Errors:\n");
	ccprintf("%d Parity Errors\n", parity_err_cnt);
	ccprintf("%d Stop Bit Errors\n", stop_bit_err_cnt);
	ccprintf("Buffer info\n");
	ccprintf("%d received\n", rx_buff_rx_char_cnt);
	ccprintf("%d chars inserted\n", rx_buff_inserted_cnt);
	ccprintf("%d chars read\n", read_char_cnt);
	ccprintf("Contents\n");
	ccprintf("[");
	for (i = 0; i < RX_BUF_SIZE; i++)
		ccprintf(" %02x ", rx_buf[i] & 0xFF);
	ccprintf("]\n");
	ccprintf("head: %d\ntail: %d\n", htp.head, htp.tail);
	ccprintf("Discards\nparity: ");
	ccprintf("[");
	for (i = 0; i < DISCARD_LOG; i++)
		ccprintf(" %02x ", parity_err_discard[i] & 0xFF);
	ccprintf("]\n");
	ccprintf("idx: %d\n", parity_discard_idx);
	ccprintf("stop bit: ");
	ccprintf("[");
	for (i = 0; i < DISCARD_LOG; i++)
		ccprintf(" %02x ", stop_bit_discard[i] & 0xFF);
	ccprintf("]\n");
	ccprintf("idx: %d\n", stop_bit_discard_idx);
	cflush();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(bbstats, command_bitbang_dump_stats,
			"",
			"dumps bitbang stats");
#endif /* BITBANG_DEBUG */
