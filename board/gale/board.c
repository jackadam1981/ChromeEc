/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* gale board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "case_closed_debug.h"
#include "charge_manager.h"
#include "common.h"
#include "console.h"
#include "driver/tcpm/tcpm.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usb_spi.h"
#include "usb-stream.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)
#define CPUTS(string) cputs(CC_COMMAND, string)

#include "gpio_list.h"

const void *const usb_strings[] = {
	[USB_STR_DESC]           = usb_string_desc,
	[USB_STR_VENDOR]         = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]        = USB_STRING_DESC("Gale debug"),
	[USB_STR_VERSION]        = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME]   = USB_STRING_DESC("EC_PD"),
	[USB_STR_AP_STREAM_NAME] = USB_STRING_DESC("AP"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/*
 * Define AP console forwarding queue and associated USART and USB
 * stream endpoints.
 */
static struct usart_config const ap_usart;

struct usb_stream_config const ap_usb;

static struct queue const ap_usart_to_usb = QUEUE_DIRECT(64, uint8_t,
							 ap_usart.producer,
							 ap_usb.consumer);
static struct queue const ap_usb_to_usart = QUEUE_DIRECT(64, uint8_t,
							 ap_usb.producer,
							 ap_usart.consumer);

/* USART2 use DMA CH4(tx) & CH5(rx) */
static struct usart_tx_dma const ap_usart_tx_dma =
	USART_TX_DMA(STM32_DMAC_CH4, 16);

static struct usart_config const ap_usart =
	USART_CONFIG(usart2_hw,
		     usart_rx_interrupt,
		     ap_usart_tx_dma.usart_tx,
		     115200,
		     ap_usart_to_usb,
		     ap_usb_to_usart);

#define AP_USB_STREAM_RX_SIZE	16
#define AP_USB_STREAM_TX_SIZE	16

USB_STREAM_CONFIG(ap_usb,
		  USB_IFACE_AP_STREAM,
		  USB_STR_AP_STREAM_NAME,
		  USB_EP_AP_STREAM,
		  AP_USB_STREAM_RX_SIZE,
		  AP_USB_STREAM_TX_SIZE,
		  ap_usb_to_usart,
		  ap_usart_to_usb)

const struct adc_t adc_channels[] = {
	/* PA1: STM32_AIN 1 */
	[ADC_CC1]       = {"CC1",  3300, 4096, 0, STM32_AIN(1)},
	/* PA3: STM32_AIN 3 */
	[ADC_CC2]       = {"CC2",  3300, 4096, 0, STM32_AIN(3)},
	/* PB0: 1/2 VBUS voltage */
	[ADC_VBUS]      = {"VBUS", 6600, 4096, 0, STM32_AIN(8)},
	/* PB0: sense resistor: 10 mOhm scale 50x */
	[ADC_CUR_SENSE] = {"CUR",  6600, 4096, 0, STM32_AIN(9)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct i2c_port_t i2c_ports[] = {
	{"pd", I2C_PORT_SLAVE, 1000, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 0, GPIO_SPI_FLASH_NSS},
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/* Gale AP power state */
static int8_t gale_power;

/* Adding dependencies */
void charge_manager_update_dualrole(int port, enum dualrole_capabilities cap)
{
}

/* USB superspeed mux */

static int usbmux_init(int i2c_addr)
{
	gpio_set_level(GPIO_USB_SS_MUX_EN_L, 1);
	return EC_SUCCESS;
}

static int usbmux_set_mux(int i2c_addr, mux_state_t mux_state)
{
	gpio_set_level(GPIO_USB_CC_POLARITY,
		       !(mux_state & MUX_POLARITY_INVERTED));
	gpio_set_level(GPIO_USB_SS_MUX_EN_L,
		       !(mux_state & MUX_USB_ENABLED));

	return EC_SUCCESS;
}

static int usbmux_get_mux(int i2c_addr, mux_state_t *mux_state)
{
	*mux_state = gpio_get_level(GPIO_USB_SS_MUX_EN_L) ? MUX_USB_ENABLED : 0;
	return EC_SUCCESS;
}

const struct usb_mux_driver usbmux_driver = {
	.init = usbmux_init,
	.set  = usbmux_set_mux,
	.get  = usbmux_get_mux,
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.port_addr = 0,
		.driver    = &usbmux_driver,
	},
};

/* SoC power control */

static void power_on_ap(void)
{
	CPUTS("power on ap\n");
	/* turn on system, IO and memory power rails */
	gpio_set_level(GPIO_SYS_PWR_EN, 1);       /* system power */
	gpio_set_level(GPIO_VDD_3P3_EN, 1);       /* 3.3v - io */
	gpio_set_level(GPIO_VDD_3P3_2G_EN, 1);
	gpio_set_level(GPIO_VDD_1P8_EN, 1);       /* 1.8v */
	gpio_set_level(GPIO_VDD_1P35_EN, 1);      /* 1.35v - memory */
	gpio_set_level(GPIO_VDD_1P1_CPU_EN, 1);   /* 1.1v - cpu */

	gale_power = 1;
}
DECLARE_DEFERRED(power_on_ap);

static void power_off_ap(void)
{
	CPUTS("power off ap\n");
	/* drive MCU_INT_L low */
	gpio_set_level(GPIO_MCU_INT_L, 0);
	/* turn off AP core power */
	gpio_set_level(GPIO_VDD_1P1_CPU_EN, 0);
	/* turn off 1.35v, 1.8v and 3.3v power rails */
	gpio_set_level(GPIO_VDD_1P35_EN, 0);
	gpio_set_level(GPIO_VDD_1P8_EN, 0);
	gpio_set_level(GPIO_VDD_3P3_2G_EN, 0);
	gpio_set_level(GPIO_VDD_3P3_EN, 0);
	gpio_set_level(GPIO_SYS_PWR_EN, 0);

	gale_power = 0;
}
DECLARE_DEFERRED(power_off_ap);

void board_set_power_supply_ready(int ready)
{
	hook_call_deferred(ready ? &power_on_ap_data : &power_off_ap_data, 0);
}

/**
 * Enable and disable SPI for case closed debugging.  This forces the AP into
 * reset while SPI is enabled, thus preventing contention on the SPI interface.
 */
void usb_spi_board_enable(struct usb_spi_config const *config)
{
	/* Place AP into reset */
	/* turn off 1.35v, 1.8v power rails */
	gpio_set_level(GPIO_VDD_1P35_EN, 0);
	gpio_set_level(GPIO_VDD_1P8_EN, 0);

	/* Enable 5V and 3.3V */
	if (!(gpio_get_level(GPIO_SYS_PWR_EN) &&
	      gpio_get_level(GPIO_VDD_3P3_EN))) {
		gpio_set_level(GPIO_SYS_PWR_EN, 1);
		gpio_set_level(GPIO_VDD_3P3_EN, 1);
		usleep(25);
	}

	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_FLASH, 1);
	gpio_set_flags(SPI_FLASH_DEVICE->gpio_cs, GPIO_OUT_HIGH);

	/* Set all four SPI pins to high speed */
	/* pins B13/14/15 and B12 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000;

	/* Enable clocks to SPI2 module */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;

	/* Reset SPI2 */
	STM32_RCC_APB1RSTR |= STM32_RCC_PB1_SPI2;
	STM32_RCC_APB1RSTR &= ~STM32_RCC_PB1_SPI2;

	spi_enable(CONFIG_SPI_FLASH_PORT, 1);
}

void usb_spi_board_disable(struct usb_spi_config const *config)
{

	/* Disable clocks to SPI2 module */
	STM32_RCC_APB1ENR &= ~STM32_RCC_PB1_SPI2;

	/* Release SPI GPIOs */
	gpio_config_module(MODULE_SPI_FLASH, 0);
	gpio_set_flags(SPI_FLASH_DEVICE->gpio_cs, GPIO_INPUT);

	spi_enable(CONFIG_SPI_FLASH_PORT, 0);
}

static int get_typec_max_current(int cc)
{
	if (cc == TYPEC_CC_VOLT_SNK_3_0)
		return 3000;
	if (cc == TYPEC_CC_VOLT_SNK_1_5)
		return 1500;
	if (cc == TYPEC_CC_VOLT_SNK_DEF)
		return 900;
	return 0;
}

static int is_typec_sink(int cc)
{
	return (cc == TYPEC_CC_VOLT_SNK_3_0 ||
		cc == TYPEC_CC_VOLT_SNK_1_5 ||
		cc == TYPEC_CC_VOLT_SNK_DEF);
}

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/*
	 * DMA map
	 *   CH1 - ADC
	 *   CH2 - TIM1_CH1:      CC RX (comp output capture)
	 *   CH3 - SPI1_TX:       CC TX
	 *   CH4 - USART2 RX
	 *   CH5 - USART2 TX:     CCD USART TX
	 *   CH6 - remap SPI2_RX: CCD SPI flash
	 *   CH7 - remap SPI2_TX: CCD SPI flash
	 */

	/* Remap SPI2 RX/TX DMA */
	STM32_SYSCFG_CFGR1 |= (1 << 24);
}

static void enable_ccd(void)
{
	ccd_set_mode(system_is_locked() ? CCD_MODE_PARTIAL
					: CCD_MODE_ENABLED);
}

static void detect_ccd(void)
{
	int cc1, cc2;

	/*
	 * Gale is solely powered by type-C adapter. And since it is not a
	 * dualrole device, check both CC pins are not connected as sink.
	 */
	tcpm_get_cc(0, &cc1, &cc2);
	if (!is_typec_sink(cc1) && !is_typec_sink(cc2))
		enable_ccd();
}
DECLARE_DEFERRED(detect_ccd);

static void board_init(void)
{
	/*
	 * Initialize AP console forwarding USART and queues.
	 */
	queue_init(&ap_usart_to_usb);
	queue_init(&ap_usb_to_usart);
	usart_init(&ap_usart);

	/*
	 * Disable UART input when the Write Protect is enabled.
	 * Detect suzy-q after 1 second if system is not locked.
	 */
	if (system_is_locked())
		ap_usb.state->rx_disabled = 1;
	else
		hook_call_deferred(&detect_ccd_data, 1000*MSEC);

}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Console commands */

static int gale_command_power(int argc, char **argv)
{
	int val;

	if (!system_is_locked() && argc >= 2)
		if (parse_bool(argv[1], &val)) {
			board_set_power_supply_ready(val);
			CPUTS("OK\n");
			return EC_SUCCESS;
		}

	CPRINTF("%8s - %s\n", argv[0], gale_power ? "on" : "off");
	return EC_SUCCESS;
}

static int gale_command_polarity(int argc, char **argv)
{
	if (!system_is_locked() && argc >= 2) {
		gpio_set_level(GPIO_USB_CC_POLARITY,
			       !strtoi(argv[1], NULL, 0));
		CPUTS("OK\n");
	}

	CPRINTF("%8s - %d\n", argv[0], !gpio_get_level(GPIO_USB_CC_POLARITY));
	return EC_SUCCESS;
}

static int gale_command_cc(int argc, char **argv)
{
	int cc1, cc2;
	int v1 = adc_read_channel(ADC_CC1);
	int v2 = adc_read_channel(ADC_CC2);
	int max_ma;

	tcpm_get_cc(0, &cc1, &cc2);
	CPRINTF("%8s - %dmV", argv[0], v1);
	max_ma = get_typec_max_current(cc1);
	if (max_ma)
		CPRINTF("(%dmA)", max_ma);
	CPRINTF(", %dmV", v2);
	max_ma = get_typec_max_current(cc2);
	if (max_ma)
		CPRINTF("(%dmA)", max_ma);
	CPUTS("\n");

	return EC_SUCCESS;
}

static int gale_command_vbus(int argc, char **argv)
{
	CPRINTF("%8s - %dmv %dma\n", argv[0], adc_read_channel(ADC_VBUS),
		adc_read_channel(ADC_CUR_SENSE));
	return EC_SUCCESS;
}

static int gale_command_dev(int argc, char **argv)
{
	int val;

	if (!system_is_locked() && argc >= 2)
		if (parse_bool(argv[1], &val)) {
			gpio_set_flags(GPIO_ENTERING_DEV, val ?
				       GPIO_OUT_LOW : GPIO_INPUT);
			CPUTS("OK\n");
		}

	CPRINTF("dev switch is %s\n", gpio_get_level(GPIO_ENTERING_DEV) ?
		"OFF" : "ON");
	return EC_SUCCESS;
}

static int gale_command_rec(int argc, char **argv)
{
	int val;

	if (!system_is_locked() && argc >= 2)
		if (parse_bool(argv[1], &val)) {
			gpio_set_flags(GPIO_ENTERING_REC, val ?
				       GPIO_OUT_LOW : GPIO_INPUT);
			CPUTS("OK\n");
		}

	CPRINTF("rec switch is %s\n", gpio_get_level(GPIO_ENTERING_REC) ?
		"OFF" : "ON");
	return EC_SUCCESS;
}

static int gale_command(int argc, char **argv)
{
	const struct {
		char *name;
		int (*func)(int, char**);
	} const sub_commands[] = {
		{"power",    gale_command_power},
		{"polarity", gale_command_polarity},
		{"cc",       gale_command_cc},
		{"vbus",     gale_command_vbus},
		{"dev",      gale_command_dev},
		{"rec",      gale_command_rec},
	};

	int i;

	if (argc < 2) {
		for (i = 0; i < ARRAY_SIZE(sub_commands); i++) {
			char *sub_argv[] = { sub_commands[i].name };

			sub_commands[i].func(1, sub_argv);
		}
		return EC_SUCCESS;
	}

	for (i = 0; i < ARRAY_SIZE(sub_commands); i++) {
		if (!strncasecmp(argv[1],
				 sub_commands[i].name,
				 strlen(sub_commands[i].name)))
			return sub_commands[i].func(argc - 1, argv + 1);
	}

	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND(gale, gale_command,
			"[power [on|off]|polarity [0|1]|dev [on|off]|"
			"rec [on|off]|cc|vbus",
			"Get and set gale controls");
