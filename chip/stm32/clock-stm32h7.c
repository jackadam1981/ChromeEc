/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "chipset.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CLOCK, outstr)
#define CPRINTF(format, args...) cprintf(CC_CLOCK, format, ## args)

static const char *const msg_unimplemented = "Unimplemented";

/* High-speed oscillator default is 64 MHz */
#define STM32_HSI_CLOCK 64000000
/* Low-speed oscillator is 32-Khz */
#define STM32_LSI_CLOCK 32000

/*
 * LPTIM is a 16-bit counter clocked by LSI
 * with /4 prescaler (2^2): period 125 us, full range ~8s
 */
#define LPTIM_PRESCALER_LOG2 2
/*
 * LPTIM_PRESCALER and LPTIM_PERIOD_US have to be signed, because we compare
 * them to an int to decide whether to go to deep sleep. Simply using BIT()
 * makes them unsigned, which causes a bug in deep sleep behavior.
 * TODO(b/140538084): Explain exactly what the bug is.
 */
#define LPTIM_PRESCALER ((int)BIT(LPTIM_PRESCALER_LOG2))
#define LPTIM_PERIOD_US (SECOND / (STM32_LSI_CLOCK / LPTIM_PRESCALER))

enum clock_osc {
	OSC_HSI = 0,	/* High-speed internal oscillator */
	OSC_CSI,	/* Multi-speed internal oscillator: NOT IMPLEMENTED */
	OSC_HSE,	/* High-speed external oscillator: NOT IMPLEMENTED */
	OSC_PLL,	/* PLL */
};

enum voltage_scale {
	VOLTAGE_SCALE0 = 0,
	VOLTAGE_SCALE1,
	VOLTAGE_SCALE2,
	VOLTAGE_SCALE3,
	VOLTAGE_SCALE_COUNT,
};

enum freq {
	FREQ_64MHZ  = 64  * 1000000,
	FREQ_200MHZ = 200 * 1000000,
	FREQ_280MHZ = 280 * 1000000,
	FREQ_400MHZ = 400 * 1000000,
	FREQ_480MHZ = 480 * 1000000,
};

static int freq = STM32_HSI_CLOCK;
static int current_osc = OSC_HSI;

int clock_get_freq(void)
{
	return freq;
}

int clock_get_timer_freq(void)
{
	return clock_get_freq();
}

void clock_wait_bus_cycles(enum bus_type bus, uint32_t cycles)
{
	volatile uint32_t dummy __attribute__((unused));

	if (bus == BUS_AHB) {
		while (cycles--)
			dummy = STM32_GPIO_IDR(GPIO_A);
	} else { /* APB */
		while (cycles--)
			dummy = STM32_USART_BRR(STM32_USART1_BASE);
	}
}

/* Flash latency values are dependent on peripheral speed and voltage scale */
static void clock_flash_latency(enum freq axi_freq, enum voltage_scale vos)
{
	uint32_t target_acr;

#ifdef CHIP_VARIANT_STM32H7X3
	if (axi_freq == FREQ_64MHZ && vos == VOLTAGE_SCALE3) {
		target_acr = STM32_FLASH_ACR_WRHIGHFREQ_85MHZ |
			     (0 << STM32_FLASH_ACR_LATENCY_SHIFT);
	} else if (axi_freq == FREQ_200MHZ && vos == VOLTAGE_SCALE1) {
		target_acr = STM32_FLASH_ACR_WRHIGHFREQ_285MHZ |
			     (2 << STM32_FLASH_ACR_LATENCY_SHIFT);
	} else {
		panic(msg_unimplemented);
	}
#endif /* CHIP_VARIANT_STM32H7X3 */

#ifdef CHIP_VARIANT_STM32H7A
	if (axi_freq == FREQ_64MHZ && vos == VOLTAGE_SCALE3) {
		target_acr = STM32_FLASH_ACR_WRHIGHFREQ(1) |
			     STM32_FLASH_ACR_LATENCY(2);
	} else if (axi_freq == FREQ_200MHZ && vos == VOLTAGE_SCALE1) {
		target_acr = STM32_FLASH_ACR_WRHIGHFREQ(2) |
			     STM32_FLASH_ACR_LATENCY(5);
	} else if (axi_freq == FREQ_200MHZ && vos == VOLTAGE_SCALE0) {
		target_acr = STM32_FLASH_ACR_WRHIGHFREQ(2) |
			     STM32_FLASH_ACR_LATENCY(4);
	} else if (axi_freq == FREQ_280MHZ && vos == VOLTAGE_SCALE0) {
		target_acr = STM32_FLASH_ACR_WRHIGHFREQ(3) |
			     STM32_FLASH_ACR_LATENCY(6);
	} else {
		panic(msg_unimplemented);
	}
#endif /* CHIP_VARIANT_STM32H7A */

	STM32_FLASH_ACR(0) = target_acr;
	while (STM32_FLASH_ACR(0) != target_acr)
		;
}

