/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for ISH30
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"

/*
 * ISH3.0 has 3 ports distributed among four controllers. Locking must
 * occur by-controller (not by-port).
 */
enum ish30_i2c_port {
	ISH30_I2C0   = 0,      /* Controller 0 */
	ISH30_I2C1   = 1,      /* Controller 1 */
	ISH30_I2C2   = 2,      /* Controller 2 */
	ISH30_I2C_PORT_COUNT,
};

#define HPET_BASE			0xFED00000
#define I2C0_BASE			0x00100000
#define I2C1_BASE			0x00102000
#define I2C2_BASE			0x00105000
#define IPC_BASE			0x00B00000
#define UART_BASE			0x00103000
#define IOAPIC_BASE			0xFEC00000

#define ISH30_I2C0_IRQ			0
#define ISH30_I2C1_IRQ			1
#define ISH30_HPET_TIMER0_IRQ		55
#define ISH30_HPET_TIMER1_IRQ		8
#define ISH30_IPC_IRQ_HOST2ISH		12
#define ISH30_IPC_IRQ_ISH2PEER0_CLR	24
#define ISH30_IPC_IRQ_PEER02ISH		12
#define ISH30_IPC_IRQ_PEER12ISH		12
#define ISH30_IPC_IRQ_ISH2PEER1_CLR	24
#define ISH30_UART0_IRQ			34
#define ISH30_UART1_IRQ			35
#define ISH30_I2C2_IRQ			40
#define ISH30_HPET_TIMER2_IRQ		11

#define PRIO_GROUP_SIZE			16
#define UART0_VEC               (PRIO_GROUP_SIZE * 6 + 1)
#define UART1_VEC               (PRIO_GROUP_SIZE * 5 + 0)
#define UART2_VEC               (PRIO_GROUP_SIZE * 5 + 1)
#define I2C2_VEC                (PRIO_GROUP_SIZE * 7 + 0)
#define I2C0_VEC                (PRIO_GROUP_SIZE * 9 + 0)
#define I2C1_VEC                (PRIO_GROUP_SIZE * 9 + 1)
#define DMA_VEC                 (PRIO_GROUP_SIZE * 11 + 0)
#define GPIO_VEC                (PRIO_GROUP_SIZE * 10 + 0)
#define IPC_VEC                 (PRIO_GROUP_SIZE * 12 + 0)
#define HPET_TIMER0_VEC         (PRIO_GROUP_SIZE * 14 + 0)
#define HPET_TIMER12_VEC        (PRIO_GROUP_SIZE * 14 + 1)

/* ---- IPC_Registers ---- */
#define IPC_PISR                       (IPC_BASE + 0x0)
#define IPC_PIMR                       (IPC_BASE + 0x4)

#define IPC_ISH2PEER0_MSG_REGS         (IPC_BASE + 0x60)
#define IPC_ISH_MINIMA_FWSTS           (IPC_BASE + 0x34)
#define IPC_PEER02ISH_DOORBELL         (IPC_BASE + 0x48)
#define IPC_PEER02ISH_MSG_REGS         (IPC_BASE + 0xE0)
#define IPC_ISH2PEER0_DOORBELL         (IPC_BASE + 0x54)
#define IPC_PEER12ISH_MSG_REGS         (IPC_BASE + 0xE0)
#define IPC_PEER12ISH_DOORBELL         (IPC_BASE + 0x48)
#define IPC_ISH2PEER1_MSG_REGS         (IPC_BASE + 0x60)
#define IPC_ISH2PEER1_DOORBELL         (IPC_BASE + 0x54)
#define IPC_BUSY_CLEAR                 (IPC_BASE + 0x378)

#define IOAPIC_IDX			0xFEC00000
#define IOAPIC_WDW			0xFEC00010
#define IOAPIC_EOI			0xFEC00040

#define IOAPIC_VERSION                  0x1

#define IOAPIC_IOREDTBL                 0x10
#define IOAPIC_REDTBL_DELMOD_FIXED      0x00000000
#define IOAPIC_REDTBL_DESTMOD_PHYS      0x00000000
#define IOAPIC_REDTBL_INTPOL_HIGH       0x00000000
#define IOAPIC_REDTBL_INTPOL_LOW        0x00002000
#define IOAPIC_REDTBL_TRIGGER_EDGE      0x00000000
#define IOAPIC_REDTBL_TRIGGER_LEVEL     0x00008000
#define IOAPIC_REDTBL_MASK              0x00010000

#define LAPIC_BASE                      0xFEE00000
#define LAPIC_EOI                       0xFEE000B0
#define LAPIC_ISR			0xFEE00170

/* Wake pin definitions, defined at board-level */
extern const enum gpio_signal hibernate_wake_pins[];
extern const int hibernate_wake_pins_used;

/*
 * Optional board-level function to get hibernate GPIO state.
 * Returns desired GPIO state in hibernate, or 0 to skip reconfiguration.
 */
uint32_t board_get_gpio_hibernate_state(uint32_t port, uint32_t pin)
	__attribute__((weak));

#endif /* __CROS_EC_REGISTERS_H */
