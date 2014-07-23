/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USART driver for Chrome EC */

#include "chip/stm32/usart.h"
#include "chip/stm32/usart-impl.h"

#include "atomic.h"
#include "gpio.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "util.h"

static size_t usart_read(in_stream const * stream,
			 uint8_t * buffer,
			 size_t count)
{
	usart_config const * config = DOWNCAST(stream, usart_config, in);

	return fifo_read(&config->tx, &config->state->tx, buffer, count);
}

static size_t usart_write(out_stream const * stream,
			  uint8_t * buffer,
			  size_t count)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);
	size_t               wrote;

	wrote = fifo_write(&config->tx, &config->state->tx, buffer, count);

	task_trigger_irq(config->hw->irq);

	return wrote;
}

static void usart_flush(out_stream const * stream)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);

	while (fifo_count(&config->tx, &config->state->tx))
	{
		if (in_interrupt_context())
			usart_interrupt(config);
		else
			task_trigger_irq(config->hw->irq);
	}
}

static int usart_pause(out_stream const * stream)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);
	int                  paused = config->state->paused;

	//STM32_USART_CR1(base) &= ~STM32_USART_CR1_TXEIE;

	config->state->paused = 1;

	enable_sleep(SLEEP_MASK_UART);

	return paused;
}

static int usart_resume(out_stream const * stream)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);
	int                  paused = config->state->paused;

	disable_sleep(SLEEP_MASK_UART);

	config->state->paused = 0;

	//STM32_USART_CR1(base) |= STM32_USART_CR1_TXEIE;

	task_trigger_irq(config->hw->irq);

	return paused;
}

static int usart_paused(out_stream const * stream)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);

	return config->state->paused;
}

in_stream_ops const usart_in_stream_ops =
{
	.read = usart_read,
};

out_stream_ops const usart_out_stream_ops =
{
	.write  = usart_write,
	.flush  = usart_flush,
	.pause  = usart_pause,
	.resume = usart_resume,
	.paused = usart_paused,
};

void usart_init(usart_config const * config)
{
	intptr_t base = config->hw->base;

	fifo_init(&config->tx, &config->state->tx);
	fifo_init(&config->rx, &config->state->rx);

	config->state->paused = 0;

	*(config->hw->clock_register) = config->hw->clock_enable;

	/* Configure GPIOs */
	gpio_config_module(MODULE_UART, 1);

	/*
	 * UART enabled, 8 Data bits, oversampling x16, no parity,
	 * TX and RX enabled.
	 */
	STM32_USART_CR1(base) =
		STM32_USART_CR1_UE | STM32_USART_CR1_TE | STM32_USART_CR1_RE;

	/* 1 stop bit, no fancy stuff */
	STM32_USART_CR2(base) = 0x0000;

	/* DMA disabled, special modes disabled, error interrupt disabled */
	STM32_USART_CR3(base) = 0x0000;

	/* Enable receive-not-empty interrupt */
	STM32_USART_CR1(base) |= STM32_USART_CR1_RXNEIE;

	usart_variant_init(config);

	/*
	 * Now that the USART has been initialized it is safe to enable the
	 * irq_lock.  Once this is set calls to the usart_interrupt function
	 * will work as expected.
	 */
	config->state->irq_lock = 1;

	task_enable_irq(config->hw->irq);
}

void usart_interrupt(usart_config const * config)
{
	intptr_t base = config->hw->base;

	/*
	 * Prevent pre-mature and reentrant calls.  These could result from
	 * shared interrupts (as in the STM32F072 USART3 and USART4 sharing
	 * a single interrupt) or because of an ongoing flush operation that
	 * collides with a normal interrupt.
	 */
	if (atomic_read_clear((uint32_t *) &config->state->irq_lock) == 0)
		return;

	if (STM32_USART_SR(base) & STM32_USART_SR_TXE)
	{
		uint8_t byte;

		/*
		 * Disable the TX empty interrupt before filling the TX buffer
		 * since it needs an actual write to DR to be cleared.
		 */
		STM32_USART_CR1(base) &= ~STM32_USART_CR1_TXEIE;

		if (fifo_read(&config->tx, &config->state->tx, &byte, 1))
		{
			STM32_USART_TDR(base) = byte;
		}

		/*
		 * Re-enable TX empty interrupt only if it was not disabled by
		 * uart_process_output().
		 */
		if (!config->state->paused)
			STM32_USART_CR1(base) |= STM32_USART_CR1_TXEIE;
	}

	if (STM32_USART_SR(base) & STM32_USART_SR_RXNE)
	{
		uint8_t byte = STM32_USART_RDR(base);

		fifo_write(&config->rx, &config->state->rx, &byte, 1);
	}

	/*
	 * We're done, re-enable the irq_lock.
	 *
	 * TODO: does this even work?  Can we deadlock?
	 */
	config->state->irq_lock = 1;
}
