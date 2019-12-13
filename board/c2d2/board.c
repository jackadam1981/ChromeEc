/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* C2D2 debug device board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "update_fw.h"
#include "usart_rx_dma.h"
#include "usart_tx_dma.h"
#include "usart-stm32f0.h"
#include "usb_hw.h"
#include "usb_i2c.h"
#include "usb_spi.h"
#include "usb-stream.h"
#include "util.h"

#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

/* TODO check this */
void board_config_pre_init(void)
{
	/* enable SYSCFG & COMP clock */
	STM32_RCC_APB2ENR |= STM32_RCC_SYSCFGEN;

	/* enable DAC for comparator input */
	STM32_RCC_APB1ENR |= STM32_RCC_DACEN;

	/*
	 * the DMA mapping is :
	 *  Chan 3 : USART3_RX
	 *  Chan 5 : USART1_RX
	 *  Chan 6 : USART4_RX (Disable)
	 *  Chan 6 : SPI2_RX
	 *  Chan 7 : SPI2_TX
	 *
	 *  i2c : no dma
	 *  tim16/17: no dma
	 */
	STM32_SYSCFG_CFGR1 |= BIT(26);  /* Remap USART3 RX/TX DMA */
	STM32_SYSCFG_CFGR1 |= BIT(10);  /* Remap USART1 RX/TX DMA */

	/* Remap SPI2 to DMA channels 6 and 7 */
	/* STM32F072 SPI2 defaults to using DMA channels 4 and 5 */
	/* but cros_ec hardcodes a 6/7 assumption in registers.h */
	STM32_SYSCFG_CFGR1 |= BIT(24);
}

/******************************************************************************
 ** ADC channels
*/
const struct adc_t adc_channels[] = {
	/* Sensing the H1's voltage at the DUT side.  Converted to mV. */
	[ADC_H1_VREF] = {
		.name = "H1_VREF",
		.factor_mul = 3300,
		.factor_div = 4096,
		.shift = 0,
		.channel = STM32_AIN(3),
	},
	/* Sensing the EC's voltage at the DUT side.  Converted to mV. */
	[ADC_EC_VREF] = {
		.name = "EC_VREF",
		.factor_mul = 3300,
		.factor_div = 4096,
		.shift = 0,
		.channel = STM32_AIN(4),
	}
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]         		= usb_string_desc,
	[USB_STR_VENDOR]       		= USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      		= USB_STRING_DESC("C2D2"),
	[USB_STR_SERIALNO]     		= 0,
	[USB_STR_VERSION]      		= USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_USART4_STREAM_NAME]  	= USB_STRING_DESC("CR50"),
	[USB_STR_UPDATE_NAME]  		= USB_STRING_DESC("Firmware update"),
	[USB_STR_CONSOLE_NAME] 		= USB_STRING_DESC("C2D2 Shell"),
	[USB_STR_I2C_NAME]     		= USB_STRING_DESC("I2C"),
	[USB_STR_USART3_STREAM_NAME]  	= USB_STRING_DESC("CPU"),
	[USB_STR_USART1_STREAM_NAME]  	= USB_STRING_DESC("EC"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);


/******************************************************************************
 * Forward UARTs as a USB serial interface.
 */

#define USB_STREAM_RX_SIZE	32
#define USB_STREAM_TX_SIZE	64

/******************************************************************************
 * Forward USART1 (EC) as a simple USB serial interface.
 */

static struct usart_config const usart1;
struct usb_stream_config const usart1_usb;

static struct queue const usart1_to_usb = QUEUE_DIRECT(128, uint8_t,
	usart1.producer, usart1_usb.consumer);
static struct queue const usb_to_usart1 = QUEUE_DIRECT(64, uint8_t,
	usart1_usb.producer, usart1.consumer);

static struct usart_rx_dma const usart1_rx_dma =
	USART_RX_DMA(STM32_DMAC_CH5, 32);

static struct usart_config const usart1 =
	USART_CONFIG(usart1_hw,
		usart1_rx_dma.usart_rx,
		usart_tx_interrupt,
		115200,
		0,
		usart1_to_usb,
		usb_to_usart1);

USB_STREAM_CONFIG_USART_IFACE(usart1_usb,
	USB_IFACE_USART1_STREAM,
	USB_STR_USART1_STREAM_NAME,
	USB_EP_USART1_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart1,
	usart1_to_usb,
	usart1)


