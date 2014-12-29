/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef INC_NVIC_H_
#define INC_NVIC_H_

#include "common.h"
#include "registers.h"

extern void nvic_irq_en(uint32_t irqnum);
extern void nvic_irq_dis(uint32_t irqnum);
extern void nvic_irq_flag_set(uint32_t irqnum);
extern void nvic_irq_flag_clr(uint32_t irqnum);
extern void nvic_handler_set(uint32_t irqnum, void (*handler)(void));

#endif /* INC_NVIC_H_ */
