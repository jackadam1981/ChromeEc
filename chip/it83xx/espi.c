/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ESPI module for Chrome EC */

#include "console.h"
#include "espi.h"
#include "hooks.h"
#include "port80.h"
#include "power.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#define CHIP_ESPI_VW_INTERRUPT_NUM 8

/* Console output macros */
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)

struct vw_channel_t {
	uint8_t  index;         /* VW index of signal */
	uint8_t  level_mask;    /* level bit of signal */
	uint8_t  valid_mask;    /* valid bit of signal */
};

/* VW settings at initialization */
static const struct vw_channel_t vw_init_setting[] = {
	/*
	 * index 04h
	 * bit 0 / 4:     VW_OOB_RST_ACK
	 * bit 2 / 6:     VW_WAKE_L
	 * bit 3 / 7:     VW_PME_L
	 */
	{0x04, 0x0, (0x1 << 4)},
	/*
	 * index 05h
	 * bit 1 / 5:     VW_ERROR_FATAL
	 * bit 2 / 6:     VW_ERROR_NON_FATAL
	 * bit 0,3 / 4,7: VW_SLAVE_BTLD_STATUS_DONE
	 */
	{0x05, 0x9, (0x9 << 4)},
	/*
	 * index 40h
	 * bit 0 / 4:     VW_SUS_ACK
	 */
	{0x40, 0x0, (0x1 << 4)},
};

/* VW settings at host startup */
static const struct vw_channel_t vw_host_startup_setting[] = {
	/*
	 * index 06h
	 * bit 0 / 4:     VW_SCI_L
	 * bit 1 / 5:     VW_SMI_L
	 * bit 2 / 6:     VW_RCIN_L
	 * bit 3 / 7:     VW_HOST_RST_ACK
	 */
	{0x06, 0xf, (0xf << 4)},
};

#define VW_CHAN(name, idx, level, valid) \
	[(name - VW_SIGNAL_BASE - 1)] = {idx, level, valid}

/* VW signals used in eSPI (NOTE: must match order of enum espi_vw_signal). */
static const struct vw_channel_t vw_channel_list[] = {
	/* index 02h: master to slave. */
	VW_CHAN(VW_SLP_S3_L,               0x02, (1 << 0), (1 << 4)),
	VW_CHAN(VW_SLP_S4_L,               0x02, (1 << 1), (1 << 5)),
	VW_CHAN(VW_SLP_S5_L,               0x02, (1 << 2), (1 << 6)),
	/* index 03h: master to slave. */
	VW_CHAN(VW_SUS_STAT_L,             0x03, (1 << 0), (1 << 4)),
	VW_CHAN(VW_PLTRST_L,               0x03, (1 << 1), (1 << 5)),
	VW_CHAN(VW_OOB_RST_WARN,           0x03, (1 << 2), (1 << 6)),
	/* index 04h: slave to master. */
	VW_CHAN(VW_OOB_RST_ACK,            0x04, (1 << 0), (1 << 4)),
	VW_CHAN(VW_WAKE_L,                 0x04, (1 << 2), (1 << 6)),
	VW_CHAN(VW_PME_L,                  0x04, (1 << 3), (1 << 7)),
	/* index 05h: slave to master. */
	VW_CHAN(VW_ERROR_FATAL,            0x05, (1 << 1), (1 << 5)),
	VW_CHAN(VW_ERROR_NON_FATAL,        0x05, (1 << 2), (1 << 6)),
	VW_CHAN(VW_SLAVE_BTLD_STATUS_DONE, 0x05, 0x9,      0x90),
	/* index 06h: slave to master. */
	VW_CHAN(VW_SCI_L,                  0x06, (1 << 0), (1 << 4)),
	VW_CHAN(VW_SMI_L,                  0x06, (1 << 1), (1 << 5)),
	VW_CHAN(VW_RCIN_L,                 0x06, (1 << 2), (1 << 6)),
	VW_CHAN(VW_HOST_RST_ACK,           0x06, (1 << 3), (1 << 7)),
	/* index 07h: master to slave. */
	VW_CHAN(VW_HOST_RST_WARN,          0x07, (1 << 0), (1 << 4)),
	/* index 40h: slave to master. */
	VW_CHAN(VW_SUS_ACK,                0x40, (1 << 0), (1 << 4)),
	/* index 41h: master to slave. */
	VW_CHAN(VW_SUS_WARN_L,             0x41, (1 << 0), (1 << 4)),
	VW_CHAN(VW_SUS_PWRDN_ACK_L,        0x41, (1 << 1), (1 << 5)),
	VW_CHAN(VW_SLP_A_L,                0x41, (1 << 3), (1 << 7)),
	/* index 42h: master to slave. */
	VW_CHAN(VW_SLP_LAN,                0x42, (1 << 0), (1 << 4)),
	VW_CHAN(VW_SLP_WLAN,               0x42, (1 << 1), (1 << 5)),
};
BUILD_ASSERT(ARRAY_SIZE(vw_channel_list) ==
		(VW_SIGNAL_BASE_END - VW_SIGNAL_BASE - 1));