/******************************************************************************
 * Forward USART3 (CPU) as a simple USB serial interface.
 */

static struct usart_config const usart3;
struct usb_stream_config const usart3_usb;

static struct queue const usart3_to_usb = QUEUE_DIRECT(1024, uint8_t,
	usart3.producer, usart3_usb.consumer);
static struct queue const usb_to_usart3 = QUEUE_DIRECT(64, uint8_t,
	usart3_usb.producer, usart3.consumer);

static struct usart_rx_dma const usart3_rx_dma =
	USART_RX_DMA(STM32_DMAC_CH3, 32);

static struct usart_config const usart3 =
	USART_CONFIG(usart3_hw,
		usart3_rx_dma.usart_rx,
		usart_tx_interrupt,
		115200,
		0,
		usart3_to_usb,
		usb_to_usart3);

USB_STREAM_CONFIG_USART_IFACE(usart3_usb,
	USB_IFACE_USART3_STREAM,
	USB_STR_USART3_STREAM_NAME,
	USB_EP_USART3_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart3,
	usart3_to_usb,
	usart3)


/******************************************************************************
 * Forward USART4 (cr50) as a simple USB serial interface.
 *  We cannot enable DMA due to lack of DMA channels.
 */

static struct usart_config const usart4;
struct usb_stream_config const usart4_usb;

static struct queue const usart4_to_usb = QUEUE_DIRECT(64, uint8_t,
	usart4.producer, usart4_usb.consumer);
static struct queue const usb_to_usart4 = QUEUE_DIRECT(64, uint8_t,
	usart4_usb.producer, usart4.consumer);

static struct usart_config const usart4 =
	USART_CONFIG(usart4_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		0,
		usart4_to_usb,
		usb_to_usart4);

USB_STREAM_CONFIG_USART_IFACE(usart4_usb,
	USB_IFACE_USART4_STREAM,
	USB_STR_USART4_STREAM_NAME,
	USB_EP_USART4_STREAM,
	USB_STREAM_RX_SIZE,
	USB_STREAM_TX_SIZE,
	usb_to_usart4,
	usart4_to_usb,
	usart4)

/******************************************************************************
 * Check parity setting on usarts.
 */
static int command_uart_parity(int argc, char **argv)
{
	int parity = 0, newparity;
	struct usart_config const *usart;
	char *e;

	if ((argc < 2) || (argc > 3))
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "usart1"))
		usart = &usart1;
	else if (!strcasecmp(argv[1], "usart3"))
		usart = &usart3;
	else if (!strcasecmp(argv[1], "usart4"))
		usart = &usart4;
	else
		return EC_ERROR_PARAM1;

	if (argc == 3) {
		parity = strtoi(argv[2], &e, 0);
		if (*e || (parity < 0) || (parity > 2))
			return EC_ERROR_PARAM2;

		usart_set_parity(usart, parity);
	}

	newparity = usart_get_parity(usart);
	ccprintf("Parity on %s is %d.\n", argv[1], newparity);

	if ((argc == 3) && (newparity != parity))
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(parity, command_uart_parity,
			"usart[2|3|4] [0|1|2]",
			"Set parity on uart");

/******************************************************************************
 * Set baud rate setting on usarts.
 */
static int command_uart_baud(int argc, char **argv)
{
	int baud = 0;
	struct usart_config const *usart;
	char *e;

	if ((argc < 2) || (argc > 3))
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "usart1"))
		usart = &usart1;
	else if (!strcasecmp(argv[1], "usart3"))
		usart = &usart3;
	else if (!strcasecmp(argv[1], "usart4"))
		usart = &usart4;
	else
		return EC_ERROR_PARAM1;

	baud = strtoi(argv[2], &e, 0);
	if (*e || baud < 0)
		return EC_ERROR_PARAM2;

	usart_set_baud(usart, baud);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(baud, command_uart_baud,
			"usart[2|3|4] rate",
			"Set baud rate on uart");

/******************************************************************************
 * Hold the usart pins low while disabling it, or return it to normal.
 */