static void clock_pll1_configure(enum freq freq) {
	uint32_t divm = 4; // Input prescaler (16MHz max for PLL -- 64/4 ==> 16)
	uint32_t divn;     // Pll multiplier
	uint32_t divp;     // Output 1 prescaler
	switch (freq)
	{
	case FREQ_400MHZ:
		/*
		 * PLL1 configuration:
		 * CPU freq = VCO / DIVP = HSI / DIVM * DIVN / DIVP
		 *          = 64MHz/4 * 50 / 2
		 *          = 16MHz * 50 / 2
		 *          = 400 Mhz
		 */
		divn = 50;
		divp = 2;
		break;
	case FREQ_200MHZ:
		/*
		 * PLL1 configuration:
		 * CPU freq = VCO / DIVP = HSI / DIVM * DIVN / DIVP
		 *          = 64 / 4 * 25 / 2
		 *          = 16MHz * 50 / 2
		 *          = 200 Mhz
		 */
		divn = 25;
		divp = 2;
		break;
	case FREQ_280MHZ:
		divn = 35;
		divp = 2;
		break;
	case FREQ_480MHZ:
		divn = 60;
		divp = 2;
		break;
	default:
		panic(msg_unimplemented);
		break;
	}

	ASSERT((STM32_HSI_CLOCK / divm * divn / divp) == freq);

	/* Configure PLL1 using 64 Mhz HSI as input */
	STM32_RCC_PLLCKSELR = STM32_RCC_PLLCKSEL_PLLSRC_HSI
			    | STM32_RCC_PLLCKSEL_DIVM1(divm);
	/* in integer mode, wide range VCO with 16Mhz input, use divP */
	STM32_RCC_PLLCFGR = STM32_RCC_PLLCFG_PLL1VCOSEL_WIDE
			  | STM32_RCC_PLLCFG_PLL1RGE_8M_16M
			  | STM32_RCC_PLLCFG_DIVP1EN;
	STM32_RCC_PLL1DIVR = STM32_RCC_PLLDIV_DIVP(divp)
			   | STM32_RCC_PLLDIV_DIVN(divn);
}

/**
 * Configure peripheral domain prescalers to allow a given sysclk frequency.
 *
 * @param sysclk The input system clock, after the system clock prescaler.
 */
