/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System module for Chrome EC : MEC17XX hardware specific implementation */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "gpio.h"
#include "host_command.h"
#include "registers.h"
#include "shared_mem.h"
#include "system.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "spi.h"
#include "lpc_chip.h"


#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)


/* Indices for hibernate data registers (RAM backed by VBAT) */
enum hibdata_index {
	HIBDATA_INDEX_SCRATCHPAD = 0,    /* General-purpose scratchpad */
	HIBDATA_INDEX_SAVED_RESET_FLAGS  /* Saved reset flags */
};

static void check_reset_cause(void)
{
	uint32_t status = MEC17XX_VBAT_STS;
	uint32_t flags = 0;
	uint32_t rst_sts = MEC17XX_PCR_PWR_RST_STS &
				(MEC17XX_PWR_RST_STS_VTR |
				MEC17XX_PWR_RST_STS_VBAT);

	/* Clear the reset causes now that we've read them */
	MEC17XX_VBAT_STS |= status;
	MEC17XX_PCR_PWR_RST_STS |= rst_sts;

	/*
	* BIT[6] determine VTR reset
	*/
	if (rst_sts & MEC17XX_PWR_RST_STS_VTR)
		flags |= RESET_FLAG_RESET_PIN;


	flags |= MEC17XX_VBAT_RAM(HIBDATA_INDEX_SAVED_RESET_FLAGS);
	MEC17XX_VBAT_RAM(HIBDATA_INDEX_SAVED_RESET_FLAGS) = 0;

	if ((status & MEC17XX_VBAT_STS_WDT) && !(flags & (RESET_FLAG_SOFT |
					    RESET_FLAG_HARD |
					    RESET_FLAG_HIBERNATE)))
		flags |= RESET_FLAG_WATCHDOG;

	system_set_reset_flags(flags);
}

int system_is_reboot_warm(void)
{
	uint32_t reset_flags;
	/*
	* Check reset cause here,
	* gpio_pre_init is executed faster than system_pre_init
	*/
	check_reset_cause();
	reset_flags = system_get_reset_flags();

	if ((reset_flags & RESET_FLAG_RESET_PIN) ||
		(reset_flags & RESET_FLAG_POWER_ON) ||
		(reset_flags & RESET_FLAG_WATCHDOG) ||
		(reset_flags & RESET_FLAG_HARD) ||
		(reset_flags & RESET_FLAG_SOFT))
		return 0;
	else
		return 1;
}

void system_pre_init(void)
{
	uint8_t imgtype;

	/* Make sure AHB Error capture is enabled.
	 * Signals bus fault to Cortex-M4 core */
	MEC17XX_EC_AHB_ERR = 0;	/* write any value to clear */
	MEC17XX_EC_AHB_ERR_EN = 0; /* enable capture of address on error */

#ifdef CONFIG_ESPI
	MEC17XX_EC_GPIO_BANK_PWR |= MEC17XX_EC_GPIO_BANK_PWR_VTR3_18;
#endif

#if 1
	/* TODO MCHP DEBUG HACK for downloading EC_RO via JTAG.
	 * No ec_lfw present to program system image type.
	 * Program VBAT system image type = 1 (EC_RO).
	 */
	if (0 == MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX)) {
#ifdef SECTION_IS_RW
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX) = SYSTEM_IMAGE_RW;
#else
		MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX) = SYSTEM_IMAGE_RO;
#endif
	}
#endif

	/* Enable direct NVIC */
	MEC17XX_EC_INT_CTRL |= 1;

	/* Disable ARM TRACE debug port */
	MEC17XX_EC_TRACE_EN &= ~1;

	/* Enable aggregated only interrupt GIRQ's
	 * Make sure direct mode GIRQ are not enabled
	 * Aggregated only GIRQ's 8,9,10,11,12,22,24,25,26
	 * Direct GIRQ's = 13,14,15,16,17,18,19,21,23
	 * These bits only need to be touched again on RESET_SYS.
	 * NOTE: GIRQ22 wake for AHB peripherals not processor.
	*/
#if 1
	MEC17XX_INT_BLK_DIS = 0xfffffffful;
	MEC17XX_INT_BLK_EN = (0x1Ful << 8) + (0x07ul << 24);
