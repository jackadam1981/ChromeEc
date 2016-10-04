/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ESPI module for Chrome EC */

#include "console.h"
#include "espi.h"
#include "hooks.h"
#include "power.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)

struct vw_interrupt_t {
	void (*vw_isr)(void);
	uint8_t vw_index;
};

struct vw_channel_t {
	uint16_t name;       /* Name of signal */
	uint8_t  index;      /* VW index of signal */
	uint8_t  level_mask; /* level bit of signal */
	uint8_t  valid_mask; /* valid bit of signal */
};

/* VW signals used in eSPI */
static const struct vw_channel_t vw_channel_list[] = {
	/* index 02h: master to slave. */
	{VW_SLP_S3_L,               0x02, (1 << 0), (1 << 4)},
	{VW_SLP_S4_L,               0x02, (1 << 1), (1 << 5)},
	{VW_SLP_S5_L,               0x02, (1 << 2), (1 << 6)},
	/* index 03h: master to slave. */
	{VW_SUS_STAT_L,             0x03, (1 << 0), (1 << 4)},
	{VW_PLTRST_L,               0x03, (1 << 1), (1 << 5)},
	{VW_OOB_RST_WARN,           0x03, (1 << 2), (1 << 6)},
	/* index 04h: slave to master. */
	{VW_OOB_RST_ACK,            0x04, (1 << 0), (1 << 4)},
	{VW_WAKE_L,                 0x04, (1 << 2), (1 << 6)},
	{VW_PME_L,                  0x04, (1 << 3), (1 << 7)},
	/* index 05h: slave to master. */
	{VW_ERROR_FATAL,            0x05, (1 << 1), (1 << 5)},
	{VW_ERROR_NON_FATAL,        0x05, (1 << 2), (1 << 6)},
	{VW_SLAVE_BTLD_STATUS_DONE, 0x05, 0x9, 0x90},
	/* index 06h: slave to master. */
	{VW_SCI_L,                  0x06, (1 << 0), (1 << 4)},
	{VW_SMI_L,                  0x06, (1 << 1), (1 << 5)},
	{VW_RCIN_L,                 0x06, (1 << 2), (1 << 6)},
	{VW_HOST_RST_ACK,           0x06, (1 << 3), (1 << 7)},
	/* index 07h: master to slave. */
	{VW_HOST_RST_WARN,          0x07, (1 << 0), (1 << 4)},
	/* TODO: not check yet */
	{VW_SUS_ACK,                0x40, (1 << 0), (1 << 4)},
	{VW_SUS_WARN_L,             0x41, (1 << 0), (1 << 4)},
	{VW_SUS_PWRDN_ACK_L,        0x41, (1 << 1), (1 << 5)},
	{VW_SLP_A_L,                0x41, (1 << 3), (1 << 7)},
	{VW_SLP_LAN,                0x42, (1 << 0), (1 << 4)},
	{VW_SLP_WLAN,               0x42, (1 << 1), (1 << 5)},
};
BUILD_ASSERT(ARRAY_SIZE(vw_channel_list) ==
		(VW_SIGNAL_BASE_END - VW_SIGNAL_BASE - 1));

/* Get vw index & value information by signal */
static int espi_vw_get_signal_index(enum espi_vw_signal event)
{
	int i;

	/* Find the vw index by signal name */
	for (i = 0; i < ARRAY_SIZE(vw_channel_list); i++) {
		if (vw_channel_list[i].name == event)
			return i;
	}
	/* Cannot find index by signal name */
	return -1;
}

static int espi_vw_set_valid(enum espi_vw_signal signal, uint8_t valid)
{
	/* Get index of vw signal list by signale name */
	int i = espi_vw_get_signal_index(signal);
	uint32_t int_mask = get_int_mask();

	if (i < 0)
		return EC_ERROR_PARAM1;

	/* critical section with interrupts off */
	interrupt_disable();
	if (valid)
		IT83XX_ESPI_VWIDX(vw_channel_list[i].index) |=
			vw_channel_list[i].valid_mask;
	else
		IT83XX_ESPI_VWIDX(vw_channel_list[i].index) &=
			~vw_channel_list[i].valid_mask;
	/* restore interrupts */
	set_int_mask(int_mask);

	return EC_SUCCESS;
}