static void clock_peripheral_configure(enum freq sysclk) {
#ifdef CHIP_VARIANT_STM32H7X3
	switch (freq)
	{
	case FREQ_64MHZ:
		/* Restore /1 HPRE (AHB prescaler) */
		/* Disable downstream prescalers */
		STM32_RCC_D1CFGR = STM32_RCC_D1CFGR_HPRE_DIV1
				 | STM32_RCC_D1CFGR_D1PPRE_DIV1
				 | STM32_RCC_D1CFGR_D1CPRE_DIV1;
		/* TODO(b/149512910): Adjust more peripheral prescalers */
		freq = FREQ_64MHZ;
		break;
	case FREQ_400MHZ:
		/* Put /2 on HPRE (AHB prescaler) to keep at the 200MHz max */
		STM32_RCC_D1CFGR = STM32_RCC_D1CFGR_HPRE_DIV2
				 | STM32_RCC_D1CFGR_D1PPRE_DIV1
				 | STM32_RCC_D1CFGR_D1CPRE_DIV1;
		/* TODO(b/149512910): Adjust more peripheral prescalers */
		freq = FREQ_400MHZ / 2;
		break;
	default:
		panic(msg_unimplemented);
	}
#endif /* CHIP_VARIANT_STM32H7X3 */
#ifdef  CHIP_VARIANT_STM32H7A
	switch (freq)
	{
	case FREQ_64MHZ:
		/* Disable all downstream bus prescalers */
		STM32_RCC_CDCFGR1 = STM32_RCC_CDCFGR1_HPRE_DIV1
				  | STM32_RCC_CDCFGR1_CDPPRE_DIV1
				  | STM32_RCC_CDCFGR1_CDCPRE_DIV1;
		freq = FREQ_64MHZ;
		break;
	case FREQ_200MHZ:
		/* No prescaler changes needed (not totally true) - All default /1 */
		/* Disable all downstream bus prescalers */
		STM32_RCC_CDCFGR1 = STM32_RCC_CDCFGR1_HPRE_DIV1
				  | STM32_RCC_CDCFGR1_CDPPRE_DIV1
				  | STM32_RCC_CDCFGR1_CDCPRE_DIV1;
		freq = FREQ_200MHZ;
		break;
	case FREQ_280MHZ:
		/* Divide all downstream buses by 2 */
		STM32_RCC_CDCFGR1 = STM32_RCC_CDCFGR1_HPRE_DIV2
				  | STM32_RCC_CDCFGR1_CDPPRE_DIV1
				  | STM32_RCC_CDCFGR1_CDCPRE_DIV1;
		freq = FREQ_280MHZ;
		break;
	default:
		panic(msg_unimplemented);
	}
#endif /* CHIP_VARIANT_STM32H7A */
}

static void clock_enable_osc(enum clock_osc osc)
{
	uint32_t ready;
	uint32_t on;

	switch (osc) {
	case OSC_HSI:
		ready = STM32_RCC_CR_HSIRDY;
		on = STM32_RCC_CR_HSION;
		break;
	case OSC_PLL:
		ready = STM32_RCC_CR_PLL1RDY;
		on = STM32_RCC_CR_PLL1ON;
		break;
	default:
		return;
	}

	if (!(STM32_RCC_CR & ready)) {
		STM32_RCC_CR |= on;
		while (!(STM32_RCC_CR & ready))
			;
	}
}

static void clock_switch_osc(enum clock_osc osc)
{
	uint32_t sw;
	uint32_t sws;

	switch (osc) {
	case OSC_HSI:
		sw = STM32_RCC_CFGR_SW_HSI;
		sws = STM32_RCC_CFGR_SWS_HSI;
		break;
	case OSC_PLL:
		sw = STM32_RCC_CFGR_SW_PLL1;
		sws = STM32_RCC_CFGR_SWS_PLL1;
		break;
	default:
		return;
	}

	STM32_RCC_CFGR = sw;
	while ((STM32_RCC_CFGR & STM32_RCC_CFGR_SWS_MASK) != sws)
		;
}

static void switch_voltage_scale(enum voltage_scale vos)
{
#ifdef CHIP_VARIANT_STM32H7X3
	volatile uint32_t *const vos_reg   = &STM32_PWR_D3CR;
	const uint32_t           vos_ready = STM32_PWR_D3CR_VOSRDY;
	const uint32_t           vos_mask  = STM32_PWR_D3CR_VOSMASK;
	const uint32_t           vos_values[VOLTAGE_SCALE_COUNT] = {
							STM32_PWR_D3CR_VOS1,
							STM32_PWR_D3CR_VOS1,
							STM32_PWR_D3CR_VOS2,
							STM32_PWR_D3CR_VOS3,
						 };
	/* VOS0 on the H743 requires VOS1 and setting an extra SYS reg */
	if (vos == VOLTAGE_SCALE0)
		panic(msg_unimplemented);
#endif /* CHIP_VARIANT_STM32H7X3 */
#ifdef  CHIP_VARIANT_STM32H7A
	volatile uint32_t *const vos_reg   = &STM32_PWR_SRDCR;
	const uint32_t           vos_ready = STM32_PWR_SRDCR_VOSRDY;
	const uint32_t           vos_mask  = STM32_PWR_SRDCR_VOSMASK;
	const uint32_t           vos_values[VOLTAGE_SCALE_COUNT] = {
							STM32_PWR_SRDCR_VOS0,
							STM32_PWR_SRDCR_VOS1,
							STM32_PWR_SRDCR_VOS2,
							STM32_PWR_SRDCR_VOS3,
						 };
#endif /* CHIP_VARIANT_STM32H7A */
	*vos_reg &= ~vos_mask;
	*vos_reg |= vos_values[vos];
	while (!(*vos_reg & vos_ready))
		;
}

