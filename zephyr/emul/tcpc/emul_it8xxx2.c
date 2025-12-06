/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Emulation of the IT8XXX2 USB Power Delivery TCPM/PHY.
 *
 * This file provides a software model for the IT83XX/IT8XXX2 PD registers
 * and behaviors, allowing the higher-level TCPM driver to be tested in a
 * host environment without the actual hardware.
 */

#include "console.h"
#include "driver/tcpm/it83xx_pd.h"
#include "ec_commands.h"
#include "emul/tcpc/emul_it8xxx2.h" /* Header for emulation-specific functions */
#include "emul/tcpc/emul_tcpci.h"
#include "host_command.h"
#include "tcpm/tcpci.h"
#include "test_util.h"

#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#ifdef CONFIG_USB_PD_TCPM_DRIVER_IT8XXX2

/* --- Emulated Register Storage --- */

/*
 * The IT8XXX2 PD registers are mapped starting at 0x00F03700.
 * We emulate a block for each port (0x100 separation).
 * We will define a structure to hold the state for a single port.
 */
#define IT8XXX2_REG_COUNT 0x100
#define MAX_EMUL_PORTS 2 /* Emulate up to two ports (P0 and P1) */

struct it8xxx2_pd_state {
	/* Array to hold all 0x100 bytes of PD registers for this port */
	uint8_t regs[IT8XXX2_REG_COUNT];
	/* Current status of the CC pins (simulated) */
	enum tcpc_cc_voltage_state cc1_state;
	enum tcpc_cc_voltage_state cc2_state;
	/* Last Hard Reset or Cable Reset sent/received */
	int last_reset_type;
};

static struct it8xxx2_pd_state pd_emul_state[MAX_EMUL_PORTS];

/* --- Emulated Register Access (Simulating REG8/REG16/REG32 macros) --- */

/*
 * Helper function to get a pointer to the emulated register byte.
 * Maps the physical register address used by the driver to the emulated array.
 */
static uint8_t *get_emul_reg_ptr(int port, int offset)
{
	uint32_t base_addr = IT83XX_USBPD_BASE(port);
	uint32_t reg_addr = base_addr + offset;

	if (port < 0 || port >= MAX_EMUL_PORTS)
		return NULL;

	/* Calculate the offset within the 0x100 byte block */
	if (offset >= IT8XXX2_REG_COUNT)
		return NULL;

	return &pd_emul_state[port].regs[offset];
}

/*
 * Macro to simulate hardware register read/write for an 8-bit register.
 * This is where the emul_it8xxx2.h/emul_it8xxx2.c would hook into
 * the driver's REG8/IT83XX_USBPD_BASE definitions.
 * In a host build, these macros are often redefined.
 */
#undef REG8
#define REG8(addr)                                       \
	(*get_emul_reg_ptr(IT8XXX2_PORT_FROM_ADDR(addr), \
			   IT8XXX2_OFFSET_FROM_ADDR(addr)))

/* implementations for the driver's port/offset helpers */
#define IT8XXX2_PORT_FROM_ADDR(addr) (((addr) - 0x00F03700) / 0x100)
#define IT8XXX2_OFFSET_FROM_ADDR(addr) (((addr) - 0x00F03700) % 0x100)

/* --- Emulated TCPM/PHY Functions --- */

/*
 * The driver code uses a lot of macros like USBPD_SW_RESET(port).
 * We need to simulate the effect of writing to the underlying register.
 */
static void pd_sw_reset(int port)
{
	/* Simulate writing USBPD_REG_MASK_SW_RESET_BIT to IT83XX_USBPD_PDGCR */
	uint8_t *reg = get_emul_reg_ptr(port, 0x00);

	if (!reg)
		return;

	if (*reg & USBPD_REG_MASK_SW_RESET_BIT) {
		/* Reset triggered: Clear all state/registers except power-on
		 * defaults */
		CPRINTF("IT8XXX2 EMUL: Port %d SW Reset.\n", port);
		memset(pd_emul_state[port].regs, 0, IT8XXX2_REG_COUNT);
		/* The SW_RESET_BIT should clear itself after the reset is
		 * complete */
		CLEAR_MASK(*reg, USBPD_REG_MASK_SW_RESET_BIT);
	}
}

/*
 * Emulates the transmit start command.
 * This should eventually set USBPD_REG_MASK_MSG_TX_DONE in ISR.
 */
