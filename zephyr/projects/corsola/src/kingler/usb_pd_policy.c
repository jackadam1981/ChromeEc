/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/uart.h>
#include <kernel.h>

#include "charge_manager.h"
#include "console.h"
#include "driver/ppc/rt1718s.h"
#include "system.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#include "baseboard_usbc_config.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ##args)

void pd_power_supply_reset(int port)
{
	int prev_en;

	prev_en = ppc_is_sourcing_vbus(port);

	if (port == USBC_PORT_C1) {
		rt1718s_gpio_set_level(port, GPIO_EN_USB_C1_SOURCE, 0);
	}

	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en) {
		pd_set_vbus_discharge(port, 1);
	}

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	/* Disable charging. */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv) {
		return rv;
	}

	pd_set_vbus_discharge(port, 0);

	/* Provide Vbus. */
	if (port == USBC_PORT_C1) {
		rt1718s_gpio_set_level(port, GPIO_EN_USB_C1_SOURCE, 1);
	}

	rv = ppc_vbus_source_enable(port, 1);
	if (rv) {
		return rv;
	}

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}

int pd_snk_is_vbus_provided(int port)
{
	/* TODO: use ADC? */
	return tcpm_check_vbus_level(port, VBUS_PRESENT);
}

/*
 * reg table:
 * CR_UART3 = 400E 4000h
 *
 * UMDSL
 * ERD bit(5)
 * ETD bit(4)
 *
 *
 * MDMA3 = 4001 1300h
 */
static uint8_t recv_buf[8];

static const struct device* uart = DEVICE_DT_GET(DT_NODELABEL(uart3));

struct uart_npcx_config {
	struct uart_reg *inst;
};

struct mdma_reg {
	volatile uint32_t CTL;
	volatile uint32_t SRCB;
	volatile uint32_t DSTB;
	volatile uint32_t TCNT;
	volatile uint32_t unused;
	volatile uint32_t CDST;
	volatile uint32_t CTCNT;
};

/* receive dma */
static volatile struct mdma_reg *const mdma3_ch0 = (void*)(0x40011300);

/* transmit dma */
/*
static volatile struct mdma_reg *const mdma3_ch1 = (void*)(0x40011320);
*/

static int command_uart_test(int argc, char **argv)
{
	memset(recv_buf, 0x56, sizeof(recv_buf));

	mdma3_ch0->CTL |= BIT(0);

	uart_poll_out(uart, 't');
	uart_poll_out(uart, 'e');
	uart_poll_out(uart, 's');
	uart_poll_out(uart, 't');

	usleep(10 * MSEC);
	CPRINTS(" %d %d %d %d %d %d %d %d", recv_buf[0], recv_buf[1], recv_buf[2], recv_buf[3],
			recv_buf[4], recv_buf[5], recv_buf[6], recv_buf[7]);

	CPRINTS(" CTL: %08x", mdma3_ch0->CTL);
	CPRINTS(" DSTB: %08x", mdma3_ch0->DSTB);
	CPRINTS(" TCNT: %08x", mdma3_ch0->TCNT);
	CPRINTS(" CDST: %08x", mdma3_ch0->CDST);
	CPRINTS(" CTCNT: %08x", mdma3_ch0->CTCNT);

	return 0;
}
DECLARE_CONSOLE_COMMAND(a, command_uart_test, NULL, "");

static int kingler_uart_init(const struct device *unused)
{
	const struct uart_npcx_config *const config = uart->config;
	struct uart_reg *const inst = config->inst;
	volatile uint8_t *pwdwn_ctl9 = (void*)0x4000D026;

	*pwdwn_ctl9 &= ~BIT(2);

	mdma3_ch0->DSTB = (uintptr_t)recv_buf;
	mdma3_ch0->TCNT = sizeof(recv_buf);

	inst->UMDSL |= BIT(5);

	CPRINTS("\x1b[1;31m%s done\x1b[m", __func__);

	return 0;
}
SYS_INIT(kingler_uart_init, APPLICATION, 1);
