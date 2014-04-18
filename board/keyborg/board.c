/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Keyborg board-specific configuration */

#include "board.h"
#include "common.h"
#include "debug.h"
#include "master_slave.h"
#include "registers.h"
#include "spi_comm.h"
#include "system.h"
#include "task.h"
#include "util.h"

int main(void)
{
	int i = 0;
	hardware_init();
	debug_printf("Keyborg starting...\n");

	master_slave_init();

	/* Set N_CHG and CS1 to high so that both chips see SPI_NSS high */
	STM32_GPIO_BSRR(GPIO_A) = (1 << 1) | (1 << 6);

	master_slave_sync(10);

	if (master_slave_is_master()) {
		spi_master_init();
	} else {
		spi_slave_init();

		/* Enable interrupt on PA0 (GPIO_SPI_NSS) */
		STM32_AFIO_EXTICR(0) &= ~0xF;
		STM32_EXTI_IMR |= (1 << 0);
		task_clear_pending_irq(STM32_IRQ_EXTI0);
		task_enable_irq(STM32_IRQ_EXTI0);
	}

	master_slave_sync(100);

	while (1) {
		i++;
		task_wait_event(SECOND);
		if (master_slave_is_master()) {
			debug_printf("Hello x 50...");
			if (spi_hello_test(50) == EC_SUCCESS)
				debug_printf("Passed\n");
			else
				debug_printf("Failed\n");
		}
	}
}