static void pd_kick_tx_start(int port)
{
	uint8_t *mtcr = get_emul_reg_ptr(port, 0x18);
	uint8_t *isr = get_emul_reg_ptr(port, 0x14);

	if (!mtcr || !isr)
		return;

	if (*mtcr & USBPD_REG_MASK_TX_START) {
		/* Simulate immediate message transmission success */
		CPRINTF("IT8XXX2 EMUL: Port %d TX Started.\n", port);

		/* 1. Clear TX_START */
		CLEAR_MASK(*mtcr, USBPD_REG_MASK_TX_START);

		/* 2. Set TX_DONE interrupt status bit */
		SET_MASK(*isr, USBPD_REG_MASK_MSG_TX_DONE);

		/* 3. The test harness should call the appropriate interrupt
		 * handler */
		/* The actual interrupt call mechanism is environment specific
		 * (e.g., host_send_host_command) */
		emul_it8xxx2_trigger_irq(port);
	}
}

/* --- Public Emulation Interface Functions --- */

/**
 * @brief Initialize the emulated PD controller.
 */
void emul_it8xxx2_init(void)
{
	int i;
	for (i = 0; i < MAX_EMUL_PORTS; i++) {
		/* Clear all registers to zero (power-on state) */
		memset(pd_emul_state[i].regs, 0, IT8XXX2_REG_COUNT);
		pd_emul_state[i].last_reset_type = 0;

		/* Set initial register defaults if necessary (e.g., enable
		 * global) */
		/* For simplicity, we keep them all zero initially */
	}
}

/**
 * @brief Main emulation loop/hook for register access.
 *
 * The driver calls these macros which redirect to our emulated registers.
 * We use this function to check for side effects on writes.
 */
void emul_it8xxx2_check_write(int port, int offset, uint8_t value)
{
	if (port < 0 || port >= MAX_EMUL_PORTS)
		return;

	switch (offset) {
	case 0x00: /* IT83XX_USBPD_PDGCR */
		if (value & USBPD_REG_MASK_SW_RESET_BIT)
			pd_sw_reset(port);
		break;
	case 0x18: /* IT83XX_USBPD_MTCR */
		if (value & USBPD_REG_MASK_TX_START)
			pd_kick_tx_start(port);
		if (value & USBPD_REG_MASK_SEND_HW_RESET) {
			pd_emul_state[port].last_reset_type = 1;
			/* Simulate immediate Hard Reset TX done */
			uint8_t *isr = get_emul_reg_ptr(port, 0x14);
			if (isr) {
				SET_MASK(*isr,
					 USBPD_REG_MASK_HARD_RESET_TX_DONE);
				emul_it8xxx2_trigger_irq(port);
			}
		}
		break;
	case 0x67: /* IT83XX_USBPD_TCDCR - Type-C Detect Control */
		/* Simulate state change based on PLUG_IN_OUT_DETECT_DISABLE */
		break;
	/* Add more register side-effect checks here */
	default:
		break;
	}
}

/**
 * @brief Sets the simulated CC line voltage state (called by test code).
 *
 * @param port The port index.
 * @param cc1_state New state for CC1.
 * @param cc2_state New state for CC2.
 */
void emul_it8xxx2_set_cc_state(int port, enum tcpc_cc_voltage_state cc1_state,
			       enum tcpc_cc_voltage_state cc2_state)
{
	uint8_t *srcvcrr = get_emul_reg_ptr(port, 0x08);
	uint8_t *snkvcrr = get_emul_reg_ptr(port, 0x09);
	uint8_t *isr = get_emul_reg_ptr(port, 0x14);

	if (!srcvcrr || !snkvcrr || !isr)
		return;

	pd_emul_state[port].cc1_state = cc1_state;
	pd_emul_state[port].cc2_state = cc2_state;

	/*
	 * In a real EC, the hardware updates SRCVCRR and SNKVCRR based on the
	 * physical CC voltage. We must simulate this logic here.
	 *
	 * For simplicity, we'll only simulate CC Detect and trigger the ISR.
	 */

	/* Clear previous CC detect interrupt status (IT83XX does not have it)
	 */

	/* Simulate a change in connection status */
	if (cc1_state != TCPC_CC_VOLT_OPEN || cc2_state != TCPC_CC_VOLT_OPEN) {
		/* A change from open to anything else is a "Plug In" event */
		/* The TCDCR register logic is complex, but we'll set the plug
		 * status */
		uint8_t *tcdcr = get_emul_reg_ptr(port, 0x67);
		if (tcdcr) {
			SET_MASK(*tcdcr, USBPD_REG_PLUG_IN_OUT_DETECT_STAT);
			CLEAR_MASK(*tcdcr, USBPD_REG_PLUG_OUT_SELECT);
		}

		/* Trigger an interrupt to notify the PD task */
		/* IT83XX_USBPD_TCDCR is usually the source for this */
		emul_it8xxx2_trigger_irq(port);
	}
}

#endif /* CONFIG_USB_PD_TCPM_DRIVER_IT8XXX2 */
