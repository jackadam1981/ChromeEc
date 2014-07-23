/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef CHIP_STM32_USART_IMPL_H
#define CHIP_STM32_USART_IMPL_H

#include "chip/stm32/usart.h"

/*
 * Handle a USART interrupt.  The per-variant USART code creates bindings
 * for the variants interrupts to call this generic USART interrupt handler
 * with the appropriate usart_config.  The USART interrupts are sometimes
 * shared, so this routine may be called for an uninitialized USART.  The
 * usart_interrupt handles this by checking the irq_lock variable, which is
 * only enabled once a USART has been initialized.
 */
void usart_interrupt(usart_config const * config);

/*
 * The generic USART initialization code calls this function to allow the
 * variant specific code to perform any variant specific initialization.  This
 * function is called right before the USARTs interrupts are enabled.  However,
 * since some USARTs share an interrupt handler, it is possible that the
 * interrupt for this USART is already enabled.
 */
void usart_variant_init(usart_config const * config);

#endif //CHIP_STM32_USART_IMPL_H