static void clock_set_osc(enum clock_osc osc)
{
	if (osc == current_osc)
		return;

	hook_notify(HOOK_PRE_FREQ_CHANGE);

	switch (osc) {
	case OSC_HSI:
		/* Switch to HSI */
		clock_switch_osc(osc);
		freq = STM32_HSI_CLOCK;
		clock_peripheral_configure(FREQ_64MHZ);
		/* Use more optimized flash latency settings for 64-MHz ACLK */
		clock_flash_latency(FREQ_64MHZ, VOLTAGE_SCALE3);
		/* Turn off the PLL1 to save power */
		STM32_RCC_CR &= ~STM32_RCC_CR_PLL1ON;
		switch_voltage_scale(VOLTAGE_SCALE3);
		break;

	case OSC_PLL:

#ifdef CHIP_VARIANT_STM32H7X3
		switch_voltage_scale(VOLTAGE_SCALE1);
		/*
		 * PLL1 configuration:
		 * CPU freq = VCO / DIVP = HSI / DIVM * DIVN / DIVP
		 *          = 64 / 4 * 25 / 2
		 *          = 200 Mhz
		 * System clock = 200 Mhz
		 *  HPRE = /1  => AHB/Timer clock = 200 Mhz
		 */
		clock_pll1_configure(FREQ_400MHZ);
		/* turn on PLL1 and wait until it's ready */
		clock_enable_osc(OSC_PLL);
		clock_peripheral_configure(FREQ_400MHZ);
		/* Increase flash latency before transition the clock */
		clock_flash_latency(FREQ_200MHZ, VOLTAGE_SCALE1);
#endif /* CHIP_VARIANT_STM32H7X3 */

#ifdef  CHIP_VARIANT_STM32H7A
		switch_voltage_scale(VOLTAGE_SCALE0);
		/*
		 * PLL1 configuration:
		 * CPU freq = VCO / DIVP = HSI / DIVM * DIVN / DIVP
		 *          = 64 / 4 * 25 / 2
		 *          = 280 Mhz
		 * System clock = 200 Mhz
		 *  HPRE = /1  => AHB/Timer clock = 200 Mhz
		 */
		clock_pll1_configure(FREQ_280MHZ);
		/* turn on PLL1 and wait until it's ready */
		clock_enable_osc(OSC_PLL);
		clock_peripheral_configure(FREQ_280MHZ);
		/* Increase flash latency before transition the clock */
		clock_flash_latency(FREQ_280MHZ, VOLTAGE_SCALE0);
#endif /* CHIP_VARIANT_STM32H7A */

		/* Switch to PLL */
		clock_switch_osc(OSC_PLL);
		break;
	default:
		break;
	}

	current_osc = osc;
	hook_notify(HOOK_FREQ_CHANGE);
}

void clock_enable_module(enum module_id module, int enable)
{
	/* Assume we have a single task using MODULE_FAST_CPU */
	if (module == MODULE_FAST_CPU) {
		/* the PLL would be off in low power mode, disable it */
		if (enable)
			disable_sleep(SLEEP_MASK_PLL);
		else
			enable_sleep(SLEEP_MASK_PLL);
		clock_set_osc(enable ? OSC_PLL : OSC_HSI);
	}
}

#ifdef CONFIG_LOW_POWER_IDLE
/* Low power idle statistics */
static int idle_sleep_cnt;
static int idle_dsleep_cnt;
static uint64_t idle_dsleep_time_us;
static int dsleep_recovery_margin_us = 1000000;

/* STOP_MODE_LATENCY: delay to wake up from STOP mode with flash off in SVOS5 */
#define STOP_MODE_LATENCY 50 /* us */

