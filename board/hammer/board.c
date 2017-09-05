/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Hammer board configuration */

#include "common.h"
#include "ec_version.h"
#include "charge_state_v2.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "i2c.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "printf.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "queue.h"
#include "queue_policies.h"
#include "registers.h"
#include "rollback.h"
#include "system.h"
#include "task.h"
#include "touchpad_elan.h"
#include "timer.h"
#include "update_fw.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_descriptor.h"
#include "usb_i2c.h"
#include "util.h"

#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

#ifdef SECTION_IS_RW
#define CROS_EC_SECTION "RW"
#else
#define CROS_EC_SECTION "RO"
#endif

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Hammer"),
	[USB_STR_SERIALNO]     = 0,
	[USB_STR_VERSION]      =
			USB_STRING_DESC(CROS_EC_SECTION ":" CROS_EC_VERSION32),
	[USB_STR_I2C_NAME]     = USB_STRING_DESC("I2C"),
	[USB_STR_UPDATE_NAME]  = USB_STRING_DESC("Firmware update"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * Support I2C bridging over USB, this requires usb_i2c_board_enable and
 * usb_i2c_board_disable to be defined to enable and disable the I2C bridge.
 */

#ifdef SECTION_IS_RW
#ifdef BOARD_WAND
/* Battery needs 100 kHz */
#define I2C_FREQ 100 /* kHz */
#else
#define I2C_FREQ 400 /* kHz */
#endif

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, I2C_FREQ,
		GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef BOARD_STAFF
#define KBLIGHT_PWM_FREQ 100 /* Hz */
#else
#define KBLIGHT_PWM_FREQ 10000 /* Hz */
#endif

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(TIM_KBLIGHT), STM32_TIM_CH(1), 0, KBLIGHT_PWM_FREQ},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

int usb_i2c_board_enable(void) { return EC_SUCCESS; }
void usb_i2c_board_disable(void) {}

int usb_i2c_board_is_enabled(void)
{
	/* Disable I2C passthrough when the system is locked */
	return !system_is_locked();
}

#ifdef CONFIG_KEYBOARD_BOARD_CONFIG
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 50,
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x3c, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};
#endif
#endif

#if defined(BOARD_WAND) && defined(SECTION_IS_RW)
struct consumer const ec_ec_usart_consumer;
static struct usart_config const ec_ec_usart;

static struct queue const ec_ec_usart_input = QUEUE_DIRECT(64, uint8_t,
				ec_ec_usart.producer, ec_ec_usart_consumer);
static struct queue const ec_ec_usart_output = QUEUE_DIRECT(64, uint8_t,
				null_producer, ec_ec_usart.consumer);

static void ec_ec_usart_written(struct consumer const *consumer, size_t count)
{
	/*CPRINTS("%s %d", __func__, count);*/
	task_wake(TASK_ID_ECCOMM);
}

static void ec_ec_usart_flush(struct consumer const *consumer)
{
	CPRINTS("%s", __func__);
}

#include "battery.h"
#include "crc.h"
#include "ec_comm.h"

struct ec_comm_battery_static_info base_battery_static;
struct ec_comm_battery_dynamic_info base_battery_dynamic;

static void flush_queue(void)
{
	while (queue_count(&ec_ec_usart_input) > 0) {
		queue_advance_head(&ec_ec_usart_input,
				queue_count(&ec_ec_usart_input));
		usleep(1*MSEC);
	}
}

static void add_data(uint32_t *data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		crc32_hash32(data[i]);
		QUEUE_ADD_UNITS(&ec_ec_usart_output, &data[i], 4);
	}
}

static void write_reply(uint8_t cmd, int seq, uint32_t *data, int len)
{
	struct ec_comm_header header;
	uint32_t crc32;

	memset(&header, 0, sizeof(header));

	header.direction = EC_COMM_DIR_IN;
	header.seq = seq;
	header.version = EC_COMM_VERSION;
	header.cmd = cmd;
	header.length = len/4;
	crc32_init();
	add_data((void *)&header, 1);
	add_data(data, len/4);
	crc32 = crc32_result();
	QUEUE_ADD_UNITS(&ec_ec_usart_output, (void *)&crc32, sizeof(crc32));
}