/* Get vw index & value information by signal */
static int espi_vw_get_signal_index(enum espi_vw_signal event)
{
	uint32_t i = event - VW_SIGNAL_BASE - 1;

	return (i < ARRAY_SIZE(vw_channel_list)) ? i : -1;
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
	uint32_t int_mask;

	if (i < 0)
		return EC_ERROR_PARAM1;

	int_mask = get_int_mask();
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
		return 0;

	/* Not valid */
	if (!(IT83XX_ESPI_VWIDX(vw_channel_list[i].index) &
		vw_channel_list[i].valid_mask))
		return 0;

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
	/*
	 * Common code calls this function to enable VW interrupt of power
	 * sequence signal.
	 * IT83xx only use a bit (bit7@IT83XX_ESPI_VWCTRL0) to enable VW
	 * interrupt.
	 * VW interrupt will be triggerd with any updated VW index flag
	 * if this control bit is set.
	 * So we will alwasy returns success here.
	 */
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
	/*
	 * We can't disable VW interrupt of power sequence signal
	 * individually.
	 */
	return EC_ERROR_UNIMPLEMENTED;
}

static void espi_vw_host_startup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vw_host_startup_setting); i++)
		IT83XX_ESPI_VWIDX(vw_host_startup_setting[i].index) =
			(vw_host_startup_setting[i].level_mask |
			vw_host_startup_setting[i].valid_mask);
}

static void espi_vw_idx47_isr(uint8_t flag_changed)
{
}

static void espi_vw_idx44_isr(uint8_t flag_changed)
{
}

static void espi_vw_idx43_isr(uint8_t flag_changed)
{
}

static void espi_vw_idx42_isr(uint8_t flag_changed)
{
}

static void espi_vw_idx41_isr(uint8_t flag_changed)
{
	if (flag_changed & (1 << 0))
		espi_vw_set_wire(VW_SUS_ACK, espi_vw_get_wire(VW_SUS_WARN_L));
}

static void espi_vw_idx7_isr(uint8_t flag_changed)
{
	if (flag_changed & (1 << 0))
		espi_vw_set_wire(VW_HOST_RST_ACK,
			espi_vw_get_wire(VW_HOST_RST_WARN));
}

static void espi_vw_idx3_isr(uint8_t flag_changed)
{
	if (flag_changed & (1 << 1)) {
		int pltrst = espi_vw_get_wire(VW_PLTRST_L);

		if (pltrst)
			espi_vw_host_startup();
		else
			/* Store port 80 reset event */
			port_80_write(PORT_80_EVENT_RESET);

		CPRINTS("PLTRST_L %sasserted", pltrst ? "de" : "");
	}

	if (flag_changed & (1 << 2))
		espi_vw_set_wire(VW_OOB_RST_ACK,
			espi_vw_get_wire(VW_OOB_RST_WARN));
}

static void espi_vw_idx2_isr(uint8_t flag_changed)
{
	if (flag_changed & (1 << 0))
		power_signal_interrupt(VW_SLP_S3_L);
	if (flag_changed & (1 << 1))
		power_signal_interrupt(VW_SLP_S4_L);
	if (flag_changed & (1 << 2))
		power_signal_interrupt(VW_SLP_S5_L);
}

struct vw_interrupt_t {
	void (*vw_isr)(uint8_t flag_changed);
	uint8_t vw_index;
};

static const struct vw_interrupt_t vw_isr_list[CHIP_ESPI_VW_INTERRUPT_NUM] = {
	{espi_vw_idx2_isr,  0x02},
	{espi_vw_idx3_isr,  0x03},
	{espi_vw_idx7_isr,  0x07},
	{espi_vw_idx41_isr, 0x41},
	{espi_vw_idx42_isr, 0x42},
	{espi_vw_idx43_isr, 0x43},
	{espi_vw_idx44_isr, 0x44},
	{espi_vw_idx47_isr, 0x47},
};
static uint8_t vw_index_flag[CHIP_ESPI_VW_INTERRUPT_NUM];

void espi_vw_interrupt(void)
{
	int i, vw_idx;
	uint8_t idx_flag;
	uint8_t vwidx_updated = IT83XX_ESPI_VWCTRL1;

	/*
	 * TODO: write-1 clear bug.
	 * for now, we have to write 0xff to clear pending bit.
	 */
#if 0
	IT83XX_ESPI_VWCTRL1 = vwidx_updated;
#else
	IT83XX_ESPI_VWCTRL1 = 0xff;
#endif
	task_clear_pending_irq(IT83XX_IRQ_ESPI_VW);

	for (i = 0; i < CHIP_ESPI_VW_INTERRUPT_NUM; i++) {
		if (vwidx_updated & (1 << i)) {
			vw_idx = vw_isr_list[i].vw_index;
			idx_flag = IT83XX_ESPI_VWIDX(vw_idx);
			vw_isr_list[i].vw_isr(vw_index_flag[i] ^ idx_flag);
			vw_index_flag[i] = idx_flag;
		}
	}
}

void espi_interrupt(void)
{
}

void espi_init(void)
{
	int i;

	/* TODO: PLL change won't success if eSPI chip select is low. */
#if (PLL_CLOCK != 48000000)
#error "Not support PLL change if eSPI module is enabled. "
#endif

	for (i = 0; i < ARRAY_SIZE(vw_init_setting); i++)
		IT83XX_ESPI_VWIDX(vw_init_setting[i].index) =
			(vw_init_setting[i].level_mask |
			vw_init_setting[i].valid_mask);

	for (i = 0; i < CHIP_ESPI_VW_INTERRUPT_NUM; i++)
		vw_index_flag[i] = IT83XX_ESPI_VWIDX(vw_isr_list[i].vw_index);

	task_clear_pending_irq(IT83XX_IRQ_ESPI_VW);
	/* bit7: VW interrupt enable */
	IT83XX_ESPI_VWCTRL0 |= (1 << 7);
	task_enable_irq(IT83XX_IRQ_ESPI_VW);
}