/**
 * Set eSPI Virtual-Wire signal to Host
 *
 * @param signal vw signal needs to set
 * @param level  level of vw signal
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_set_wire(enum espi_vw_signal signal, uint8_t level)
{
	/* Get index of vw signal list by signale name */
	int i = espi_vw_get_signal_index(signal);
	uint32_t int_mask = get_int_mask();

	if (i < 0)
		return EC_ERROR_PARAM1;

	/* critical section with interrupts off */
	interrupt_disable();
	if (level)
		IT83XX_ESPI_VWIDX(vw_channel_list[i].index) |=
			vw_channel_list[i].level_mask;
	else
		IT83XX_ESPI_VWIDX(vw_channel_list[i].index) &=
			~vw_channel_list[i].level_mask;
	/* restore interrupts */
	set_int_mask(int_mask);

	return EC_SUCCESS;
}

/**
 * Get eSPI Virtual-Wire signal from host
 *
 * @param signal vw signal needs to get
 * @return      1: set by host, otherwise: no signal
 */
int espi_vw_get_wire(enum espi_vw_signal signal)
{
	/* Get index of vw signal list by signale name */
	int i = espi_vw_get_signal_index(signal);

	if (i < 0)
		return EC_ERROR_PARAM1;

	/* Not valid */
	if (!(IT83XX_ESPI_VWIDX(vw_channel_list[i].index) |
		vw_channel_list[i].valid_mask))
		return EC_ERROR_INVAL;

	return !!(IT83XX_ESPI_VWIDX(vw_channel_list[i].index) &
		vw_channel_list[i].level_mask);
}

/**
 * Enable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to enable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_enable_wire_int(enum espi_vw_signal signal)
{
	return EC_SUCCESS;
}

/**
 * Disable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to disable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_disable_wire_int(enum espi_vw_signal signal)
{
	return EC_SUCCESS;
}

static void espi_vw_idx47_isr(void)
{
}

static void espi_vw_idx44_isr(void)
{
}

static void espi_vw_idx43_isr(void)
{
}

static void espi_vw_idx42_isr(void)
{
}

static void espi_vw_idx41_isr(void)
{
	espi_vw_set_valid(VW_SUS_ACK, 1);
	espi_vw_set_wire(VW_SUS_ACK, espi_vw_get_wire(VW_SUS_WARN_L));
}

static void espi_vw_idx7_isr(void)
{
	espi_vw_set_valid(VW_HOST_RST_ACK, 1);
	espi_vw_set_wire(VW_HOST_RST_ACK, espi_vw_get_wire(VW_HOST_RST_WARN));
}

static void espi_vw_idx3_isr(void)
{
	espi_vw_set_wire(VW_OOB_RST_ACK, espi_vw_get_wire(VW_OOB_RST_WARN));
}

static void espi_vw_idx2_isr(void)
{
	power_signal_interrupt(VW_SLP_S3_L);
	power_signal_interrupt(VW_SLP_S4_L);
	power_signal_interrupt(VW_SLP_S5_L);
}

static const struct vw_interrupt_t vw_isr_list[] = {
	{espi_vw_idx2_isr,  0x02},
	{espi_vw_idx3_isr,  0x03},
	{espi_vw_idx7_isr,  0x07},
	{espi_vw_idx41_isr, 0x41},
	{espi_vw_idx42_isr, 0x42},
	{espi_vw_idx43_isr, 0x43},
	{espi_vw_idx44_isr, 0x44},
	{espi_vw_idx47_isr, 0x47},
};

void espi_vw_interrupt(void)
{
	int i;
	uint8_t vwidx_updated = IT83XX_ESPI_VWCTRL1;

	uint8_t value __attribute__ ((unused)) = 0;

	/*
	 * TODO: bug of write-1 to clear.
	 * for now, we have to write 0xff to clear pending bit.
	 */
#if 0
	IT83XX_ESPI_VWCTRL1 = vwidx_updated;
#else
	IT83XX_ESPI_VWCTRL1 = 0xff;
#endif
	task_clear_pending_irq(IT83XX_IRQ_ESPI_VW);

	for (i = 0; i < ARRAY_SIZE(vw_isr_list); i++) {
		if (vwidx_updated & (1 << i)) {
			vw_isr_list[i].vw_isr();
			value = IT83XX_ESPI_VWIDX(vw_isr_list[i].vw_index);
		}
	}
}

void espi_interrupt(void)
{
}

void espi_init(void)
{
#if (PLL_CLOCK < 48000000)
#error "The PLL frequency can't less than 32.3MHz to support eSPI. "
#endif
	/* valid and level high */
	espi_vw_set_valid(VW_SLAVE_BTLD_STATUS_DONE, 1);
	espi_vw_set_wire(VW_SLAVE_BTLD_STATUS_DONE, 1);

	task_clear_pending_irq(IT83XX_IRQ_ESPI_VW);
	/* bit7: VW interrupt enable */
	IT83XX_ESPI_VWCTRL0 |= (1 << 7);
	task_enable_irq(IT83XX_IRQ_ESPI_VW);
}
