/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#include "common.h"
#include "cpu.h"
#include "hooks.h"
#include "hwtimer_chip.h"
#include "panic.h"
#include "registers.h"
#include "task.h"
#include "watchdog.h"

/* Enter critical period or not. */
static int wdt_warning_fired;

/*
 * We use WDT_EXT_TIMER to trigger an interrupt just before the watchdog timer
 * will fire so that we can capture important state information before
 * being reset.
 */

/* Magic value to tickle the watchdog register. */
#define ITE83XX_WATCHDOG_MAGIC_WORD  0x5C
/* Start to print warning message. */
#define ITE83XX_WATCHDOG_WARNING_MS  CONFIG_AUX_TIMER_PERIOD_MS
/* The interval to print warning message at critical period. */
#define ITE83XX_WATCHDOG_CRITICAL_MS 30

#define WARNING_TIMER_MS_TO_1024HZ_COUNT(ms)  ((ms) * 1024 / 1000)

/* set warning timer */
static void watchdog_set_warning_timer(int32_t ms, int init)
{
#ifdef CONFIG_IT83XX_WDT_WARNING_BY_TIMER1
	/* timer 1 is 16-bit counter down timer */
	uint16_t cnt = WARNING_TIMER_MS_TO_1024HZ_COUNT(ms);

	/* configure timer 1 counter */
	IT83XX_ETWD_ET1CNTLHR = (cnt >> 8) & 0xff;
	IT83XX_ETWD_ET1CNTLLR = cnt & 0xff;
	/* clear interrupt status */
	task_clear_pending_irq(IT83XX_IRQ_EXT_TIMER1);
#else
	ext_timer_ms(WDT_EXT_TIMER, EXT_PSR_32P768K_HZ, 1, 1, ms, init, 0);
#endif
}

void watchdog_warning_irq(void)
{
#ifdef CONFIG_SOFTWARE_PANIC
	struct panic_data * const pdata_ptr = get_panic_data_write();

#if defined(CHIP_CORE_NDS32)
	pdata_ptr->nds_n8.ipc = get_ipc();
#elif defined(CHIP_CORE_RISCV)
	pdata_ptr->riscv.mepc = get_mepc();
#endif
#endif
#ifdef CONFIG_IT83XX_WDT_WARNING_BY_TIMER1
	/* clear interrupt status */
	task_clear_pending_irq(IT83XX_IRQ_EXT_TIMER1);

	/* Reset warning timer. */
	IT83XX_ETWD_ETWCTRL |= BIT(0);
#else
	/* clear interrupt status */
	task_clear_pending_irq(et_ctrl_regs[WDT_EXT_TIMER].irq);

	/* Reset warning timer. */
	IT83XX_ETWD_ETXCTRL(WDT_EXT_TIMER) = 0x03;

#endif

#if defined(CHIP_CORE_NDS32)
	/*
	 * The IPC (Interruption Program Counter) is the shadow stack register
	 * of the PC (Program Counter). It stores the return address of program
	 * (PC->IPC) when the ISR was called.
	 *
	 * The LP (Link Pointer) stores the program address of the next
	 * sequential instruction for function call return purposes.
	 * LP = PC+4 after a jump and link instruction (jal).
	 */
	panic_printf("Pre-WDT warning! IPC:%08x LP:%08x TASK_ID:%d\n",
		get_ipc(), ilp, task_get_current());
#elif defined(CHIP_CORE_RISCV)
	panic_printf("Pre-WDT warning! MEPC:%08x RA:%08x TASK_ID:%d\n",
		get_mepc(), ira, task_get_current());
#endif

	if (!wdt_warning_fired++)
		/*
		 * Reduce interval of warning timer, so we can print more
		 * warning messages during critical period.
		 */
		watchdog_set_warning_timer(ITE83XX_WATCHDOG_CRITICAL_MS, 0);
}

void watchdog_reload(void)
{
	/* Reset warning timer. */
#ifdef CONFIG_IT83XX_WDT_WARNING_BY_TIMER1
	IT83XX_ETWD_ETWCTRL |= BIT(0);
#else
	IT83XX_ETWD_ETXCTRL(WDT_EXT_TIMER) = 0x03;
#endif

	/* Restart (tickle) watchdog timer. */
	IT83XX_ETWD_EWDKEYR = ITE83XX_WATCHDOG_MAGIC_WORD;

	if (wdt_warning_fired) {
		wdt_warning_fired = 0;
		/* Reset warning timer to default if watchdog is touched. */
		watchdog_set_warning_timer(ITE83XX_WATCHDOG_WARNING_MS, 0);
	}
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

int watchdog_init(void)
{
	uint16_t wdt_count = CONFIG_WATCHDOG_PERIOD_MS * 1024 / 1000;

	/* Unlock access to watchdog registers. */
	IT83XX_ETWD_ETWCFG = 0x00;

	/* Set WD timer to use 1.024kHz clock. */
	IT83XX_ETWD_ET1PSR = 0x01;

	/* Set WDT key match enabled and WDT clock to use ET1PSR. */
	IT83XX_ETWD_ETWCFG = 0x30;

#ifdef CONFIG_HIBERNATE
	/* bit4: watchdog can be stopped. */
	IT83XX_ETWD_ETWCTRL |= BIT(4);
#else
	/* Specify that watchdog cannot be stopped. */
	IT83XX_ETWD_ETWCTRL = 0x00;
#endif

	/* Start WDT_EXT_TIMER (CONFIG_AUX_TIMER_PERIOD_MS ms). */
	watchdog_set_warning_timer(ITE83XX_WATCHDOG_WARNING_MS, 1);

#ifdef CONFIG_IT83XX_WDT_WARNING_BY_TIMER1
	/* Enable timer 1 interrupt for printing warning message */
	task_enable_irq(IT83XX_IRQ_EXT_TIMER1);
#else
	/* Start timer 1 (must be started for watchdog timer to run). */
	IT83XX_ETWD_ET1CNTLLR = 0x00;
#endif

	/*
	 * Set watchdog timer to CONFIG_WATCHDOG_PERIOD_MS ms.
	 * Writing CNTLL starts timer.
	 */
	IT83XX_ETWD_EWDCNTLHR = (wdt_count >> 8) & 0xff;
	IT83XX_ETWD_EWDCNTLLR = wdt_count & 0xff;

#ifdef CONFIG_IT83XX_WDT_WARNING_BY_TIMER1
	/*
	 * Lock access to watchdog registers.
	 * bit2=0: access timer 1 counter is allowed
	 */
	IT83XX_ETWD_ETWCFG = 0x3b;
#else
	/* Lock access to watchdog registers. */
	IT83XX_ETWD_ETWCFG = 0x3f;
#endif

	return EC_SUCCESS;
}
