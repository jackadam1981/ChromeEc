/*
 * Copyright 2021 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ite_it8xxx2_cros_kb_raw

#include <assert.h>
/* include <dt-bindings/clock/npcx_clock.h> */
#include <drivers/cros_kb_raw.h>
#include <drivers/clock_control.h>
#include <drivers/gpio.h>
#include <kernel.h>
#include <soc.h>
/* include <soc/nuvoton_npcx/reg_def_cros.h> need? */

#include "ec_tasks.h"
#include "keyboard_raw.h"
#include "task.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(cros_kb_raw, LOG_LEVEL_ERR);

#define ITE_MAX_KEY_COLS 18 /* Maximum rows of keyboard matrix */
#define ITE_MAX_KEY_ROWS 8 /* Maximum columns of keyboard matrix */

#define IT83XX_IRQ_WKINTC 13 /* need declare again? */

/* Device config */
struct cros_kb_raw_ite_config {
	/* keyboard scan controller base address */
	uintptr_t base;
	/* Keyboard scan input (KSI) wake-up irq */
	int irq;
};

static int kb_raw_ite_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Clock default is on */
	return 0;
}

/* Cros ec keyboard raw api functions */
static int cros_kb_raw_ite_enable_interrupt(const struct device *dev,
					    int enable)
{
	ARG_UNUSED(dev);

	if (enable) {
		WUESR3 = 0xFF;
		ite_intc_isr_clear(IT83XX_IRQ_WKINTC); //task_clear_pending_irq(IT83XX_IRQ_WKINTC);
		irq_enable(IT83XX_IRQ_WKINTC); //task_enable_irq(IT83XX_IRQ_WKINTC);
	} else
		irq_disable(IT83XX_IRQ_WKINTC); //task_disable_irq(IT83XX_IRQ_WKINTC);

	return 0;
}

static int cros_kb_raw_ite_read_row(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Bits are active-low, so invert returned levels */
	return KSI ^ 0xff;
}

static int cros_kb_raw_ite_drive_column(const struct device *dev, int col)
{
	int mask;
	unsigned int key;

	ARG_UNUSED(dev);

	/* Tri-state all outputs */
	if (col == KEYBOARD_COLUMN_NONE)
		mask = 0xffff;
	/* Assert all outputs */
	else if (col == KEYBOARD_COLUMN_ALL)
		mask = 0;
	/* Assert a single output */
	else
		mask = 0xffff ^ BIT(col);
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
	/* KSO[2] is inverted. */
	mask ^= BIT(2);
#endif
	KSOL = mask & 0xff;
	/* critical section with interrupts off */
	key = irq_lock(); //int_mask = read_clear_int_mask();
	/*
	 * Because IT83XX_KBS_KSOH1 register is shared by keyboard scan
	 * out and GPIO output mode, so we don't drive all KSOH pins
	 * here (this depends on how many keyboard matrix output pin
	 * we are using).
	 */
	KSOH1 = (KSOH1 & ~KSOH_PIN_MASK) | ((mask >> 8) & KSOH_PIN_MASK);
	//KSOH2 = 0x00; /*KSO[17:16] output data*/
	/* restore interrupts */
	irq_unlock(key); //set_int_mask(int_mask);

	return 0;
}

static void cros_kb_raw_ite_ksi_isr(const struct device *dev)
{
	ARG_UNUSED(dev);

	WUESR3 = 0xFF;
	ite_intc_isr_clear(IT83XX_IRQ_WKINTC); //task_clear_pending_irq(IT83XX_IRQ_WKINTC);

	LOG_DBG("%s: KSI%d is changed", __func__, wui->bit);
	/* Wake-up keyboard scan task */
	task_wake(TASK_ID_KEYSCAN);
}

static int cros_kb_raw_ite_init(const struct device *dev)
{
	unsigned int key;

	/* Ensure top-level interrupt is disabled */
	cros_kb_raw_ite_enable_interrupt(dev ,0);

	/*
	 * bit2, Setting 1 enables the internal pull-up of the KSO[15:0] pins.
	 * To pull up KSO[17:16], set the GPCR registers of their
	 * corresponding GPIO ports.
	 * bit0, Setting 1 enables the open-drain mode of the KSO[17:0] pins.
	 */
	KSOCTRL = 0x05;
	/* bit2, 1 enables the internal pull-up of the KSI[7:0] pins. */
	KSICTRL = 0x04;
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
	/* KSO[2] is high, others are low. */
	KSOL = BIT(2);
	/* Enable KSO2's push-pull */
	KSOLGCTRL |= BIT(2);
	KSOLGOEN |= BIT(2);
#else
	/* KSO[7:0] pins low. */
	KSOL = 0x00;
#endif
	/* critical section with interrupts off */
	key = irq_lock(); //int_mask = read_clear_int_mask();
	/*
	 * KSO[COLS_MAX:8] pins low.
	 * NOTE: KSO[15:8] pins can part be enabled for keyboard function and
	 *		 rest be configured as GPIO output mode. In this case that we
	 *		 disable the ISR in critical section to avoid race condition.
	 */
	KSOH1 &= ~KSOH_PIN_MASK;
	/* restore interrupts */
	irq_unlock(key); //set_int_mask(int_mask);
	/* KSI[0-7] falling-edge triggered is selected */
	WUEMR3 = 0xFF;
	/* W/C */
	WUESR3 = 0xFF;
	ite_intc_isr_clear(IT83XX_IRQ_WKINTC); //task_clear_pending_irq(IT83XX_IRQ_WKINTC);
	/* Enable WUC for KSI[0-7] */
	WUENR3 = 0xFF;

	IRQ_CONNECT(IT83XX_IRQ_WKINTC, 0, cros_kb_raw_ite_ksi_isr, NULL, 0);

	return 0;
}

static const struct cros_kb_raw_driver_api cros_kb_raw_ite_driver_api = {
	.init = cros_kb_raw_ite_init,
	.drive_colum = cros_kb_raw_ite_drive_column,
	.read_rows = cros_kb_raw_ite_read_row,
	.enable_interrupt = cros_kb_raw_ite_enable_interrupt,
};

static const struct cros_kb_raw_ite_config cros_kb_raw_cfg = {
	.base = DT_INST_REG_ADDR(0),
	.irq = DT_INST_IRQN(0), /*= IT83XX_IRQ_WKINTC 13, use config->irq or IT83XX_IRQ_WKINTC*/
};

DEVICE_DEFINE(cros_kb_raw_npcx_0, DT_INST_LABEL(0), kb_raw_ite_init, NULL,
	      NULL, &cros_kb_raw_cfg, PRE_KERNEL_1,
	      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
	      &cros_kb_raw_ite_driver_api);
