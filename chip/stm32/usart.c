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

	return queue_remove_units(&config->rx, buffer, count);
}

static size_t usart_write(out_stream const * stream,
			  uint8_t const * buffer,
			  size_t count)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);
	size_t               wrote  = queue_add_units(&config->tx,
						      buffer,
						      count);

	/*
	 * Trigger the USART interrupt.  This causes the USART interrupt
	 * handler to start fetching from the TX queue if it wasn't already,
	 * and if it is, this just results in an extra triggering of the USART
	 * interrupt, that will be harmless.
	 */
	task_trigger_irq(config->hw->irq);

	return wrote;
}

static void usart_flush(out_stream const * stream)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);

	/*
	 * We're not going to make any progress if we are paused, the paused
	 * state wins over the flush request and we just return.
	 */
	if (config->state->paused)
		return;

	while (queue_count(&config->tx))
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

	config->state->paused = 1;

	/*
	 * We explicitly enable deep sleep mode here because it is not
	 * guaranteed that a USART interrupt will happen in a timely manner.
	 */
	enable_sleep(SLEEP_MASK_UART);

	return paused;
}

static int usart_resume(out_stream const * stream)
{
	usart_config const * config = DOWNCAST(stream, usart_config, out);
	int                  paused = config->state->paused;

	config->state->paused = 0;

	/*
	 * Trigger the USART interrupt.  This causes the USART interrupt
	 * handler to start fetching from the TX queue.  If the queue is empty
	 * the TXE interrupt will remain disabled.  But transmission will start
	 * again when a write causes another manual triggering of the USART
	 * interrupt.
	 */
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

	queue_init(&config->tx);
	queue_init(&config->rx);

	config->state->paused = 0;

	/*
	 * Enable clock to USART, this must be done first, before attempting
	 * to configure the USART.
	 */
	*(config->hw->clock_register) |= config->hw->clock_enable;

	/*
	 * Switch all GPIOs assigned to the USART module over to their USART
	 * alternate functions.
	 */
	gpio_config_module(MODULE_USART, 1);

	/*
	 * 8N1, 16 samples per bit, enable TX and RX (and associated RX
	 * interrupt) DMA, error interrupts, and special modes disabled.
	 */
	STM32_USART_CR1(base) = (STM32_USART_CR1_TE |
				 STM32_USART_CR1_RE |
				 STM32_USART_CR1_RXNEIE);
	STM32_USART_CR2(base) = 0x0000;
	STM32_USART_CR3(base) = 0x0000;

	usart_variant_init(config);

	/*
	 * Finally, enable the USART, this must be done last since most of the
	 * configuration bits require that the USART be disabled for writes to
	 * succeed.
	 */
	STM32_USART_CR1(base) |= STM32_USART_CR1_UE;

	/*
	 * Now that the USART has been initialized it is safe to enable the
	 * irq_lock.  Once this is set calls to the usart_interrupt function
	 * will work as expected.
	 */
	config->state->irq_lock = 1;

	task_enable_irq(config->hw->irq);
}

static void usart_interrupt_tx(usart_config const * config)
{
	intptr_t base = config->hw->base;
	uint8_t  byte;

	if (!config->state->paused && queue_remove_units(&config->tx, &byte, 1))
	{
		STM32_USART_TDR(base) = byte;

		out_stream_ready(&config->out);

		/*
		 * Make sure the TXE interrupt is enabled and that we won't go
		 * into deep sleep.  This invocation of the USART interrupt
		 * handler may have been manually triggered to start
		 * transmission.
		 */
		disable_sleep(SLEEP_MASK_UART);

		STM32_USART_CR1(base) |= STM32_USART_CR1_TXEIE;
	}
	else
	{
		/*
		 * The TX queue is empty, or we have been asked to pause.  In
		 * either case, disable the TXE interrupt and enable deep sleep
		 * mode. The TXE interrupt will remain disabled until either it
		 * is unpaused or a write call happens.  If it is unpaused and
		 * the queue is empty, the transmit interrupt will once again be
		 * disabled after a single interrupt.  Similarly, if a write
		 * call happens while the USART is paused it will trigger a
		 * single interrupt which will again leave the transmit
		 * interrupt disabled.
		 */
		enable_sleep(SLEEP_MASK_UART);

		STM32_USART_CR1(base) &= ~STM32_USART_CR1_TXEIE;
	}
}

static void usart_interrupt_rx(usart_config const * config)
{
	intptr_t base    = config->hw->base;
	uint8_t  byte    = STM32_USART_RDR(base);
	uint32_t dropped = 1 - queue_add_units(&config->rx, &byte, 1);

	atomic_add((uint32_t *) &config->state->rx_dropped, dropped);

	/*
	 * Wake up whoever is listening on the other end of the queue.  The
	 * queue_add_units call above may have failed due to a full queue, but
	 * it doesn't really matter to the ready callback because there will be
	 * something in the queue to consume either way.
	 */
	in_stream_ready(&config->in);
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
	if (!config->state->irq_lock)
		return;

//	if (atomic_read_clear((uint32_t *) &config->state->irq_lock) == 0)
//		return;

	if (STM32_USART_SR(base) & STM32_USART_SR_TXE)
		usart_interrupt_tx(config);

	if (STM32_USART_SR(base) & STM32_USART_SR_RXNE)
		usart_interrupt_rx(config);

	/*
	 * We're done, re-enable the irq_lock.
	 *
	 * TODO: does this even work?  Can we deadlock?
	 */
//	config->state->irq_lock = 1;
}
