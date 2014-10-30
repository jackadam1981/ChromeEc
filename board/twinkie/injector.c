/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"

/* Stub the function as we are not using the RX path */
void pd_set_max_voltage(unsigned mv)
{
}

/* we don't have the default DMA handlers */
void dma_event_interrupt_channel_3(void)
{
	if (STM32_DMA1_REGS->isr & STM32_DMA_ISR_TCIF(STM32_DMAC_CH3)) {
		dma_clear_isr(STM32_DMAC_CH3);
		task_wake(TASK_ID_CONSOLE);
	}
}
DECLARE_IRQ(STM32_IRQ_DMA_CHANNEL_2_3, dma_event_interrupt_channel_3, 3);

static int send_message(int polarity, uint16_t header,
			uint8_t cnt, const uint32_t *data)
{
	int bit_len;

	bit_len = prepare_message(0, header, cnt, data);
	/* Transmit the packet */
	pd_start_tx(0, polarity, bit_len);
	pd_tx_done(0, polarity);

	return bit_len;
}

static void twinkie_init(void)
{
	/* configure TX clock pins */
	gpio_config_module(MODULE_USB_PD, 1);
	/* Initialize physical layer */
	pd_hw_init(0);
}
DECLARE_HOOK(HOOK_INIT, twinkie_init, HOOK_PRIO_DEFAULT);

static int hex8tou32(char *str, uint32_t *val)
{
	char *ptr = str;
	uint32_t tmp = 0;

	while (*ptr) {
		char c = *ptr++;
		if (c >= '0' && c <= '9')
			tmp = (tmp << 4) + (c - '0');
		else if (c >= 'A' && c <= 'F')
			tmp = (tmp << 4) + (c - 'A' + 10);
		else if (c >= 'a' && c <= 'f')
			tmp = (tmp << 4) + (c - 'a' + 10);
		else
			return EC_ERROR_INVAL;
	}
	if (ptr != str + 8)
		return EC_ERROR_INVAL;
	*val = tmp;
	return EC_SUCCESS;
}

static int cmd_send(int argc, char **argv)
{
	int pol, cnt, i;
	uint16_t header;
	uint32_t data[VDO_MAX_SIZE-1];
	char *e;
	int bit_len;

	cnt = argc - 2;
	if (argc < 2 || cnt > VDO_MAX_SIZE)
		return EC_ERROR_PARAM_COUNT;

	pol = strtoi(argv[0], &e, 10) - 1;
	if (*e || pol > 1 || pol < 0)
		return EC_ERROR_PARAM2;
	header = strtoi(argv[1], &e, 16);
	if (*e)
		return EC_ERROR_PARAM3;

	for (i = 0; i < cnt; i++)
		if (hex8tou32(argv[i+2], data + i))
			return EC_ERROR_INVAL;

	bit_len = send_message(pol, header, cnt, data);
	ccprintf("Sent CC%d %04x + %d = %d\n", pol + 1, header, cnt, bit_len);

	return EC_SUCCESS;
}

static void set_resistor(int pol, char *name)
{
	/* Resistors GPIOs :
	 * CC1_RA       A8
	 * CC1_RPUSB    A13
	 * CC1_RP1A5    A14
	 * CC1_RP3A0    A15
	 * CC2_RPUSB    B0
	 * CC1_RD       B5
	 * CC2_RD       B8
	 * CC2_RA       B15
	 * CC2_RP1A5    C14
	 * CC2_RP3A0    C15
	 */
	int i;
	static const struct res_cfg {
		const char *name;
		struct config {
			uint32_t port;
			uint32_t mask;
			uint32_t flags;
		} cfgs[2];
	} res_cfg[] = {
		{"NONE"},
		{"RA", {{GPIO_A, 0x0100, GPIO_ODR_LOW},
			{GPIO_B, 0x8000, GPIO_ODR_LOW} } },
		{"RD", {{GPIO_B, 0x0020, GPIO_ODR_LOW},
			{GPIO_B, 0x0100, GPIO_ODR_LOW} } },
		{"RPUSB", {{GPIO_A, 0x2000, GPIO_OUT_HIGH},
			   {GPIO_B, 0x0001, GPIO_OUT_HIGH} } },
		{"RP1A5", {{GPIO_A, 0x4000, GPIO_OUT_HIGH},
			   {GPIO_C, 0x4000, GPIO_OUT_HIGH} } },
		{"RP3A0", {{GPIO_A, 0x8000, GPIO_OUT_HIGH},
			   {GPIO_C, 0x8000, GPIO_OUT_HIGH} } },
	};
	/* reset everything to high impedance */
	gpio_set_flags_by_mask(GPIO_A, 0xE100, GPIO_ODR_HIGH);
	gpio_set_flags_by_mask(GPIO_B, 0x8121, GPIO_ODR_HIGH);
	gpio_set_flags_by_mask(GPIO_C, 0xC000, GPIO_ODR_HIGH);
	/* connect the resistors */
	for (i = 0; i < ARRAY_SIZE(res_cfg); i++)
		if ((strcasecmp(res_cfg[i].name, name) == 0) &&
		    res_cfg[i].cfgs[pol].mask) {
			gpio_set_flags_by_mask(res_cfg[i].cfgs[pol].port,
						res_cfg[i].cfgs[pol].mask,
						res_cfg[i].cfgs[pol].flags);
			break;
		}
}

static int cmd_resistor(int argc, char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	set_resistor(0, argv[0]);
	set_resistor(1, argv[1]);

	return EC_SUCCESS;
}

static int cmd_tx_clock(int argc, char **argv)
{
	int freq;
	char *e;

	if (argc < 1)
		return EC_ERROR_PARAM2;

	freq = strtoi(argv[0], &e, 10);
	if (*e)
		return EC_ERROR_PARAM2;
	pd_set_clock(0, freq);
	ccprintf("TX frequency = %d Hz\n", freq);

	return EC_SUCCESS;
}

static int cmd_rx_threshold(int argc, char **argv)
{
	int mv;
	char *e;

	if (argc < 1)
		return EC_ERROR_PARAM2;

	mv = strtoi(argv[0], &e, 10);
	if (*e)
		return EC_ERROR_PARAM2;

	/* set DAC voltage (Vref = 3.3V) */
	STM32_DAC_DHR12RD = mv * 4096 / 3300;
	ccprintf("RX threshold = %d mV\n", mv);

	return EC_SUCCESS;
}

static int command_tw(int argc, char **argv)
{
	if (!strcasecmp(argv[1], "send"))
		return cmd_send(argc - 2, argv + 2);
	else if (!strncasecmp(argv[1], "resistor", 3))
		return cmd_resistor(argc - 2, argv + 2);
	else if (!strcasecmp(argv[1], "txclock"))
		return cmd_tx_clock(argc - 2, argv + 2);
	else if (!strncasecmp(argv[1], "rxthresh", 8))
		return cmd_rx_threshold(argc - 2, argv + 2);
	else
		return EC_ERROR_PARAM1;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(twinkie, command_tw,
			"[send|resistor|txclock|rxthresh]",
			"Manual Twinkie tweaking", NULL);