#endif
	/* Deassert nSIO_RESET */
	/* TODO MEC17XX signal is nRESET_OUT
	 * On MEC17xx this register selects the signal to generate platform reset.
	 * POR default=1 LRESET# pin generates MEC17xx internal platform reset.
	 * 0 eSPI_PLTRST# signal generates internal platform reset.
	 * So we clear this bit for eSPI connected EC.
	 * MEC17xx has nRESET_OUT and nRESET_IN. How are these controlled?
	 * GPIO062 POR default = RESETO#
	 */
#ifdef CONFIG_ESPI
	/* b[8]=0(eSPI PLTRST# VWire is platfrom reset), b[0]=0 VCC_PWRGD is
         * asserted when PLTRST# VWire is 1(inactive) */
	MEC17XX_PCR_PWR_RST_CTL = 0;
#else
	/* b[8]=1(LRESET# is platform reset), b[0]=0 VCC_PWRGD is
         * asserted when LRESET# is 1(inactive) */
	MEC17XX_PCR_PWR_RST_CTL = 0x100ul;
#endif

	spi_enable(CONFIG_SPI_FLASH_PORT, 1);

	/* MCHP */
	tfdp_power(1);
	tfdp_enable(1, 1);
	imgtype = MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX);
	CPRINTS("system_pre_init. Image type = 0x%02x",imgtype);
}

void chip_save_reset_flags(int flags)
{
	MEC17XX_VBAT_RAM(HIBDATA_INDEX_SAVED_RESET_FLAGS) = flags;
}

void _system_reset(int flags, int wake_from_hibernate)
{
	uint32_t save_flags = 0;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable();

	/* Save current reset reasons if necessary */
	if (flags & SYSTEM_RESET_PRESERVE_FLAGS)
		save_flags = system_get_reset_flags() | RESET_FLAG_PRESERVED;

	if (flags & SYSTEM_RESET_LEAVE_AP_OFF)
		save_flags |= RESET_FLAG_AP_OFF;

	if (wake_from_hibernate)
		save_flags |= RESET_FLAG_HIBERNATE;
	else if (flags & SYSTEM_RESET_HARD)
		save_flags |= RESET_FLAG_HARD;
	else
		save_flags |= RESET_FLAG_SOFT;

	chip_save_reset_flags(save_flags);

	/* Trigger watchdog in 1ms */
#if 0	/* TODO, should we use WDT? Does this code read WDT status
	 * to determine reset cause? */
	MEC17XX_WDG_LOAD = 1;
	MEC17XX_WDG_CTL |= 1;
#else
	/* MEC17XX use chip reset feature in PCR block */
	/* MCHP DEBUG
	MEC17XX_PCR_SYS_RST |= (1ul << 8);
	*/
	__asm__ __volatile__ (
		"\t bkpt 14 \n"
		"\t bx lr \n"
	);

#endif
	/* Spin and wait for reboot; should never return */
	while (1)
		;
}

void system_reset(int flags)
{
	_system_reset(flags, 0);
}

const char *system_get_chip_vendor(void)
{
	return "mchp";
}

const char *system_get_chip_name(void)
{
	switch (MEC17XX_CHIP_DEV_ID) {
	case 0x2D:
		return "mec1701";
	default:
		return "unknown";
	}
}

static char to_hex(int x)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	return 'a' + x - 10;
}

const char *system_get_chip_revision(void)
{
	static char buf[3];
	uint8_t rev = MEC17XX_CHIP_DEV_REV;

	buf[0] = to_hex(rev / 16);
	buf[1] = to_hex(rev & 0xf);
	buf[2] = '\0';
	return buf;
}

int system_get_vbnvcontext(uint8_t *block)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_vbnvcontext(const uint8_t *block)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_scratchpad(uint32_t value)
{
	MEC17XX_VBAT_RAM(HIBDATA_INDEX_SCRATCHPAD) = value;
	return EC_SUCCESS;
}

