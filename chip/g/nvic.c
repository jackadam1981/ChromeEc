/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nvic.h"

/*
 * Enable interrupt
 * @param irqnum Interrupt number
 */
void nvic_irq_en(uint32_t irqnum)
{
	uint32_t iser_reg_off = (irqnum >> 5) << 2;
	uint32_t iser_reg_bit = 1 << (irqnum & 0x1F);
	uint32_t iser_addr = GC_M3_NVIC_ISER0_ADDR + iser_reg_off;
	uint32_t iser_cur_val = REG32(iser_addr);
	REG32(iser_addr) = iser_cur_val | iser_reg_bit;
}

/*
 * Disable interrupt
 * @param irqnum Interrupt number
 */
void nvic_irq_dis(uint32_t irqnum)
{
	uint32_t icer_reg_off = (irqnum >> 5) << 2;
	uint32_t icer_reg_bit = 1 << (irqnum & 0x1F);
	uint32_t icer_addr = GC_M3_NVIC_ICER0_ADDR + icer_reg_off;
	REG32(icer_addr) = icer_reg_bit;
}

/*
 * Set interrupt flag
 * @param irqnum Interrupt number
 */
void nvic_irq_flag_set(uint32_t irqnum)
{
	uint32_t ispr_reg_off = (irqnum >> 5) << 2;
	uint32_t ispr_reg_bit = 1 << (irqnum & 0x1F);
	uint32_t ispr_addr = GC_M3_NVIC_ISPR0_ADDR + ispr_reg_off;
	uint32_t ispr_cur_val = REG32(ispr_addr);
	REG32(ispr_addr) = ispr_cur_val | ispr_reg_bit;
}

/*
 * Clear interrupt flag
 * @param irqnum Interrupt number
 */
void nvic_irq_flag_clr(uint32_t irqnum)
{
	uint32_t icpr_reg_off = (irqnum >> 5) << 2;
	uint32_t icpr_reg_bit = 1 << (irqnum & 0x1F);
	uint32_t icpr_addr = GC_M3_NVIC_ICPR0_ADDR + icpr_reg_off;
	REG32(icpr_addr) = icpr_reg_bit;
}

/*
 * Add or replace an existing interrupt handler
 * @note This function assumes that VTOR is pointing to SRAM.
 * @param irqnum Interrupt number
 * @param callback Interrupt handler function
 */
void nvic_handler_set(uint32_t irqnum, void (*handler)(void))
{
	uint32_t vectab = REG32(GC_M3_BASE_ADDR + GC_M3_VTOR_OFFSET);
	/* Offset of +16 to match Cortex-M3 and
	 * NVIC IRQ numbering conventions
	 */
	REG32(vectab + ((irqnum + 16) << 2)) =  (uint32_t)handler;
}
