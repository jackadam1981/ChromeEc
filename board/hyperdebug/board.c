/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug board configuration */

#include "adc.h"
#include "clock_chip.h"
#include "common.h"
#include "dfu_bootmanager_shared.h"
#include "ec_version.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "stm32-dma.h"
#include "timer.h"
#include "usart-stm32l5.h"
#include "usb-stream.h"
#include "usb_spi.h"

#include <stdio.h>

/* Must come after other header files and interrupt handler declarations */
#include "gpio_list.h"

void board_config_pre_init(void)
{
	/* We know VDDIO2 is present, enable the GPIO circuit. */
	STM32_PWR_CR2 |= STM32_PWR_CR2_IOSV;

	/* Peripheral clock derived by dividing core clock by 4. */
	STM32_RCC_CFGR = STM32_RCC_CFGR_PPRE1_DIV4 | STM32_RCC_CFGR_PPRE2_DIV4;
}

/******************************************************************************
 * Forward UARTs as a USB serial interface.
 */

#define USB_STREAM_RX_SIZE 64
#define USB_STREAM_TX_SIZE 64

/******************************************************************************
 * Forward USART2 as a simple USB serial interface.
 */

static struct usart_config const usart2;
struct usb_stream_config const usart2_usb;

static struct queue const usart2_to_usb =
	QUEUE_DIRECT(1024, uint8_t, usart2.producer, usart2_usb.consumer);
static struct queue const usb_to_usart2 =
	QUEUE_DIRECT(64, uint8_t, usart2_usb.producer, usart2.consumer);

static struct usart_config const usart2 =
	USART_CONFIG(usart2_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart2_to_usb, usb_to_usart2);

USB_STREAM_CONFIG_USART_IFACE(usart2_usb, USB_IFACE_USART2_STREAM,
			      USB_STR_USART2_STREAM_NAME, USB_EP_USART2_STREAM,
			      USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE,
			      usb_to_usart2, usart2_to_usb, usart2)

/******************************************************************************
 * Forward USART3 as a simple USB serial interface.
 */

static struct usart_config const usart3;
struct usb_stream_config const usart3_usb;

static struct queue const usart3_to_usb =
	QUEUE_DIRECT(1024, uint8_t, usart3.producer, usart3_usb.consumer);
static struct queue const usb_to_usart3 =
	QUEUE_DIRECT(64, uint8_t, usart3_usb.producer, usart3.consumer);

static struct usart_config const usart3 =
	USART_CONFIG(usart3_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart3_to_usb, usb_to_usart3);

USB_STREAM_CONFIG_USART_IFACE(usart3_usb, USB_IFACE_USART3_STREAM,
			      USB_STR_USART3_STREAM_NAME, USB_EP_USART3_STREAM,
			      USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE,
			      usb_to_usart3, usart3_to_usb, usart3)

/******************************************************************************
 * Forward USART4 as a simple USB serial interface.
 */

static struct usart_config const usart4;
struct usb_stream_config const usart4_usb;

static struct queue const usart4_to_usb =
	QUEUE_DIRECT(1024, uint8_t, usart4.producer, usart4_usb.consumer);
static struct queue const usb_to_usart4 =
	QUEUE_DIRECT(64, uint8_t, usart4_usb.producer, usart4.consumer);

static struct usart_config const usart4 =
	USART_CONFIG(usart4_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart4_to_usb, usb_to_usart4);

USB_STREAM_CONFIG_USART_IFACE(usart4_usb, USB_IFACE_USART4_STREAM,
			      USB_STR_USART4_STREAM_NAME, USB_EP_USART4_STREAM,
			      USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE,
			      usb_to_usart4, usart4_to_usb, usart4)

/******************************************************************************
 * Forward USART5 as a simple USB serial interface.
 */

static struct usart_config const usart5;
struct usb_stream_config const usart5_usb;

static struct queue const usart5_to_usb =
	QUEUE_DIRECT(1024, uint8_t, usart5.producer, usart5_usb.consumer);
static struct queue const usb_to_usart5 =
	QUEUE_DIRECT(64, uint8_t, usart5_usb.producer, usart5.consumer);

static struct usart_config const usart5 =
	USART_CONFIG(usart5_hw, usart_rx_interrupt, usart_tx_interrupt, 115200,
		     0, usart5_to_usb, usb_to_usart5);