uint32_t system_get_scratchpad(void)
{
	return MEC17XX_VBAT_RAM(HIBDATA_INDEX_SCRATCHPAD);
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
	int i;

#ifdef CONFIG_HOSTCMD_PD
	/* Inform the PD MCU that we are going to hibernate. */
	host_command_pd_request_hibernate();
	/* Wait to ensure exchange with PD before hibernating. */
	msleep(100);
#endif

	cflush();

	if (board_hibernate)
		board_hibernate();

	/* Disable interrupts */
	interrupt_disable();
	for (i = 0; i <= 92; ++i) {
		task_disable_irq(i);
		task_clear_pending_irq(i);
	}

	for (i = 8; i <= 26; ++i)
		MEC17XX_INT_DISABLE(i) = 0xffffffff;

	MEC17XX_INT_BLK_DIS |= 0xffff00;

	/* Power down ADC VREF */
	/* TODO MEC17xx does not have internal ADC voltage reference
	 * therefore we must power ADC down a different way
	 * MEC1322_EC_ADC_VREF_PD |= 1;
	*/

	/* Assert nSIO_RESET */
	/* TODO MEC17xx how to assert nRESET_OUT ? */
	/* MEC17XX_PCR_PWR_RST_CTL |= 1; */
	/* MEC17xx GPIO062 = RESETO# */


	/* Disable UART */
	MEC17XX_UART_ACT(0) &= ~0x1;
	MEC17XX_LPC_ACT &= ~0x1;

	/* Disable JTAG */
	MEC17XX_EC_JTAG_EN &= ~1;

	/* Disable 32KHz clock */
	/* TODO MEC17xx */
	MEC17XX_VBAT_CE &= ~0x2;

	/* Stop watchdog */
	MEC17XX_WDG_CTL &= ~1;

	/* Stop timers */
	MEC17XX_TMR32_CTL(0) &= ~1;
	MEC17XX_TMR32_CTL(1) &= ~1;
	MEC17XX_TMR16_CTL(0) &= ~1;

	/* Power down ADC */
	/* MEC17xx TODO - Does ADC require other shutdown before sleep?
	 * If ADC is in middle of acquisition it will continue until finished */
	MEC17XX_ADC_CTRL &= ~1;

	/* Disable blocks */
	/* TODO MEC17xx set all bits or use SLEEP_ALL?
	 * What is wake source?
	 * MEC17xx Sleep system is:
	 * For blocks you that are not needed as wake sources
	 * Set PCR_SLP_ENx bits
	 * For blocks that are wake sources make sure PCR_SLP_ENx bits
	 * are clear.
	 * Set PCR_SYS_SLP_CTL SLEEP_ALL(bit[3]) = 1 and
	 * bit[0] = 0 (light sleep, 0ms wake latency, PLL remains ON)
	 * bit[1] = 1 (heavy sleep, 3ms wake latency, PLL is turned OFF)
	 */
	MEC17XX_PCR_SLP_EN0 |= MEC17XX_PCR_SLP_EN0_SLEEP;
	MEC17XX_PCR_SLP_EN1 |= MEC17XX_PCR_SLP_EN1_SLEEP;
	MEC17XX_PCR_SLP_EN2 |= MEC17XX_PCR_SLP_EN2_SLEEP;
	MEC17XX_PCR_SLP_EN3 |= MEC17XX_PCR_SLP_EN3_SLEEP;
	MEC17XX_PCR_SLP_EN4 |= MEC17XX_PCR_SLP_EN4_SLEEP;

	MEC17XX_PCR_SLOW_CLK_CTL &= ~(MEC17xx_PCR_SLOW_CLK_CTL_MASK);

	/* Set sleep state */
	/* TODO MEC17xx. On MEC1322 this sets
	 * sleep mode = System Heavy Sleep 2
	MEC1322_PCR_SYS_SLP_CTL = (MEC17XX_PCR_SYS_SLP_CTL & ~0x7) | 0x2;
	*/
	MEC17XX_PCR_SYS_SLP_CTL = (1ul << 3) + (1ul << 0);
	CPU_SCB_SYSCTRL |= 0x4;

	/* Setup GPIOs for hibernate */
	if (board_hibernate_late)
		board_hibernate_late();

#ifdef CONFIG_USB_PD_PORT_COUNT
	/*
	 * Leave USB-C charging enabled in hibernate, in order to
	 * allow wake-on-plug. 5V enable must be pulled low.
	 */
#if CONFIG_USB_PD_PORT_COUNT > 0
	gpio_set_flags(GPIO_USB_C0_5V_EN, GPIO_PULL_DOWN | GPIO_INPUT);
	gpio_set_level(GPIO_USB_C0_CHARGE_EN_L, 0);
#endif
#if CONFIG_USB_PD_PORT_COUNT > 1
	gpio_set_flags(GPIO_USB_C1_5V_EN, GPIO_PULL_DOWN | GPIO_INPUT);
	gpio_set_level(GPIO_USB_C1_CHARGE_EN_L, 0);
#endif
#endif /* CONFIG_USB_PD_PORT_COUNT */

	if (hibernate_wake_pins_used > 0) {
		for (i = 0; i < hibernate_wake_pins_used; ++i) {
			const enum gpio_signal pin = hibernate_wake_pins[i];

			gpio_reset(pin);
			gpio_enable_interrupt(pin);
		}

		interrupt_enable();
		task_enable_irq(MEC17XX_IRQ_GIRQ8);
		task_enable_irq(MEC17XX_IRQ_GIRQ9);
		task_enable_irq(MEC17XX_IRQ_GIRQ10);
		task_enable_irq(MEC17XX_IRQ_GIRQ11);
		task_enable_irq(MEC17XX_IRQ_GIRQ12); /* MEC17xx GPIO200 - GPIO235 */
	}

	if (seconds || microseconds) {
		/* Not needed for direct mode interrupts
		MEC17XX_INT_BLK_EN |= 1 << MEC17XX_HTIMER_GIRQ;
		*/
		MEC17XX_INT_ENABLE(MEC17XX_HTIMER_GIRQ) = MEC17XX_HTIMER_GIRQ_BIT(0);
		interrupt_enable();
		task_enable_irq(MEC17XX_IRQ_HTIMER0);
		if (seconds > 2) {
			ASSERT(seconds <= 0xffff / 8);
			MEC17XX_HTIMER_CONTROL(0) = 1;
			MEC17XX_HTIMER_PRELOAD(0) =
				(seconds * 8 + microseconds / 125000);
		} else {
			MEC17XX_HTIMER_CONTROL(0) = 0;
			MEC17XX_HTIMER_PRELOAD(0) =
				(seconds * 1000000 + microseconds) * 2 / 71;
		}
	}

	asm("wfi");

	/* Use 48MHz clock to speed through wake-up */
	MEC17XX_PCR_PROC_CLK_CTL = 1;

	/* Reboot */
	_system_reset(0, 1);

	/* We should never get here. */
	while (1)
		;
}

