/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Hammer board configuration */

#include "common.h"
#include "ec_version.h"
#include "touchpad_elan.h"
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
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 400,
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

#ifdef SECTION_IS_RW
struct consumer const usart3_consumer;
static struct usart_config const usart3;

static struct queue const usart3_input = QUEUE_DIRECT(64, uint8_t,
	usart3.producer, usart3_consumer);
static struct queue const usart3_output = QUEUE_DIRECT(64, uint8_t,
        null_producer, usart3.consumer);

static void usart3_written(struct consumer const *consumer, size_t count)
{
	/*CPRINTS("%s %d", __func__, count);*/
	task_wake(TASK_ID_ECCOMM);
}

static void usart3_flush(struct consumer const *consumer)
{
	CPRINTS("%s", __func__);
}

#include "battery.h"
#include "crc.h"
#include "ec_comm.h"

/* Battery information that does not change */
struct ec_comm_battery_static_info bat_static = {
	.design_capacity = 6300,
	.design_voltage = 7700,
	.cycle_count = 3,
	.manufacturer = "man",
	.model = "model",
	.serial = "serial",
	.type = "type",
};

struct ec_comm_battery_dynamic_info bat_dynamic = {
        .voltage = 8608,
	.current = 250,
	.remaining_capacity = 6000,
	.full_capacity = 6250,
	.status = 0x00e0,
	.flags = 0x02,

	.desired_voltage = 8700,
	.desired_current = 300,
};

static void flush_queue(void) {
	//CPRINTS("%s %d", __func__, queue_count(&usart3_input));

	while (queue_count(&usart3_input) > 0) {
		queue_advance_head(&usart3_input, queue_count(&usart3_input));
		usleep(1*MSEC);
		//CPRINTS("%s %d", __func__, queue_count(&usart3_input));
	}
}

static void add_data(uint32_t *data, int len) {
	int i;

	for (i = 0; i < len; i++) {
		crc32_hash32(data[i]);
		QUEUE_ADD_UNITS(&usart3_output, &data[i], 4);
	}
}

static void write_reply(uint8_t cmd, uint32_t *data, int len) {
	struct ec_comm_header header;
	uint32_t crc32;

	header.direction = EC_COMM_DIR_IN;
	header.cmd = cmd;
	header._reserved = 0;
	header.length = len/4;
	crc32_init();
	add_data((void*)&header, 1);
	add_data(data, len/4);
	crc32 = crc32_result();
	QUEUE_ADD_UNITS(&usart3_output, (void *)&crc32, sizeof(crc32));
}

void ec_comm_task(void *u)
{
	uint32_t command[4];
	struct ec_comm_header* header = (void *)&command[0];
	int len;
	//uint32_t crc32;

	while (1) {
                task_wait_event(-1);

		//CPRINTS("%s loop %d", __func__, queue_count(&usart3_input));

		if (queue_count(&usart3_input) == 0)
			continue;

		if (queue_count(&usart3_input) > 0 &&
			queue_count(&usart3_input) < 4) {
			usleep(1000);
		}

		//CPRINTS("%s loop2 %d", __func__, queue_count(&usart3_input));

		if (queue_count(&usart3_input) < 4) {
			flush_queue();
			continue;
		}

		QUEUE_REMOVE_UNITS(&usart3_input, (void *)header, 4);

		//CPRINTS("%s %08x", __func__, *((uint32_t*)header));
		CPRINTS("%s dir=%02x cmd=%02x, length=%d", __func__,
			header->direction, header->cmd, header->length);

		len = 4*(header->length-1);

		/* Flush on obvious errors */
		if (len > 60 || len < 0 ||
			header->direction != EC_COMM_DIR_OUT) {
			flush_queue();
			continue;
		}

		while (queue_count(&usart3_input) < len) {
			udelay(10);
		}

		QUEUE_REMOVE_UNITS(&usart3_input, (void *)command[1], len);

		if (header->cmd == EC_COMM_BATTERY_STATIC_INFO) {
			write_reply(EC_COMM_BATTERY_STATIC_INFO,
				(void *)&bat_static, sizeof(bat_static));
			continue;
		} else if (header->cmd == EC_COMM_BATTERY_DYNAMIC_INFO) {
			write_reply(EC_COMM_BATTERY_DYNAMIC_INFO,
				(void *)&bat_dynamic, sizeof(bat_dynamic));
		} else {
			flush_queue();
		}
        }
}

struct consumer const usart3_consumer = {
        .queue = &usart3_input,
        .ops   = &((struct consumer_ops const) {
                .written = usart3_written,
                .flush   = usart3_flush,
        }),
};

static struct usart_config const usart3 =
	USART_CONFIG(usart3_hw,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		usart3_input,
		usart3_output);
#endif

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
#ifdef SECTION_IS_RW
	/* USB to serial queues */
	queue_init(&usart3_input);
	queue_init(&usart3_output);

	/* UART init */
	usart_init(&usart3);
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