void ec_comm_task(void *u)
{
	uint32_t command[4];
	struct ec_comm_header *header = (void *)&command[0];
	int len;
	//uint32_t crc32;

	while (1) {
		task_wait_event(-1);

		if (queue_count(&ec_ec_usart_input) == 0)
			continue;

		if (queue_count(&ec_ec_usart_input) > 0 &&
			queue_count(&ec_ec_usart_input) < 4) {
			usleep(1000);
		}

		if (queue_count(&ec_ec_usart_input) < 4) {
			flush_queue();
			continue;
		}

		QUEUE_REMOVE_UNITS(&ec_ec_usart_input, (void *)header, 4);

#if 0
		CPRINTS("%s dir=%02x cmd=%02x, length=%d", __func__,
			header->direction, header->cmd, header->length);
#endif

		len = 4*(header->length+1);

		/* Flush on errors. */
		if (len > (sizeof(command) - 4) || len < 0 ||
				header->direction != EC_COMM_DIR_OUT ||
				header->version != EC_COMM_VERSION) {
			flush_queue();
			continue;
		}

		/* FIXME: Add timeout */
		while (queue_count(&ec_ec_usart_input) < len) {
			udelay(10);
		}

		QUEUE_REMOVE_UNITS(&ec_ec_usart_input, (void *)&command[1], len);

		/* FIXME: Check CRC!! */

		if (header->cmd == EC_COMM_BATTERY_STATIC_INFO) {
			write_reply(EC_COMM_BATTERY_STATIC_INFO, header->seq,
				(void *)&base_battery_static,
				sizeof(base_battery_static));
		} else if (header->cmd == EC_COMM_BATTERY_DYNAMIC_INFO) {
			write_reply(EC_COMM_BATTERY_DYNAMIC_INFO, header->seq,
				(void *)&base_battery_dynamic,
				sizeof(base_battery_dynamic));
		} else if (header->cmd == EC_COMM_CHARGER_CONTROL) {
			/* FIXME: Sanity check command length */
			struct ec_comm_charger_control *ctrl =
				(void *)&command[1];
			if (ctrl->max_current >= 0) {
				charger_enable_otg_power(0);
				charge_set_input_current_limit(ctrl->max_current, 1 /*FIXME*/);
			} else {
				/* FIXME: reset to minimum */
				charge_set_input_current_limit(128, 1 /*FIXME*/);
				/* Do OTG */
				charger_set_otg_current_voltage(
					-ctrl->max_current, ctrl->otg_voltage);
				charger_enable_otg_power(1);
			}
			/* FIXME: rewrite ctrl. */
			write_reply(EC_COMM_CHARGER_CONTROL, header->seq,
				(void *)ctrl, sizeof(*ctrl));
		} else {
			flush_queue();
		}
	}
}

struct consumer const ec_ec_usart_consumer = {
	.queue = &ec_ec_usart_input,
	.ops   = &((struct consumer_ops const) {
		.written = ec_ec_usart_written,
		.flush   = ec_ec_usart_flush,
	}),
};

static struct usart_config const ec_ec_usart =
	USART_CONFIG(EC_EC_UART,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		USART_CONFIG_FLAG_RX_INV | USART_CONFIG_FLAG_TX_INV,
		ec_ec_usart_input,
		ec_ec_usart_output);
#endif /* BOARD_WAND && SECTION_IS_RW */

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
#if defined(BOARD_WAND) && defined(SECTION_IS_RW)
	/* USB to serial queues */
	queue_init(&ec_ec_usart_input);
	queue_init(&ec_ec_usart_output);

	/* UART init */
	usart_init(&ec_ec_usart);
#endif
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/* Remap USART DMA to match the USART driver */
	/*
	 * the DMA mapping is :
	 *  Chan 4 : USART1_TX
	 *  Chan 5 : USART1_RX
	 */
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10); /* Remap USART1 RX/TX DMA */
}

/*
 * Side-band USB wake, to be able to wake lid even in deep S3, when USB
 * controller is off.
 */
void board_usb_wake(void)
{
	/*
	 * Poke detection pin for about 500us, we disable interrupts
	 * to make sure that we do not get preempted (setting GPIO high
	 * for too long would prevent pulse detection on lid EC side from
	 * working properly, or even kill hammer power if it is held for
	 * longer than debounce time).
	 */
	interrupt_disable();
	gpio_set_flags(GPIO_BASE_DET, GPIO_OUT_HIGH);
	udelay(500);
	gpio_set_flags(GPIO_BASE_DET, GPIO_INPUT);
	interrupt_enable();
}

/*
 * Get entropy based on Clock Recovery System, which is enabled on hammer to
 * synchronize USB SOF with internal oscillator.
 */
int board_get_entropy(void *buffer, int len)
{
	int i = 0;
	uint8_t *data = buffer;
	uint32_t start;
	/* We expect one SOF per ms, so wait at most 2ms. */
	const uint32_t timeout = 2*MSEC;

	for (i = 0; i < len; i++) {
		STM32_CRS_ICR |= STM32_CRS_ICR_SYNCOKC;
		start = __hw_clock_source_read();
		while (!(STM32_CRS_ISR & STM32_CRS_ISR_SYNCOKF)) {
			if ((__hw_clock_source_read() - start) > timeout)
				return 0;
			usleep(500);
		}
		/* Pick 8 bits, including FEDIR and 7 LSB of FECAP. */
		data[i] = STM32_CRS_ISR >> 15;
	}

	return 1;
}

/*
 * Generate a USB serial number from unique chip ID.
 */
const char *board_read_serial(void)
{
	static char str[CONFIG_SERIALNO_LEN];

	if (str[0] == '\0') {
		uint8_t *id;
		int pos = 0;
		int idlen = system_get_chip_unique_id(&id);
		int i;

		for (i = 0; i < idlen && pos < sizeof(str); i++, pos += 2) {
			snprintf(&str[pos], sizeof(str)-pos,
				"%02x", id[i]);
		}
	}

	return str;
}

int board_write_serial(const char *serialno)
{
	return 0;
}

#ifdef BOARD_WAND
/* TODO(b:66575472): This assumes external power is always present. */
int extpower_is_present(void)
{
	return 1;
}
#endif
