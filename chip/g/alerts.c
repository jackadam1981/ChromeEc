/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"

/* This file includes code to manage the g chip alert interrupts. */
#define	CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

static void alert_irq_handler(void)
{
	uint64_t active_alerts;
	uint64_t mask;
	int i;

	/*
	 * Alert interrupt status is reported by two 32 bit status registers.
	 * There are 47 alert sources, status of each is represented by a bit
	 * in one of the status registers. That is to say, in the second
	 * status register only 15 bits are used, the rest are returned as
	 * zeros on read.
	 *
	 * Writing a 1 into the asserted status bit resets the bit and
	 * deasserts the interrupt (in most cases it looks like, but not
	 * always...).
	 */

	/*
	 * Create a 64 bit vector of alert interrupt statuses, clearing
	 * pending ones at the same time.
	 */
	active_alerts = GREAD(GLOBALSEC, ALERT_INTR_STS1);
	GWRITE(GLOBALSEC, ALERT_INTR_STS1, (uint32_t)active_alerts);
	active_alerts <<= 32;  /* Higher numbers go into the upper half. */
	active_alerts |= GREAD(GLOBALSEC, ALERT_INTR_STS0);
	GWRITE(GLOBALSEC, ALERT_INTR_STS0, (uint32_t)active_alerts);

	if (!active_alerts)
		return;	/* All alerts must have been processed in an earlier
			   invocation. */

	CPRINTF("%s detected alert(s):", __func__);
	for (i = 0, mask = 1; active_alerts; i++, mask <<= 1) {
		if (!(active_alerts & mask))
			continue;
		active_alerts ^= mask;
		CPRINTF(" %d.%02d", i / 32, i % 32);
	}
	CPRINTF("\n");
}

/*
 * A table of interrupt source descriptors, all mapping to the same interrupt
 * servicing routine.
 *
 * The ISR iterates over all alert sources and clrears all of in one shot.
 * This clears interrupt requests from the alert module, but the interrupt
 * controller seems to latch them, so the ISR will is invoked as many times as
 * there are alert bits set.
 *
 * This is not a problem, as alerts are extremely rare events and many of them
 * are unlikely to happen concurrently.
 */
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_CAMO0_BREACH_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_CRYPTO0_DMEM_PARITY_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_CRYPTO0_DRF_PARITY_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_CRYPTO0_IMEM_PARITY_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_CRYPTO0_PGM_FAULT_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_CPU0_D_IF_BUS_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_CPU0_D_IF_UPDATE_WATCHDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_CPU0_I_IF_BUS_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_CPU0_I_IF_UPDATE_WATCHDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_CPU0_S_IF_BUS_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_CPU0_S_IF_UPDATE_WATCHDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_DDMA0_IF_BUS_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_DDMA0_IF_UPDATE_WATCHDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_DSPS0_IF_BUS_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_DSPS0_IF_UPDATE_WATCHDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_DUSB0_IF_BUS_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_DBCTRL_DUSB0_IF_UPDATE_WATCHDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_FUSE0_FUSE_DEFAULTS_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_ALERT_GROUPA_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_ALERT_GROUPB_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_ALERT_GROUPC_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_DIFF_FAIL_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_FW0_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_FW1_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_FW2_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_FW3_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_HEARTBEAT_FAIL_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_PROC_OPCODE_HASH_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_GLOBALSEC_SRAM_PARITY_SCRUB_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_AES_EXEC_CTR_MAX_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_AES_HKEY_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_CERT_LOOKUP_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_FLASH_ENTRY_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_PW_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_SHA_EXEC_CTR_MAX_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_SHA_FAULT_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_KEYMGR0_SHA_HKEY_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_PMU_BATTERY_MON_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_PMU_PMU_WDOG_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_RTC0_RTC_DEAD_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_TEMP0_MAX_TEMP_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_TEMP0_MAX_TEMP_DIFF_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_TEMP0_MIN_TEMP_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_TRNG0_OUT_OF_SPEC_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_TRNG0_TIMEOUT_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_VOLT0_VOLT_ERR_ALERT_INT,
	    alert_irq_handler, 1);
DECLARE_IRQ(GC_IRQNUM_GLOBALSEC_XO0_JITTERY_TRIM_DIS_ALERT_INT,
	    alert_irq_handler, 1);

static void alert_ints_init(void)
{
	int i;

	/* Enable all alert interrupts */
	for (i = GC_IRQNUM_GLOBALSEC_CAMO0_BREACH_ALERT_INT;
	     i <= GC_IRQNUM_GLOBALSEC_XO0_JITTERY_TRIM_DIS_ALERT_INT;
	     i++)
		task_enable_irq(i);
	CPRINTS("Alert Interrupts enabled");
}
DECLARE_HOOK(HOOK_INIT, alert_ints_init, HOOK_PRIO_DEFAULT);

static int command_alrtt(int argc, char **argv)
{
	char *e;
	int fw_shift;
	uint32_t reg_value;

	if (argc == 1) {
		/* Trigger them all. */
		GWRITE(GLOBALSEC, ALERT_FW_TRIGGER, 0x55);
		GWRITE(GLOBALSEC, ALERT_FW_TRIGGER, 0xaa);
		return EC_SUCCESS;
	}

	if (argc != 2)
		return EC_ERROR_INVAL;

	fw_shift = strtoi(argv[1], &e, 10) * 2;

	if (*e || (fw_shift < 0) || (fw_shift > 6))
		return EC_ERROR_PARAM1;

	/* Induce alert condition on the requested bit. */
	reg_value = GREAD(GLOBALSEC, ALERT_FW_TRIGGER);
	reg_value &= ~(3 << fw_shift);
	reg_value |= 1 << fw_shift;
	GWRITE(GLOBALSEC, ALERT_FW_TRIGGER, reg_value);
	reg_value &= ~(3 << fw_shift);
	reg_value |= 2 << fw_shift;
	GWRITE(GLOBALSEC, ALERT_FW_TRIGGER, reg_value);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(alerttest, command_alrtt,
			"[<FW alert number in 0..3 range>]",
			"Simulate a firmware alert, "
			"or all of them if none specified",
			NULL);