void htimer_interrupt(void)
{
	/* Time to wake up */
}
DECLARE_IRQ(MEC17XX_IRQ_HTIMER0, htimer_interrupt, 1);

enum system_image_copy_t system_get_shrspi_image_copy(void)
{
	return MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX);
}

uint32_t system_get_lfw_address(void)
{
	uint32_t * const lfw_vector =
		(uint32_t * const)CONFIG_PROGRAM_MEMORY_BASE; /* TODO check value */

	return *(lfw_vector + 1);
}

void system_set_image_copy(enum system_image_copy_t copy)
{
	MEC17XX_VBAT_RAM(MEC17XX_IMAGETYPE_IDX) = (copy == SYSTEM_IMAGE_RW) ?
				SYSTEM_IMAGE_RW : SYSTEM_IMAGE_RO;
}

/* MCHP Debug add exception handlers with breakpoint to facilitate
 * GDB JTAG debugging */
#if 1
__attribute__((naked)) void hard_fault_handler(void)
{
	__asm__ __volatile__ (
		"\t bkpt 10 \n"
		"\t bx lr \n"
	);
}

__attribute__((naked)) void mpu_fault_handler(void)
{
	__asm__ __volatile__ (
		"\t bkpt 11 \n"
		"\t bx lr \n"
	);
}

__attribute__((naked)) void bus_fault_handler(void)
{
	__asm__ __volatile__ (
		"\t bkpt 12 \n"
		"\t bx lr \n"
	);
}


__attribute__((naked)) void usage_fault_handler(void)
{
	__asm__ __volatile__ (
		"\t bkpt 13 \n"
		"\t bx lr \n"
	);
}
#endif