static int command_hold_usart_low(int argc, char **argv)
{
	/* Each bit represents if that port is being held low */
	static int usart_status;

	const struct usart_config *usart;
	int usart_mask;
	enum gpio_signal tx, rx;

	if (argc > 3 || argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "usart1")) {
		usart = &usart1;
		usart_mask = 1 << 1;
		tx = GPIO_UART_DBG_TX_EC_RX_SCL;
		rx = GPIO_UART_EC_TX_DBG_RX_SDA;
	} else if (!strcasecmp(argv[1], "usart3")) {
		usart = &usart3;
		usart_mask = 1 << 3;
		tx = GPIO_UART_DBG_TX_AP_RX_INA_SCL;
		rx = GPIO_UART_AP_TX_DBG_RX_INA_SDA;
	} else if (!strcasecmp(argv[1], "usart4")) {
		usart = &usart4;
		usart_mask = 1 << 4;
		tx = GPIO_UART_DBG_TX_H1_RX;
		rx = GPIO_UART_H1_TX_DBG_RX;
	} else {
		return EC_ERROR_PARAM1;
	}

	/* Updating the status of this port */
	if (argc == 3) {
		char *e;
		const int hold_low = strtoi(argv[2], &e, 0);

		if (*e || (hold_low < 0) || (hold_low > 1))
			return EC_ERROR_PARAM2;

		if (!!(usart_status & usart_mask) == hold_low) {
			/* Do nothing since there is no change */
		} else if (hold_low) {
			/*
			 * Only one USART can be held low at a time, because
			 * re-initializing one USART will pull all of the USART
			 * GPIO pins back into alternate mode.
			 */
			if (usart_status)
				return EC_ERROR_BUSY;

			/*
			 * Shutdown the USB uart,
			 * turn off alternate mode, then set the RX line
			 * pin to output low to enter UART programming mode.
			 */
			usart_shutdown(usart);
			gpio_config_pin(MODULE_USART, rx, 0);
			gpio_config_pin(MODULE_USART, tx, 0);
			gpio_set_flags(rx, GPIO_OUT_LOW);

			usart_status |= usart_mask;
		} else {
			/*
			 * This will reset the alternate mode of the
			 * GPIO pins appropriately and restart USB UART
			 */
			usart_init(usart);

			/*
			 * Since only one USART can be held low at a time, the
			 * uart_status will always be 0 after this call.
			 */
			usart_status = 0;
		}
	}

	/* Print status for get and set case. */
	ccprintf("USART status: %s\n",
			usart_status & usart_mask ? "held low" : "normal");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hold_usart_low, command_hold_usart_low,
			"usart[1|3|4] [0|1]?",
			"Get/set the hold-low state for usart port");

/* Guards access to h1 reset and h1 vref functionality */


static struct mutex vref_mutex;
static int h1_in_reset_state;
static int pwr_button_state;

/* forware declare (TODO move) */
static void update_vrefs_and_shifters(void);
DECLARE_DEFERRED(update_vrefs_and_shifters);

/******************************************************************************
 * Perform cold_reset using H1_reset
 */
static int command_pwr_button(int argc, char **argv)
{
	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	/* Updating the status of this port */
	if (argc == 2) {
		char *e;
		const int hold_low = strtoi(argv[1], &e, 0);

		if (*e || (hold_low < 0) || (hold_low > 1))
			return EC_ERROR_PARAM1;
		
		mutex_lock(&vref_mutex);
		gpio_set_flags(GPIO_SPIVREF_HOLDN_ECVREF_H1_PWRBTN_ODL,
			       hold_low ? GPIO_OUT_LOW : GPIO_INPUT);

		if (hold_low) {
			STM32_EXTI_IMR &= ~EXTI_COMP2_EVENT;
			gpio_set_level(GPIO_EN_SPIVREF_HOLDN_ECVREF_H1_PWRBTN, 1);
		}

		/* Transitioning out of reset, correct vrefs */
		if (pwr_button_state && !hold_low)
			hook_call_deferred(&update_vrefs_and_shifters_data, 0);

		pwr_button_state = hold_low;
		mutex_unlock(&vref_mutex);
	}

	ccprintf("Power button held: %s\n", pwr_button_state ? "yes" : "no");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pwr_button, command_pwr_button,
			"[0|1]?",
			"Get/set the power button state");


