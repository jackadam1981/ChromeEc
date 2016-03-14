/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Raw keyboard I/O layer for MEC1322
 */

#include "gpio.h"
#include "keyboard_config.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void keyboard_raw_init(void)
{
	keyboard_raw_enable_interrupt(0);
	gpio_config_module(MODULE_KEYBOARD_SCAN, 1);

	/* Enable keyboard scan interrupt */
	MEC1322_INT_ENABLE(17) |= 1 << 21;
	MEC1322_INT_BLK_EN |= 1 << 17;
	MEC1322_KS_KSI_INT_EN = 0xff;
}

void keyboard_raw_task_start(void)
{
	task_enable_irq(MEC1322_IRQ_KSC_INT);
}

test_mockable void keyboard_raw_drive_column(int out)
{
	if (out == KEYBOARD_COLUMN_ALL) {
		MEC1322_KS_KSO_SEL = 1 << 5; /* KSEN=0, KSALL=1 */
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
		gpio_set_level(GPIO_KBD_KSO2, 1);
#endif
	} else if (out == KEYBOARD_COLUMN_NONE) {
		MEC1322_KS_KSO_SEL = 1 << 6; /* KSEN=1 */
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
		gpio_set_level(GPIO_KBD_KSO2, 0);
#endif
	} else {
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
		if (out == 2) {
			MEC1322_KS_KSO_SEL = 1 << 6; /* KSEN=1 */
			gpio_set_level(GPIO_KBD_KSO2, 1);
		} else {
			MEC1322_KS_KSO_SEL = out + CONFIG_KEYBOARD_KSO_BASE;
			gpio_set_level(GPIO_KBD_KSO2, 0);
		}
#else
		MEC1322_KS_KSO_SEL = out + CONFIG_KEYBOARD_KSO_BASE;
#endif
	}
}

test_mockable int keyboard_raw_read_rows(void)
{
	/* Invert it so 0=not pressed, 1=pressed */
	return (MEC1322_KS_KSI_INPUT & 0xff) ^ 0xff;
}

void keyboard_raw_enable_interrupt(int enable)
{
	if (enable) {
		task_clear_pending_irq(MEC1322_IRQ_KSC_INT);
		task_enable_irq(MEC1322_IRQ_KSC_INT);
	} else {
		task_disable_irq(MEC1322_IRQ_KSC_INT);
	}
}

void keyboard_raw_interrupt(void)
{
	/* Clear interrupt status bits */
	MEC1322_KS_KSI_STATUS = 0xff;

	/* Wake keyboard scan task to handle interrupt */
	task_wake(TASK_ID_KEYSCAN);
}
DECLARE_IRQ(MEC1322_IRQ_KSC_INT, keyboard_raw_interrupt, 1);

int kso_ksi_short_scan(void)
{
	int i, j, k, g, val;
	uint32_t port;
	uint8_t r = 0;

	const int pins[][2] = {
				{12, 5}, {12, 6}, {14, 4}, {3, 2}, {14, 2},
				{4, 0}, {4, 2}, {4, 3},	{0, 0}, {10, 0},
				{10, 2}, {10, 3}, {10, 4}, {0, 1}, {0, 2},
				{0, 3}, {10, 6}, {0, 4}, {10, 7}, {0, 5},
	};

	keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_CLOSED);

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
		port = pins[i][0];
		j = pins[i][1];
		gpio_set_alternate_function(port, 1 << j, -1);
		gpio_set_flags_by_mask(port, 1 << j,
			GPIO_INPUT | GPIO_PULL_UP);
	}

	for (k = 0; k < ARRAY_SIZE(pins); k++) {
		port = pins[k][0];
		j = pins[k][1];
		gpio_set_flags_by_mask(port, 1 << j, GPIO_OUT_LOW);

		for (g = 0; g < ARRAY_SIZE(pins); g++) {
			if (k != g) {
				val = MEC1322_GPIO_CTL(pins[g][0], pins[g][1]);
				if ((val & (1 << 24)) == 0) {
					r = 1;
					goto out;
				}
			}
		}
		gpio_set_flags_by_mask(port, 1 << j,
			GPIO_INPUT | GPIO_PULL_UP);
	}
out:
	gpio_config_module(MODULE_KEYBOARD_SCAN, 1);
	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_CLOSED);

	return r;
}