USB_STREAM_CONFIG_USART_IFACE(usart5_usb, USB_IFACE_USART5_STREAM,
			      USB_STR_USART5_STREAM_NAME, USB_EP_USART5_STREAM,
			      USB_STREAM_RX_SIZE, USB_STREAM_TX_SIZE,
			      usb_to_usart5, usart5_to_usb, usart5)

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google LLC"),
	[USB_STR_PRODUCT] = USB_STRING_DESC("HyperDebug CMSIS-DAP"),
	[USB_STR_SERIALNO] = 0,
	[USB_STR_VERSION] = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("HyperDebug Shell"),
	[USB_STR_SPI_NAME] = USB_STRING_DESC("SPI"),
	[USB_STR_CMSIS_DAP_NAME] = USB_STRING_DESC("I2C CMSIS-DAP"),
	[USB_STR_USART2_STREAM_NAME] = USB_STRING_DESC("UART2"),
	[USB_STR_USART3_STREAM_NAME] = USB_STRING_DESC("UART3"),
	[USB_STR_USART4_STREAM_NAME] = USB_STRING_DESC("UART4"),
	[USB_STR_USART5_STREAM_NAME] = USB_STRING_DESC("UART5"),
	[USB_STR_DFU_NAME] = USB_STRING_DESC("DFU"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * Set up ADC
 */

/* ADC channels */
struct adc_t adc_channels[] = {
	/*
	 * All available ADC signals, converted to mV (3300mV/4096).  Every one
	 * is declared with same name as the GPIO signal on the same pin, that
	 * is how opentitantool identifies the signal.
	 *
	 * Technically, the Nucleo-L552ZE-Q board can run at either 1v8 or 3v3
	 * supply, but we use HyperDebug only on 3v3 setting.  If in the future
	 * we want to detect actual voltage, Vrefint could be used.  This would
	 * also serve as calibration as the supply voltage may not be 3300mV
	 * exactly.
	 */
	[ADC_VREFINT] = { "VREFINT", 1, 1, 0, STM32_AIN(0) },
	[ADC_CN9_11] = { "CN9_11", 3300, 4096, 0, STM32_AIN(1) },
	[ADC_CN9_9] = { "CN9_9", 3300, 4096, 0, STM32_AIN(2) },
	/*[ADC_CN10_9] = { "CN10_9", 3300, 4096, 0, STM32_AIN(3) },*/
	[ADC_CN9_5] = { "CN9_5", 3300, 4096, 0, STM32_AIN(4) },
	[ADC_CN10_29] = { "CN10_29", 3300, 4096, 0, STM32_AIN(5) },
	[ADC_CN10_11] = { "CN10_11", 3300, 4096, 0, STM32_AIN(6) },
	[ADC_CN9_3] = { "CN9_3", 3300, 4096, 0, STM32_AIN(7) },
	[ADC_CN9_1] = { "CN9_1", 3300, 4096, 0, STM32_AIN(8) },
	[ADC_CN7_9] = { "CN7_9", 3300, 4096, 0, STM32_AIN(9) },
	[ADC_CN7_10] = { "CN7_10", 3300, 4096, 0, STM32_AIN(10) },
	[ADC_CN7_12] = { "CN7_12", 3300, 4096, 0, STM32_AIN(11) },
	[ADC_CN7_14] = { "CN7_14", 3300, 4096, 0, STM32_AIN(12) },
	[ADC_CN9_7] = { "CN9_7", 3300, 4096, 0, STM32_AIN(15) },
	[ADC_CN10_7] = { "CN10_7", 3300, 4096, 0, STM32_AIN(16) },
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************
 * Allow changing of system and peripheral clock frequency at runtime.
 *
 * Changing clock frequency may disrupt speed settings of SPI ports or PWMs
 * already set up, so one should preferably choose the clock speed before
 * setting up anything else.
 */

/* Default divisors, resulting in maximum 110MHz system clock. */
int stm32_pllm = 4;
int stm32_plln = 55;
int stm32_pllr = 2;

/* Frequency of external oscillator in Hz.  0 if not present. */
int external_clock_frequency = 0;

/*
 * Determines if clock signal is present on CN11_29, either through external
 * crystal between CN11_29 and CN11_31, or direct external feed.  If so, measure
 * the frequency, which must be a multiple of 4MHz, and use it as reference for
 * all internal clocks.  Having an external crystal greatly improves the
 * HyperDebug clock jitter and precision of timing measurements or bit-banging.
 */
static void probe_external_clock(void)
{
	STM32_RCC_CR |= STM32_RCC_CR_HSEON;

	/* Wait up to 10ms for external oscillator to stabilize. */
	timestamp_t deadline;
	deadline.val = get_time().val + 10000;
	while (!(STM32_RCC_CR & STM32_RCC_CR_HSERDY)) {
		timestamp_t now = get_time();
		if (timestamp_expired(deadline, &now)) {
			cprints(CC_SYSTEM, "External clock: not detected");
			STM32_RCC_CR &= ~STM32_RCC_CR_HSEON &
					~STM32_RCC_CR_HSEBYP;
			return;
		}
	}

	/*
	 * Measure the external frequency, using timer 17, which can be
	 * configured to trigger a capture event on every 32nd clock pulse of
	 * the external clock.
	 */

	__hw_timer_enable_clock(HSE_TIMER, 1);
	STM32_TIM_CR1(HSE_TIMER) = 0;

	/* Full 16-bit counter range. */
	STM32_TIM_ARR(HSE_TIMER) = 0xFFFF;

	/* No prescaler. */
	STM32_TIM_PSC(HSE_TIMER) = 0;

	/* Select HSE/32 as TI1 input. */
	STM32_TIM_OR(HSE_TIMER) = 0x02;

	/*
	 * Enable input capture on every 8th trigger edge, that is every 256th
	 * external clock pulse.
	 */
	STM32_TIM_CCER(HSE_TIMER) = 0x00;
	STM32_TIM_CCMR1(HSE_TIMER) = 0x0D;
	STM32_TIM_CCER(HSE_TIMER) = 0x01;

	/* No interrupts. */
	STM32_TIM_DIER(HSE_TIMER) = 0;

	/* Enable timer. */
	STM32_TIM_CR2(HSE_TIMER) = 0;
	STM32_TIM_CR1(HSE_TIMER) = STM32_TIM_CR1_CEN;

	/* Clear any latched status bits. */
	STM32_TIM_SR(HSE_TIMER) = 0;

	/* Wait for first capture event */
	uint32_t status;
	do {
		status = STM32_TIM_SR(HSE_TIMER);
	} while (!(status & 0x0002));

	if (status & 0x0200) {
		cprints(CC_SYSTEM, "Problem detecting external clock");
		STM32_RCC_CR &= ~STM32_RCC_CR_HSEON & ~STM32_RCC_CR_HSEBYP;
		return;
	}

	uint16_t first_capture = STM32_TIM_CCR1(HSE_TIMER);
	uint16_t last_capture = first_capture;

	/*
	 * Now wait for further capture events, until at least 5000 cycles have
	 * passed, which should give us plenty of precision for calculating the
	 * frequency of the external clock.
	 */
	int intervals = 0;
	do {
		uint32_t status;
		do {
			status = STM32_TIM_SR(HSE_TIMER);
		} while (!(status & 0x0002));

		if (status & 0x0200) {
			cprints(CC_SYSTEM, "Problem detecting external clock");
			STM32_RCC_CR &= ~STM32_RCC_CR_HSEON &
					~STM32_RCC_CR_HSEBYP;
			return;
		}

		uint16_t capture = STM32_TIM_CCR1(HSE_TIMER);

		intervals++;
		last_capture = capture;
	} while (last_capture - first_capture < 5000);

	STM32_TIM_CR1(HSE_TIMER) = 0;

	int hse_freq = DIV_ROUND_NEAREST(clock_get_timer_freq(),
					 last_capture - first_capture) *
		       (intervals * 8 * 32);

	int hse_freq_mhz = DIV_ROUND_NEAREST(hse_freq, 4000000) * 4;

	if (!hse_freq_mhz) {
		cprints(CC_SYSTEM,
			"External clock measured at %d Hz, not within 2%% of a multiple of 4MHz",
			hse_freq);
		STM32_RCC_CR &= ~STM32_RCC_CR_HSEON & ~STM32_RCC_CR_HSEBYP;
		return;
	}

	int deviation_ppm = DIV_ROUND_NEAREST(hse_freq - hse_freq_mhz * 1000000,
					      hse_freq_mhz);

	if (deviation_ppm > 20000 || deviation_ppm < -20000) {
		cprints(CC_SYSTEM,
			"External clock measured at %d Hz, not within 2%% of a multiple of 4MHz",
			hse_freq);
		STM32_RCC_CR &= ~STM32_RCC_CR_HSEON & ~STM32_RCC_CR_HSEBYP;
		return;
	}

	cprints(CC_SYSTEM, "External clock: %dMHz", hse_freq_mhz);
	external_clock_frequency = hse_freq_mhz * 1000000;
}

/* Change system/core clock frequency and peripheral clock frequency. */
static void change_frequencies(int n, int r, uint32_t rcc_cfgr_prescaler)
{
	/*
	 * There are a few concerns: We must not use the PLL as source of
	 * system clock, while modifying its parameters.  Also, the peripheral
	 * clock must never drop below 10MHz, or the USB controller might not
	 * be able to keep up with the bus.
	 */

	/* Peripheral clock equal to system core clock (no division). */
	STM32_RCC_CFGR = (STM32_RCC_CFGR & ~STM32_RCC_CFGR_PPRE1_MSK &
			  ~STM32_RCC_CFGR_PPRE2_MSK) |
			 STM32_RCC_CFGR_PPRE1_DIV1 | STM32_RCC_CFGR_PPRE2_DIV1;

	/* Temporarily use 16MHz clock source, rather than PLL. */
	clock_set_osc(OSC_HSI, OSC_INIT);

	/*
	 * Now we are free to modify PLL parameters.
	 */

	/*
	 * PLL input must stay within 4MHz - 16MHz, we always use 4MHz, which is
	 * obtained either by dividing HSI (16MHz) by 4, or by dividing the
	 * external clock by some value.
	 */
	enum clock_osc pll_osc;
	if (!external_clock_frequency) {
		pll_osc = OSC_HSI;
		stm32_pllm = 4;
	} else {
		pll_osc = OSC_HSE;
		stm32_pllm = external_clock_frequency / 4000000;
	}

	/* Above frequency * N must stay within 64MHz - 344MHz. */
	stm32_plln = n;

	/* Above frequency / R must not exceed 110MHz. */
	stm32_pllr = r;

	/*
	 * Switch to PLL clock source, using newly updated parameters, waiting
	 * for the new source to stabilize.
	 */
	clock_set_osc(OSC_PLL, pll_osc);

	/* Now apply the desired divisor to peripheral clock. */
	hook_notify(HOOK_PRE_FREQ_CHANGE);
	STM32_RCC_CFGR = (STM32_RCC_CFGR & ~STM32_RCC_CFGR_PPRE1_MSK &
			  ~STM32_RCC_CFGR_PPRE2_MSK) |
			 rcc_cfgr_prescaler;
	hook_notify(HOOK_FREQ_CHANGE);
}

static int command_clock_set(int argc, const char **argv)
{
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	char *e;
	int req_freq = strtoi(argv[1], &e, 0);
	int plln, pllr;
	if (*e)
		return EC_ERROR_PARAM1;

	/*
	 * We restrict ourselves to a PLL input frequency of 4MHz, which is
	 * then multiplied by a value N in the range 16 though 86 and divided
	 * by R: 2, 4, or 8.  This allows producing any frequency between
	 * 10MHz and 110MHz with no more than +/- 1.5% deviation.
	 */
	if (req_freq > 110000000 || req_freq < 10000000) {
		ccprintf("Error: Clock frequency out of range\n");
		return EC_ERROR_PARAM1;
	}
	if (req_freq >= 32000000 && req_freq % 2000000 == 0) {
		plln = req_freq / 2000000;
		pllr = 2;
	} else if (req_freq >= 16000000 && req_freq % 1000000 == 0) {
		plln = req_freq / 1000000;
		pllr = 4;
	} else if (req_freq >= 8000000 && req_freq % 500000 == 0) {
		plln = req_freq / 500000;
		pllr = 8;
	} else {
		ccprintf("Error: Clock frequency not supported\n");
		return EC_ERROR_PARAM1;
	}

	if (plln > 86) {
		ccprintf("Error: Clock frequency not supported\n");
	}

	int peripheral_clock_div = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;
	if (req_freq / peripheral_clock_div < 10000000) {
		/*
		 * The STM32L5 USB peripheral requires an APB1 clock frequency
		 * of at least 10MHz for correct operation.
		 */
		ccprintf(
			"Error: Peripheral frequency must be at least 10000000,"
			" reduce the divisor\n");
		return EC_ERROR_PARAM2;
	}

	uint32_t rcc_cfgr;
	switch (peripheral_clock_div) {
	case 1:
		rcc_cfgr = STM32_RCC_CFGR_PPRE1_DIV1 |
			   STM32_RCC_CFGR_PPRE2_DIV1;
		break;
	case 2:
		rcc_cfgr = STM32_RCC_CFGR_PPRE1_DIV2 |
			   STM32_RCC_CFGR_PPRE2_DIV2;
		break;
	case 4:
		rcc_cfgr = STM32_RCC_CFGR_PPRE1_DIV4 |
			   STM32_RCC_CFGR_PPRE2_DIV4;
		break;
	case 8:
		rcc_cfgr = STM32_RCC_CFGR_PPRE1_DIV8 |
			   STM32_RCC_CFGR_PPRE2_DIV8;
		break;
	case 16:
		rcc_cfgr = STM32_RCC_CFGR_PPRE1_DIV16 |
			   STM32_RCC_CFGR_PPRE2_DIV16;
		break;
	default:
		ccprintf("Error: Divisor must be power of two, at most 16\n");
		return EC_ERROR_PARAM2;
	}

	change_frequencies(plln, pllr, rcc_cfgr);
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND_FLAGS(
	clock_set, command_clock_set, "clock_set [Hz] [divisor]",
	"Valid range: Hz: 10,000,000 to 110,000,000 (with restrictions)\n"
	"             divisor: 1, 2, 4, 8",
	CMD_FLAG_RESTRICTED);

/******************************************************************************
 * Initialize board.  (More initialization done by hooks in other files.)
 */

static void board_init(void)
{
	/* Measure frequency of external oscillator, if any. */
	probe_external_clock();

	/* USB to serial queues */
	queue_init(&usart2_to_usb);
	queue_init(&usb_to_usart2);
	queue_init(&usart3_to_usb);
	queue_init(&usb_to_usart3);
	queue_init(&usart4_to_usb);
	queue_init(&usb_to_usart4);
	queue_init(&usart5_to_usb);
	queue_init(&usb_to_usart5);

	/* UART init */
	usart_init(&usart2);
	usart_init(&usart3);
	usart_init(&usart4);
	usart_init(&usart5);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_PRE_DEFAULT);

static void usart_reinit(struct usb_stream_config const *usart_usb,
			 struct usart_config const *usart)
{
	usb_usart_clear(usart_usb, usart, CLEAR_BOTH_FIFOS);
	usart_set_parity(usart, 0);
	usart_set_baud(usart, 115200);
	usart_set_break(usart, false);
}

static void usart_reinit_all(void)
{
	usart_reinit(&usart2_usb, &usart2);
	usart_reinit(&usart3_usb, &usart3);
	usart_reinit(&usart4_usb, &usart4);
	usart_reinit(&usart5_usb, &usart5);
}
DECLARE_HOOK(HOOK_REINIT, usart_reinit_all, HOOK_PRIO_DEFAULT);

static int command_reinit(int argc, const char **argv)
{
	/* Switch back to power-on clock configuration. */
	change_frequencies(
		55, 2, STM32_RCC_CFGR_PPRE1_DIV4 | STM32_RCC_CFGR_PPRE2_DIV4);

	/* Let every module know to re-initialize to power-on state. */
	hook_notify(HOOK_REINIT);

	/*
	 * Since we apparently have some communication with OpenTitanTool,
	 * rewind "repeating reset" counter, which would otherwise trigger DFU
	 * mode after a certain number of resets.
	 */
	dfu_bootmanager_clear();
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND_FLAGS(
	reinit, command_reinit, "",
	"Stop any ongoing operation, revert to power-on state.",
	CMD_FLAG_RESTRICTED);

const char *board_read_serial(void)
{
	const uint32_t *stm32_unique_id =
		(const uint32_t *)STM32_UNIQUE_ID_BASE;
	static char serial[13];

	// Compute 12 hex digits from three factory programmed 32-bit "Unique
	// ID" words in a manner that has been observed to be consistent with
	// how the STM DFU ROM bootloader presents its serial number.  This
	// means that the serial number of any particular HyperDebug board will
	// remain the same as it enters and leaves DFU mode for software
	// upgrade.
	int rc = snprintf(serial, sizeof(serial), "%08X%04X",
			  stm32_unique_id[0] + stm32_unique_id[2],
			  stm32_unique_id[1] >> 16);
	if (12 != rc)
		return NULL;
	return serial;
}