static void low_power_init(void)
{
	/* Clock LPTIM1 on the 32-kHz LSI for STOP mode time keeping */
	STM32_RCC_D2CCIP2R = (STM32_RCC_D2CCIP2R &
		~STM32_RCC_D2CCIP2_LPTIM1SEL_MASK)
		| STM32_RCC_D2CCIP2_LPTIM1SEL_LSI;

	/* configure LPTIM1 as our 1-Khz low power timer in STOP mode */
	STM32_RCC_APB1LENR |= STM32_RCC_PB1_LPTIM1;
	STM32_LPTIM_CR(1) = 0; /* ensure it's disabled before configuring */
	STM32_LPTIM_CFGR(1) = LPTIM_PRESCALER_LOG2 << 9; /* Prescaler /4 */
	STM32_LPTIM_IER(1) = STM32_LPTIM_INT_CMPM; /* Compare int for wake-up */
	/* Start the 16-bit free-running counter */
	STM32_LPTIM_CR(1) = STM32_LPTIM_CR_ENABLE;
	STM32_LPTIM_ARR(1) = 0xFFFF;
	STM32_LPTIM_CR(1) = STM32_LPTIM_CR_ENABLE | STM32_LPTIM_CR_CNTSTRT;
	task_enable_irq(STM32_IRQ_LPTIM1);

	/* Wake-up interrupts from EXTI for USART and LPTIM */
	STM32_EXTI_CPUIMR1 |= BIT(26); /* [26] wkup26: USART1 wake-up */
	STM32_EXTI_CPUIMR2 |= BIT(15); /* [15] wkup47: LPTIM1 wake-up */

	/* optimize power vs latency in STOP mode */
	STM32_PWR_CR = (STM32_PWR_CR & ~STM32_PWR_CR_SVOS_MASK)
		     | STM32_PWR_CR_SVOS5
		     | STM32_PWR_CR_FLPS;
}

void clock_refresh_console_in_use(void)
{
}

void lptim_interrupt(void)
{
	STM32_LPTIM_ICR(1) = STM32_LPTIM_INT_CMPM;
}
DECLARE_IRQ(STM32_IRQ_LPTIM1, lptim_interrupt, 2);

static uint16_t lptim_read(void)
{
	uint16_t cnt;

	do {
		cnt = STM32_LPTIM_CNT(1);
	} while (cnt != STM32_LPTIM_CNT(1));

	return cnt;
}

static void set_lptim_event(int delay_us, uint16_t *lptim_cnt)
{
	uint16_t cnt = lptim_read();

	STM32_LPTIM_CMP(1) = cnt + MIN(delay_us / LPTIM_PERIOD_US - 1, 0xffff);
	/* clean-up previous event */
	STM32_LPTIM_ICR(1) = STM32_LPTIM_INT_CMPM;
	*lptim_cnt = cnt;
}

void __idle(void)
{
	timestamp_t t0;
	int next_delay;
	int margin_us, t_diff;
	uint16_t lptim0;

	while (1) {
		asm volatile("cpsid i");

		t0 = get_time();
		next_delay = __hw_clock_event_get() - t0.le.lo;

		if (DEEP_SLEEP_ALLOWED &&
		    next_delay > LPTIM_PERIOD_US + STOP_MODE_LATENCY) {
			/* deep-sleep in STOP mode */
			idle_dsleep_cnt++;

			uart_enable_wakeup(1);

			/* set deep sleep bit */
			CPU_SCB_SYSCTRL |= 0x4;

			set_lptim_event(next_delay - STOP_MODE_LATENCY,
					&lptim0);

			/* ensure outstanding memory transactions complete */
			asm volatile("dsb");

			asm("wfi");

			CPU_SCB_SYSCTRL &= ~0x4;

			/* fast forward timer according to low power counter */
			if (STM32_PWR_CPUCR & STM32_PWR_CPUCR_STOPF) {
				uint16_t lptim_dt = lptim_read() - lptim0;

				t_diff = (int)lptim_dt * LPTIM_PERIOD_US;
				t0.val = t0.val + t_diff;
				force_time(t0);
				/* clear STOPF flag */
				STM32_PWR_CPUCR |= STM32_PWR_CPUCR_CSSF;
			} else { /* STOP entry was aborted, no fixup */
				t_diff = 0;
			}

			uart_enable_wakeup(0);

			/* Record time spent in deep sleep. */
			idle_dsleep_time_us += t_diff;

			/* Calculate how close we were to missing deadline */
			margin_us = next_delay - t_diff;
			if (margin_us < 0)
				/* Use CPUTS to save stack space */
				CPUTS("Overslept!\n");

			/* Record the closest to missing a deadline. */
			if (margin_us < dsleep_recovery_margin_us)
				dsleep_recovery_margin_us = margin_us;
		} else {
			idle_sleep_cnt++;

			/* normal idle : only CPU clock stopped */
			asm("wfi");
		}
		asm volatile("cpsie i");
	}
}

