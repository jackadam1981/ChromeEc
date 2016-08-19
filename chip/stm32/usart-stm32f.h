/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_USART_STM32F_H
#define __CROS_EC_USART_STM32F_H

#include "usart.h"

/*
 * The STM32F series can have many UARTS.  These are the HW configs
 * for those UARTS.  They can be used to initialize STM32 generic UART configs.
 * stm32f0: 4 UARTs.
 * stm32f3: 3 UARTs.
 * stm32f4: 6 UARTs.
 */
extern struct usart_hw_config const usart1_hw;
extern struct usart_hw_config const usart2_hw;
extern struct usart_hw_config const usart3_hw;
#if defined(CHIP_FAMILY_STM32F0) | defined(CHIP_FAMILY_STM32F4)
extern struct usart_hw_config const usart4_hw;
#endif
#if defined(CHIP_FAMILY_STM32F4)
extern struct usart_hw_config const usart5_hw;
extern struct usart_hw_config const usart6_hw;
#endif

#endif /* __CROS_EC_USART_STM32F_H */