static int command_h1_reset(int argc, char **argv)
{
	if (argc > 2)
		return EC_ERROR_PARAM_COUNT;

	/* Updating the status of this port */
	if (argc == 2) {
		char *e;
		const int hold_low = strtoi(argv[1], &e, 0);

		if (*e || (hold_low < 0) || (hold_low > 1))
			return EC_ERROR_PARAM1;
		
		mutex_lock(&vref_mutex);
		gpio_set_flags(GPIO_SPIVREF_RSVD_H1VREF_H1_RST_ODL,
			       hold_low ? GPIO_OUT_LOW : GPIO_INPUT);

		if (hold_low) {
			STM32_EXTI_IMR &= ~EXTI_COMP2_EVENT;
			gpio_set_level(GPIO_EN_SPIVREF_RSVD_H1VREF_H1_RST, 1);
			/*
			 * Do not turn off EC UART since we need to use the RX
			 * pin to get into UUT boot mode
			 */
		}

		/* Transitioning out of reset, correct vrefs */
		if (h1_in_reset_state && !hold_low)
			hook_call_deferred(&update_vrefs_and_shifters_data, 0);

		h1_in_reset_state = hold_low;
		mutex_unlock(&vref_mutex);
	}

	ccprintf("H1 held in reset: %s\n", h1_in_reset_state ? "yes" : "no");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(h1_reset, command_h1_reset,
			"[0|1]?",
			"Get/set the h1 reset state");


enum vref {
	OFF = 0,
	PP1800 = 1800,
	PP3300 = 3300,
};


/* Voltage thresholds for rail detection */
#define VREF_3300_MIN_MV 2300
#define VREF_1800_MIN_MV 1500

static enum vref get_vref(enum adc_channel chan)
{
	const int adc = adc_read_channel(chan);

	if (adc == ADC_READ_ERROR)
		return OFF;
	else if (adc > VREF_3300_MIN_MV)
		return PP3300;
	else if (adc > VREF_1800_MIN_MV)
		return PP1800;
	else
		return OFF;
}

static void drain_vref_lines(void)
{
	mutex_lock(&vref_mutex);

	if (h1_in_reset_state)
		goto exit_unlock;
	
	gpio_set_level(GPIO_EN_SPIVREF_RSVD_H1VREF_H1_RST, 0);
	gpio_set_level(GPIO_EN_SPIVREF_HOLDN_ECVREF_H1_PWRBTN, 0);

	/* Drain floating voltage while pass through is disconnected */
	gpio_set_flags(GPIO_SPIVREF_RSVD_H1VREF_H1_RST_ODL,
		       GPIO_INPUT | GPIO_PULL_DOWN);
	gpio_set_flags(GPIO_SPIVREF_HOLDN_ECVREF_H1_PWRBTN_ODL,
		       GPIO_INPUT | GPIO_PULL_DOWN);
	
	/* Allow the ADC Vref lines to drain for 50 msec. Do not hold lock. */
	mutex_unlock(&vref_mutex);
	msleep(50);
	mutex_lock(&vref_mutex);

	if (h1_in_reset_state)
		goto exit_unlock;

	gpio_set_flags(GPIO_SPIVREF_RSVD_H1VREF_H1_RST_ODL, GPIO_INPUT);
	gpio_set_flags(GPIO_SPIVREF_HOLDN_ECVREF_H1_PWRBTN_ODL, GPIO_INPUT);

	gpio_set_level(GPIO_EN_SPIVREF_RSVD_H1VREF_H1_RST, 1);
	gpio_set_level(GPIO_EN_SPIVREF_HOLDN_ECVREF_H1_PWRBTN, 1);

	mutex_unlock(&vref_mutex);

	/* Allow the ADC Vref lines to charge to real voltage for 50 msec */
	msleep(50);

	return;

exit_unlock:
	mutex_unlock(&vref_mutex);
}

static void update_vrefs_and_shifters(void)
{
	enum vref h1_vref, ec_vref;
	int adc_mv;

	CPRINTS("Start Vref update");

	drain_vref_lines();

	mutex_lock(&vref_mutex);
	if (h1_in_reset_state)
		goto exit_unlock;
		
	h1_vref = get_vref(ADC_H1_VREF);
	gpio_set_level(GPIO_SEL_SPIVREF_H1VREF_3V3, h1_vref == PP3300);
	gpio_set_level(GPIO_EN_MISO_MOSI_H1_UART, h1_vref != OFF);

	ec_vref = get_vref(ADC_EC_VREF);
	gpio_set_level(GPIO_SEL_SPIVREF_ECVREF_3V3, ec_vref == PP3300);
	gpio_set_level(GPIO_EN_CLK_CSN_EC_UART, ec_vref != OFF);

	/* Set up DAC2 for comparison on H1 Vref */
	adc_mv = (h1_vref == PP3300) ? VREF_3300_MIN_MV : VREF_1800_MIN_MV;
	/* 8-bit DAC based off of 3.3V rail */
	STM32_DAC_DHR8R2 = 256 * adc_mv / 3300;

	/* Clear any pending interrupts and enabled H1 comparator */
	STM32_EXTI_PR = EXTI_COMP2_EVENT;
	STM32_EXTI_IMR |= EXTI_COMP2_EVENT;
	
	mutex_unlock(&vref_mutex);

	CPRINTS("Vref update: H1 -> %d; EC -> %d", h1_vref, ec_vref);
	return;

exit_unlock:
	mutex_unlock(&vref_mutex);
}

static void set_up_comparator(void)
{
	/* Overwrite any previous values. This is the only comp usage */
	STM32_COMP_CSR = STM32_COMP_CMP2HYST_HI |
			 STM32_COMP_CMP2OUTSEL_NONE |
			 STM32_COMP_CMP2INSEL_INM5 | // CMP to DAC_OUT2 (PA5)
			 STM32_COMP_CMP2MODE_LSPEED |
			 STM32_COMP_CMP2EN;

	/* Set Falling and Rising interrupts for COMP2 */
	STM32_EXTI_FTSR |= EXTI_COMP2_EVENT;
	STM32_EXTI_RTSR |= EXTI_COMP2_EVENT;

	/* Interrupt for COMP2 enabled when setting Vrefs */

	task_enable_irq(STM32_IRQ_COMP);
}

static void h1_vref_change(void)
{
	/* Ack the interrupt */
	STM32_EXTI_PR = EXTI_COMP2_EVENT;

	/* Disable interrupt, setting Vref will enable again */
	STM32_EXTI_IMR &= ~EXTI_COMP2_EVENT;

	hook_call_deferred(&update_vrefs_and_shifters_data, 0);
}
DECLARE_IRQ(STM32_IRQ_COMP, h1_vref_change, 1);

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
	/* USB to serial queues */
	queue_init(&usart1_to_usb);
	queue_init(&usb_to_usart1);
	queue_init(&usart3_to_usb);
	queue_init(&usb_to_usart3);
	queue_init(&usart4_to_usb);
	queue_init(&usb_to_usart4);

	/* UART init */
	usart_init(&usart1);
	usart_init(&usart3);
	usart_init(&usart4);

	/* Enabled DAC, when setting Vref, this voltage is adjusted */
	STM32_DAC_CR = STM32_DAC_CR_EN2;

	/* Set Vrefs and enabled level shifters */
	set_up_comparator();

	/*
	 * Ensure we set up vrefs at least once. Don't call here because
	 * there are delays in the reads
	 */
	hook_call_deferred(&update_vrefs_and_shifters_data, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/******************************************************************************
 * Turn down USART before jumping to RW.
 */
static void board_jump(void)
{
	/*
	 * If we don't shutdown the USARTs before jumping to RW, then when early
	 * RW tries to set the GPIOs to input (or anything other than alternate)
	 * the jump fail on some servo micros.
	 *
	 * It also make sense to shut them down since RW will reinitialize them
	 * in board_init above.
	 */
	usart_shutdown(&usart1);
	usart_shutdown(&usart3);
	usart_shutdown(&usart4);

	/* Put the board into safer state while jumping */
	gpio_set_level(GPIO_EN_SPIVREF_RSVD_H1VREF_H1_RST, 0);
	gpio_set_level(GPIO_EN_SPIVREF_HOLDN_ECVREF_H1_PWRBTN, 0);
	gpio_set_level(GPIO_EN_CLK_CSN_EC_UART, 0);
	gpio_set_level(GPIO_EN_MISO_MOSI_H1_UART, 0);
}
DECLARE_HOOK(HOOK_SYSJUMP, board_jump, HOOK_PRIO_DEFAULT);