#ifdef CONFIG_CMD_IDLE_STATS
/**
 * Print low power idle statistics
 */
static int command_idle_stats(int argc, char **argv)
{
	timestamp_t ts = get_time();

	ccprintf("Num idle calls that sleep:           %d\n", idle_sleep_cnt);
	ccprintf("Num idle calls that deep-sleep:      %d\n", idle_dsleep_cnt);
	ccprintf("Time spent in deep-sleep:            %.6llds\n",
			idle_dsleep_time_us);
	ccprintf("Total time on:                       %.6llds\n", ts.val);
	ccprintf("Deep-sleep closest to wake deadline: %dus\n",
			dsleep_recovery_margin_us);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(idlestats, command_idle_stats,
			"",
			"Print last idle stats");
#endif /* CONFIG_CMD_IDLE_STATS */
#endif /* CONFIG_LOW_POWER_IDLE */

void clock_init(void)
{
	/*
	 * STM32H743 Errata 2.2.15:
	 * 'Reading from AXI SRAM might lead to data read corruption'
	 *
	 * limit concurrent read access on AXI master to 1.
	 */
	STM32_AXI_TARG_FN_MOD(7) |= READ_ISS_OVERRIDE;

	/*
	 * Lock (SCUEN=0) power configuration with the LDO enabled.
	 *
	 * The STM32H7 Reference Manual says:
	 * The lower byte of this register is written once after POR and shall
	 * be written before changing VOS level or ck_sys clock frequency.
	 *
	 * The interesting side-effect of this that while the LDO is enabled by
	 * default at startup, if we enter STOP mode without locking it the MCU
	 * seems to freeze forever.
	 */
	STM32_PWR_CR3 = STM32_PWR_CR3_LDOEN;
	/*
	 * Ensure the SPI is always clocked at the same frequency
	 * by putting it on the fixed 64-Mhz HSI clock.
	 * per_ck is clocked directly by the HSI (as per the default settings).
	 */
	STM32_RCC_D2CCIP1R = (STM32_RCC_D2CCIP1R &
		~(STM32_RCC_D2CCIP1R_SPI123SEL_MASK |
		  STM32_RCC_D2CCIP1R_SPI45SEL_MASK))
		| STM32_RCC_D2CCIP1R_SPI123SEL_PERCK
		| STM32_RCC_D2CCIP1R_SPI45SEL_HSI;

	/* Use more optimized flash latency settings for ACLK = HSI = 64 Mhz */
	clock_flash_latency(FREQ_64MHZ, VOLTAGE_SCALE3);

	/* Ensure that LSI is ON to clock LPTIM1 and IWDG */
	STM32_RCC_CSR |= STM32_RCC_CSR_LSION;
	while (!(STM32_RCC_CSR & STM32_RCC_CSR_LSIRDY))
		;

#ifdef CONFIG_LOW_POWER_IDLE
	low_power_init();
#endif
}

static int command_clock(int argc, char **argv)
{
	if (argc >= 2) {
		if (!strcasecmp(argv[1], "hsi"))
			clock_set_osc(OSC_HSI);
		else if (!strcasecmp(argv[1], "pll"))
			clock_set_osc(OSC_PLL);
		else
			return EC_ERROR_PARAM1;
	}
	ccprintf("Clock frequency is now %d Hz\n", freq);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(clock, command_clock,
			"hsi | pll", "Set clock frequency");
