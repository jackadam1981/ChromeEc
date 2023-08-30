/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Copyright (c) 2011 - 2013 ARM LIMITED

   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   - Neither the name of ARM nor the names of its contributors may be used
     to endorse or promote products derived from this software without
     specific prior written permission.
   *
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
   ---------------------------------------------------------------------------*/

	.syntax unified
	.arch   armv7e-m

	.section .stack
	.align  3

	.equ    Stack_Size, 0x000007FC

	.globl  __StackTop
	.globl  __StackLimit
__StackLimit:
	.space  Stack_Size
	.size   __StackLimit, . - __StackLimit
__StackTop:
	.size   __StackTop, . - __StackTop

	.section .heap
	.align  3
#ifdef __HEAP_SIZE
	.equ    Heap_Size, __HEAP_SIZE
#else
	.equ    Heap_Size, 0x0000800
#endif
	.globl  __HeapBase
	.globl  __HeapLimit
__HeapBase:
	.if Heap_Size
	.space  Heap_Size
	.endif
	.size   __HeapBase, . - __HeapBase
__HeapLimit:
	.size   __HeapLimit, . - __HeapLimit

/******************************************************************************
*
* The minimal vector table for a Cortex-M4.  Note that the proper constructs
* must be placed on this to ensure that it ends up at a properly aligned
* address. Cortex-M4 requires the table alignemnt is to the next power of 2
* greater than the size of the table.
* MEC172x has 181 external + 16 internal sources = 197 words.
* The next power of 2 number of words = 256.
* Alignment is 256 words or 1024 bytes.
*
******************************************************************************/
	.section .isr_vector,"a",%progbits
	.globl   __isr_vector
	.balign  256, 0xff
__isr_vector:
	.long __StackTop		/* Top of Stack */
	.long Reset_Handler		/* Reset Handler */
	.long NMI_Handler		/* NMI Handler */
	.long HardFault_Handler		/* Hard Fault Handler */
	.long MemManage_Handler		/* MPU Fault Handler */
	.long BusFault_Handler		/* Bus Fault Handler */
	.long UsageFault_Handler	/* Usage Fault Handler */
	.long 0				/* Reserved */
	.long 0				/* Reserved */
	.long 0				/* Reserved */
	.long 0				/* Reserved */
	.long SVC_Handler		/* SVCall Handler */
	.long DebugMon_Handler		/* Debug Monitor Handler */
	.long 0				/* Reserved */
	.long PendSV_Handler		/* PendSV Handler */
	.long SysTick_Handler		/* SysTick Handler */

	/* MEC172x External interrupts */
	.long GIRQ08_IRQHandler		/* NVIC 0, GPIO_0140 - 0177 */
	.long GIRQ09_IRQHandler		/* NVIC 1, GPIO_0100 - 0137 */
	.long GIRQ10_IRQHandler		/* NVIC 2, GPIO_0040 - 0077 */
	.long GIRQ11_IRQHandler		/* NVIC 3, GPIO_0000 - 0037 */
	.long GIRQ12_IRQHandler		/* NVIC 4, GPIO_0200 - 0237 */
	.long GIRQ13_IRQHandler		/* NVIC 5, Direct capable */
	.long GIRQ14_IRQHandler		/* NVIC 6, Direct capable */
	.long GIRQ15_IRQHandler		/* NVIC 7, Direct capable */
	.long GIRQ16_IRQHandler		/* NVIC 8, Direct capable */
	.long GIRQ17_IRQHandler		/* NVIC 9, Direct capable */
	.long GIRQ18_IRQHandler		/* NVIC 10, Direct capable */
	.long GIRQ19_IRQHandler		/* NVIC 11, Direct capable */
	.long GIRQ20_IRQHandler		/* NVIC 12, */
	.long GIRQ21_IRQHandler		/* NVIC 13, Direct capable */
/* NOTE GIRQ22 is peripheral wake only and is not routed to NVIC */
	.long GIRQ23_IRQHandler		/* NVIC 14, Direct capable */
	.long GIRQ24_IRQHandler		/* NVIC 15, */
	.long GIRQ25_IRQHandler		/* NVIC 16, */
	.long GIRQ26_IRQHandler		/* NVIC 17, */
	.long 0				/* NVIC 18, Reserved */
	.long 0				/* NVIC 19, Reserved */
	.long SMB0_IRQHandler		/* NVIC 20,  GIRQ13 b[0] */
	.long SMB1_IRQHandler		/* NVIC 21,  GIRQ13 b[1] */
	.long SMB2_IRQHandler		/* NVIC 22,  GIRQ13 b[2] */
	.long SMB3_IRQHandler		/* NVIC 23,  GIRQ13 b[3] */
	.long DMA0_IRQHandler		/* NVIC 24, GIRQ14 b[0] */
	.long DMA1_IRQHandler		/* NVIC 25, GIRQ14 b[1] */
	.long DMA2_IRQHandler		/* NVIC 26, GIRQ14 b[2] */
	.long DMA3_IRQHandler		/* NVIC 27, GIRQ14 b[3] */
	.long DMA4_IRQHandler		/* NVIC 28, GIRQ14 b[4] */
	.long DMA5_IRQHandler		/* NVIC 29, GIRQ14 b[5] */
	.long DMA6_IRQHandler		/* NVIC 30, GIRQ14 b[6] */
	.long DMA7_IRQHandler		/* NVIC 31, GIRQ14 b[7] */
	.long DMA8_IRQHandler		/* NVIC 32, GIRQ14 b[8] */
	.long DMA9_IRQHandler		/* NVIC 33, GIRQ14 b[9] */
	.long DMA10_IRQHandler		/* NVIC 34, GIRQ14 b[10] */
	.long DMA11_IRQHandler		/* NVIC 35, GIRQ14 b[11] */
	.long DMA12_IRQHandler		/* NVIC 36, GIRQ14 b[12] */
	.long DMA13_IRQHandler		/* NVIC 37, GIRQ14 b[13] */
	.long DMA14_IRQHandler		/* NVIC 38, GIRQ14 b[14] */
	.long DMA15_IRQHandler		/* NVIC 39, GIRQ14 b[15] */
	.long UART0_IRQHandler		/* NVIC 40, GIRQ15 b[0] */
	.long UART1_IRQHandler		/* NVIC 41, GIRQ15 b[1] */
	.long EMI0_IRQHandler		/* NVIC 42, GIRQ15 b[2] */
	.long EMI1_IRQHandler		/* NVIC 43, GIRQ15 b[3] */
	.long EMI2_IRQHandler		/* NVIC 44, GIRQ15 b[4] */
	.long AEC0_IBF_IRQHandler	/* NVIC 45, GIRQ15 b[5] */
	.long AEC0_OBE_IRQHandler	/* NVIC 46, GIRQ15 b[6] */
	.long AEC1_IBF_IRQHandler	/* NVIC 47, GIRQ15 b[7] */
	.long AEC1_OBE_IRQHandler	/* NVIC 48, GIRQ15 b[8] */
	.long AEC2_IBF_IRQHandler	/* NVIC 49, GIRQ15 b[9] */
	.long AEC2_OBE_IRQHandler	/* NVIC 50, GIRQ15 b[10] */
	.long AEC3_IBF_IRQHandler	/* NVIC 51, GIRQ15 b[11] */
	.long AEC3_OBE_IRQHandler	/* NVIC 52, GIRQ15 b[12] */
	.long AEC4_IBF_IRQHandler	/* NVIC 53, GIRQ15 b[13] */
	.long AEC4_OBE_IRQHandler	/* NVIC 54, GIRQ15 b[14] */
	.long APM1_CTL_IRQHandler	/* NVIC 55, GIRQ15 b[15] */
	.long APM1_EN_IRQHandler	/* NVIC 56, GIRQ15 b[16] */
	.long APM1_STS_IRQHandler	/* NVIC 57, GIRQ15 b[17] */
	.long KBC_OBE_IRQHandler	/* NVIC 58, GIRQ15 b[18] */
	.long KBC_IBF_IRQHandler	/* NVIC 59, GIRQ15 b[19] */
	.long MBOX_H2EC_IRQHandler	/* NVIC 60, GIRQ15 b[20] */
	.long 0				/* NVIC 61, Reserved */
	.long BDP0_IRQHandler		/* NVIC 62, GIRQ15 b[22] */
	.long 0				/* NVIC 63, Reserved */
	.long LASIC_IRQHandler		/* NVIC 64, GIRQ15 b[24] */
	.long PK_IRQHandler		/* NVIC 65, GIRQ16 b[0] */
	.long 0				/* NVIC 66, Reserved */
	.long TRNG_IRQHandler		/* NVIC 67, GIRQ16 b[2] */
	.long SXDMA_IRQHandler		/* NVIC 68, GIRQ16 b[3] */
	.long 0				/* NVIC 69, Reserved */
	.long PECI_IRQHandler		/* NVIC 70, GIRQ17 b[0] */
	.long TACH0_IRQHandler		/* NVIC 71, GIRQ17 b[1] */
	.long TACH1_IRQHandler		/* NVIC 72, GIRQ17 b[2] */
	.long TACH2_IRQHandler		/* NVIC 73, GIRQ17 b[3] */
	.long R2P0_FAIL_IRQHandler	/* NVIC 74, GIRQ17 b[20] */
	.long R2P0_STALL_IRQHandler	/* NVIC 75, GIRQ17 b[21] */
	.long R2P1_FAIL_IRQHandler	/* NVIC 76, GIRQ17 b[22] */
	.long R2P1_STALL_IRQHandler	/* NVIC 77, GIRQ17 b[23] */
	.long ADC_SNGL_IRQHandler	/* NVIC 78, GIRQ17 b[8] */
	.long ADC_RPT_IRQHandler	/* NVIC 79, GIRQ17 b[9] */
	.long RCID0_IRQHandler		/* NVIC 80, GIRQ17 b[10] */
	.long RCID1_IRQHandler		/* NVIC 81, GIRQ17 b[11] */
	.long RCID2_IRQHandler		/* NVIC 82, GIRQ17 b[12] */
	.long LED0_IRQHandler		/* NVIC 83, GIRQ17 b[13] */
	.long LED1_IRQHandler		/* NVIC 84, GIRQ17 b[14] */
	.long LED2_IRQHandler		/* NVIC 85, GIRQ17 b[15] */
	.long LED3_IRQHandler		/* NVIC 86, GIRQ17 b[16] */
	.long PROCHOT_IRQHandler	/* NVIC 87, GIRQ17 b[17] */
	.long PWRGRD0_IRQHandler	/* NVIC 88, GIRQ17 b[18] */
	.long PWRGRD1_IRQHandler	/* NVIC 89, GIRQ17 b[19] */
	.long SLVSPI_IRQHandler		/* NVIC 90, GIRQ18 b[0] */
	.long QMSPI_IRQHandler		/* NVIC 91, GIRQ18 b[1] */
	.long GPSPI0_TXBE_IRQHandler	/* NVIC 92, GIRQ18 b[2] */
	.long GPSPI0_RXBF_IRQHandler	/* NVIC 93, GIRQ18 b[3] */
	.long GPSPI1_TXBE_IRQHandler	/* NVIC 94, GIRQ18 b[4] */
	.long GPSPI1_RXBF_IRQHandler	/* NVIC 95, GIRQ18 b[5] */
	.long BCL0_BCLR_IRQHandler	/* NVIC 96, GIRQ18 b[6] */
	.long BCL0_BERR_IRQHandler	/* NVIC 97, GIRQ18 b[7] */
	.long 0				/* NVIC 98, Reserved */
	.long 0				/* NVIC 99, Reserved */
	.long PS2_0_IRQHandler		/* NVIC 100, GIRQ18 b[10] */
	.long 0				/* NVIC 101, Reserved */
	.long 0				/* NVIC 102, Reserved */
	.long ESPI_PC_IRQHandler	/* NVIC 103, GIRQ19 b[0] */
	.long ESPI_BM1_IRQHandler	/* NVIC 104, GIRQ19 b[1] */
	.long ESPI_BM2_IRQHandler	/* NVIC 105, GIRQ19 b[2] */
	.long ESPI_LTR_IRQHandler	/* NVIC 106, GIRQ19 b[3] */
	.long ESPI_OOB_UP_IRQHandler	/* NVIC 107, GIRQ19 b[4] */
	.long ESPI_OOB_DN_IRQHandler	/* NVIC 108, GIRQ19 b[5] */
	.long ESPI_FC_IRQHandler	/* NVIC 109, GIRQ19 b[6] */
	.long ESPI_RESET_IRQHandler	/* NVIC 110, GIRQ19 b[7] */
	.long RTMR_IRQHandler		/* NVIC 111, GIRQ23 b[10] */
	.long HTMR0_IRQHandler		/* NVIC 112, GIRQ23 b[16] */
	.long HTMR1_IRQHandler		/* NVIC 113, GIRQ23 b[17] */
	.long WKT_IRQHandler		/* NVIC 114, GIRQ21 b[3] */
	.long WKT_SUB_IRQHandler	/* NVIC 115, GIRQ21 b[4] */
	.long WKT_SEC_IRQHandler	/* NVIC 116, GIRQ21 b[5] */
	.long WKT_SUBSEC_IRQHandler	/* NVIC 117, GIRQ21 b[6] */
	.long WKT_SYSPWR_IRQHandler	/* NVIC 118, GIRQ21 b[7] */
	.long RTC_IRQHandler		/* NVIC 119, GIRQ21 b[8] */
	.long RTC_ALARM_IRQHandler	/* NVIC 120, GIRQ21 b[9] */
	.long VCI_OVRD_IN_IRQHandler	/* NVIC 121, GIRQ21 b[10] */
	.long VCI_IN0_IRQHandler	/* NVIC 122, GIRQ21 b[11] */
	.long VCI_IN1_IRQHandler	/* NVIC 123, GIRQ21 b[12] */
	.long VCI_IN2_IRQHandler	/* NVIC 124, GIRQ21 b[13] */
	.long VCI_IN3_IRQHandler	/* NVIC 125, GIRQ21 b[14] */
	.long VCI_IN4_IRQHandler	/* NVIC 126, GIRQ21 b[15] */
	.long 0				/* NVIC 127, Reserved */
	.long 0				/* NVIC 128, Reserved */
	.long PS2_0A_WAKE_IRQHandler	/* NVIC 129, GIRQ21 b[18] */
	.long PS2_0B_WAKE_IRQHandler	/* NVIC 130, GIRQ21 b[19] */
	.long 0				/* NVIC 131, Reserved */
	.long 0				/* NVIC 132, Reserved */
	.long 0				/* NVIC 133, Reserved */
	.long ENVMON_IRQHandler		/* NVIC 134, GIRQ21 b[24] */
	.long KEYSCAN_IRQHandler	/* NVIC 135, GIRQ21 b[25] */
	.long BTMR0_IRQHandler		/* NVIC 136, GIRQ23 b[0] */
	.long BTMR1_IRQHandler		/* NVIC 137, GIRQ23 b[1] */
	.long BTMR2_IRQHandler		/* NVIC 138, GIRQ23 b[2] */
	.long BTMR3_IRQHandler		/* NVIC 139, GIRQ23 b[3] */
	.long BTMR4_IRQHandler		/* NVIC 140, GIRQ23 b[4] */
	.long BTMR5_IRQHandler		/* NVIC 141, GIRQ23 b[5] */
	.long CTMR0_IRQHandler		/* NVIC 142, GIRQ23 b[6] */
	.long CTMR1_IRQHandler		/* NVIC 143, GIRQ23 b[7] */
	.long CTMR2_IRQHandler		/* NVIC 144, GIRQ23 b[8] */
	.long CTMR3_IRQHandler		/* NVIC 145, GIRQ23 b[9] */
	.long CCT_IRQHandler		/* NVIC 146, GIRQ18 b[20] */
	.long CCT_CAP0_IRQHandler	/* NVIC 147, GIRQ18 b[21] */
	.long CCT_CAP1_IRQHandler	/* NVIC 148, GIRQ18 b[22] */
	.long CCT_CAP2_IRQHandler	/* NVIC 149, GIRQ18 b[23] */
	.long CCT_CAP3_IRQHandler	/* NVIC 150, GIRQ18 b[24] */
	.long CCT_CAP4_IRQHandler	/* NVIC 151, GIRQ18 b[25] */
	.long CCT_CAP5_IRQHandler	/* NVIC 152, GIRQ18 b[26] */
	.long CCT_CMP0_IRQHandler	/* NVIC 153, GIRQ18 b[27] */
	.long CCT_CMP1_IRQHandler	/* NVIC 154, GIRQ18 b[28] */
	.long EEPROM_CTRL_IRQHandler	/* NVIC 155, GIRQ18 b[13] */
	.long ESPI_VW_CHEN_IRQHandler	/* NVIC 156, GIRQ19 b[8] */
	.long 0				/* NVIC 157, Reserved */
	.long SMB4_IRQHandler		/* NVIC 158, GIRQ13 b[4] */
	.long TACH3_IRQHandler		/* NVIC 159, GIRQ17 b[4] */
	.long 0				/* NVIC 160, Reserved */
	.long 0				/* NVIC 161, Reserved */
	.long 0				/* NVIC 162, Reserved */
	.long 0				/* NVIC 163, Reserved */
	.long 0				/* NVIC 164, Reserved */
	.long 0				/* NVIC 165, Reserved */
	.long SAFS_DONE_IRQHandler	/* NVIC 166, GIRQ19 b[9] */
	.long SAFS_ERR_IRQHandler	/* NVIC 167, GIRQ19 b[10] */
	.long 0				/* NVIC 168, Reserved */
	.long SAFS_CACHE_IRQHandler	/* NVIC 169, GIRQ19 b[11] */
	.long 0				/* NVIC 170, Reserved */
	.long WDT_IRQHandler		/* NVIC 171, GIRQ21 b[2] */
	.long GLUE_IRQHandler		/* NVIC 172, GIRQ23 b[26] */
	.long OTP_IRQHandler		/* NVIC 173, GIRQ20 b[3] */
	.long CLK_32K_MON_IRQHandler	/* NVIC 174, GIRQ20 b[9] */
	.long ACPI_EC0_IRQHandler	/* NVIC 175, No GIRQ */
	.long ACPI_EC1_IRQHandler	/* NVIC 176, No GIRQ */
	.long ACPI_EC2_IRQHandler	/* NVIC 177, No GIRQ */
	.long ACPI_EC3_IRQHandler	/* NVIC 178, No GIRQ */
	.long ACPI_EC4_IRQHandler	/* NVIC 179, No GIRQ */
	.long ACPI_PM1_IRQHandler	/* NVIC 180, No GIRQ */

	.size __isr_vector, . - __isr_vector

	.text
	.thumb
	.thumb_func
	.align  2
	.globl  Reset_Handler
	.type   Reset_Handler, %function
Reset_Handler:
/*
 * On Vector or chip reset vector table is 0 (Boot-ROM)
 * We load our application vector table into Cortex-M3/M4 VTOR
 * register at 0xE000ED08.
 * We load SP with stack pointer from out vector table.
 */
	ldr	r2, =__isr_vector
	ldr	r3, =0xE000ED08
	str	r2, [r3]
	dsb
	isb
	ldr	r3, [r2]
	msr	msp, r3
	isb	0xf
	msr	psp, r3
	isb	0xf

#ifdef STACK_SENTINEL
/*
 *  Help diagnose stack underflow by filling the stack with
 *  a sentinel value.
 */
	ldr	r0, =0x57AC5E91 /* STACK_SENTINEL_VAL */
	ldr	r1, =__StackLimit
	ldr	r2, =__StackTop
.L_loop_ss0:
	str	r0, [r1], #4
	cmp	r1, r2
	bne	.L_loop_ss0
#endif

/*
 *  Firstly it copies data from read only memory to RAM. There are two schemes
 *  to copy. One can copy more than one sections. Another can only copy
 *  one section.  The former scheme needs more instructions and read-only
 *  data to implement than the latter.
 *  Macro __STARTUP_COPY_MULTIPLE is used to choose between two schemes.  */

#ifdef __STARTUP_COPY_MULTIPLE
/*
 *  Multiple sections scheme.
 *
 *  Between symbol address __copy_table_start__ and __copy_table_end__,
 *  there are array of triplets, each of which specify:
 *    offset 0: LMA of start of a section to copy from
 *    offset 4: VMA of start of a section to copy to
 *    offset 8: size of the section to copy. Must be multiply of 4
 *
 *  All addresses must be aligned to 4 bytes boundary.
 */
	ldr	r4, =__copy_table_start__
	ldr	r5, =__copy_table_end__

.L_loop0:
	cmp	r4, r5
	bge	.L_loop0_done
	ldr	r1, [r4]
	ldr	r2, [r4, #4]
	ldr	r3, [r4, #8]

.L_loop0_0:
	subs	r3, #4
	ittt	ge
	ldrge	r0, [r1, r3]
	strge	r0, [r2, r3]
	bge	.L_loop0_0

	adds	r4, #12
	b	.L_loop0

.L_loop0_done:
#else
/*
 * Single section scheme.
 *
 *  The ranges of copy from/to are specified by following symbols
 *    __etext: LMA of start of the section to copy from. Usually end of text
 *    __data_start__: VMA of start of the section to copy to
 *    __data_end__: VMA of end of the section to copy to
 *
 *  All addresses must be aligned to 4 bytes boundary.
 */
	ldr	r1, =__etext
	ldr	r2, =__data_start__
	ldr	r3, =__data_end__

.L_loop1:
	cmp	r2, r3
	ittt	lt
	ldrlt	r0, [r1], #4
	strlt	r0, [r2], #4
	blt	.L_loop1
#endif /*__STARTUP_COPY_MULTIPLE */

/*
 * Single BSS section scheme.
 *
 *  The BSS section is specified by following symbols
 *    __bss_start__: start of the BSS section.
 *    __bss_end__: end of the BSS section.
 *
 *  Both addresses must be aligned to 4 bytes boundary.
 */
	ldr	r1, =__bss_start__
	ldr	r2, =__bss_end__

	movs	r0, 0
.L_loop3:
	cmp	r1, r2
	itt	lt
	strlt	r0, [r1], #4
	blt	.L_loop3

	bl	main

	.pool
	.size	Reset_Handler, . - Reset_Handler

	.balign 8
	.thumb_func
	.weak	Default_Handler
	.type	Default_Handler, %function
Default_Handler:
#ifdef __DEFAULT_ISR
	b 	__DEFAULT_ISR
#else
	b	.
#endif
	.size	Default_Handler, . - Default_Handler

/*
 * Macro to define default handlers. Default handler
 * will be weak symbol and just dead loops. They can be
 * overwritten by other handlers
 */
	.macro	def_irq_handler handler_name
	.weak	\handler_name
	.set	\handler_name, Default_Handler
	.endm

	def_irq_handler		NMI_Handler
	def_irq_handler		HardFault_Handler
	def_irq_handler		MemManage_Handler
	def_irq_handler		BusFault_Handler
	def_irq_handler		UsageFault_Handler
	def_irq_handler		SVC_Handler
	def_irq_handler		DebugMon_Handler
	def_irq_handler		PendSV_Handler
	def_irq_handler		SysTick_Handler

/* MEC172x External Interrupt Handlers */
	def_irq_handler		GIRQ08_IRQHandler
	def_irq_handler		GIRQ09_IRQHandler
	def_irq_handler		GIRQ10_IRQHandler
	def_irq_handler		GIRQ11_IRQHandler
	def_irq_handler		GIRQ12_IRQHandler
	def_irq_handler		GIRQ13_IRQHandler
	def_irq_handler		GIRQ14_IRQHandler
	def_irq_handler		GIRQ15_IRQHandler
	def_irq_handler		GIRQ16_IRQHandler
	def_irq_handler		GIRQ17_IRQHandler
	def_irq_handler		GIRQ18_IRQHandler
	def_irq_handler		GIRQ19_IRQHandler
	def_irq_handler		GIRQ20_IRQHandler
	def_irq_handler		GIRQ21_IRQHandler
	def_irq_handler		GIRQ23_IRQHandler
	def_irq_handler		GIRQ24_IRQHandler
	def_irq_handler		GIRQ25_IRQHandler
	def_irq_handler		GIRQ26_IRQHandler
	def_irq_handler		SMB0_IRQHandler
	def_irq_handler		SMB1_IRQHandler
	def_irq_handler		SMB2_IRQHandler
	def_irq_handler		SMB3_IRQHandler
	def_irq_handler		DMA0_IRQHandler
	def_irq_handler		DMA1_IRQHandler
	def_irq_handler		DMA2_IRQHandler
	def_irq_handler		DMA3_IRQHandler
	def_irq_handler		DMA4_IRQHandler
	def_irq_handler		DMA5_IRQHandler
	def_irq_handler		DMA6_IRQHandler
	def_irq_handler		DMA7_IRQHandler
	def_irq_handler		DMA8_IRQHandler
	def_irq_handler		DMA9_IRQHandler
	def_irq_handler		DMA10_IRQHandler
	def_irq_handler		DMA11_IRQHandler
	def_irq_handler		DMA12_IRQHandler
	def_irq_handler		DMA13_IRQHandler
	def_irq_handler		DMA14_IRQHandler
	def_irq_handler		DMA15_IRQHandler
	def_irq_handler		UART0_IRQHandler
	def_irq_handler		UART1_IRQHandler
	def_irq_handler		EMI0_IRQHandler
	def_irq_handler		EMI1_IRQHandler
	def_irq_handler		EMI2_IRQHandler
	def_irq_handler		AEC0_IBF_IRQHandler
	def_irq_handler		AEC0_OBE_IRQHandler
	def_irq_handler		AEC1_IBF_IRQHandler
	def_irq_handler		AEC1_OBE_IRQHandler
	def_irq_handler		AEC2_IBF_IRQHandler
	def_irq_handler		AEC2_OBE_IRQHandler
	def_irq_handler		AEC3_IBF_IRQHandler
	def_irq_handler		AEC3_OBE_IRQHandler
	def_irq_handler		AEC4_IBF_IRQHandler
	def_irq_handler		AEC4_OBE_IRQHandler
	def_irq_handler		APM1_CTL_IRQHandler
	def_irq_handler		APM1_EN_IRQHandler
	def_irq_handler		APM1_STS_IRQHandler
	def_irq_handler		KBC_OBE_IRQHandler
	def_irq_handler		KBC_IBF_IRQHandler
	def_irq_handler		MBOX_H2EC_IRQHandler
	def_irq_handler		BDP0_IRQHandler
	def_irq_handler		LASIC_IRQHandler
	def_irq_handler		PK_IRQHandler
	def_irq_handler		TRNG_IRQHandler
	def_irq_handler		SXDMA_IRQHandler
	def_irq_handler		PECI_IRQHandler
	def_irq_handler		TACH0_IRQHandler
	def_irq_handler		TACH1_IRQHandler
	def_irq_handler		TACH2_IRQHandler
	def_irq_handler		R2P0_FAIL_IRQHandler
	def_irq_handler		R2P0_STALL_IRQHandler
	def_irq_handler		R2P1_FAIL_IRQHandler
	def_irq_handler		R2P1_STALL_IRQHandler
	def_irq_handler		ADC_SNGL_IRQHandler
	def_irq_handler		ADC_RPT_IRQHandler
	def_irq_handler		RCID0_IRQHandler
	def_irq_handler		RCID1_IRQHandler
	def_irq_handler		RCID2_IRQHandler
	def_irq_handler		LED0_IRQHandler
	def_irq_handler		LED1_IRQHandler
	def_irq_handler		LED2_IRQHandler
	def_irq_handler		LED3_IRQHandler
	def_irq_handler		PROCHOT_IRQHandler
	def_irq_handler		PWRGRD0_IRQHandler
	def_irq_handler		PWRGRD1_IRQHandler
	def_irq_handler		SLVSPI_IRQHandler
	def_irq_handler		QMSPI_IRQHandler
	def_irq_handler		GPSPI0_TXBE_IRQHandler
	def_irq_handler		GPSPI0_RXBF_IRQHandler
	def_irq_handler		GPSPI1_TXBE_IRQHandler
	def_irq_handler		GPSPI1_RXBF_IRQHandler
	def_irq_handler		BCL0_BCLR_IRQHandler
	def_irq_handler		BCL0_BERR_IRQHandler
	def_irq_handler		PS2_0_IRQHandler
	def_irq_handler		ESPI_PC_IRQHandler
	def_irq_handler		ESPI_BM1_IRQHandler
	def_irq_handler		ESPI_BM2_IRQHandler
	def_irq_handler		ESPI_LTR_IRQHandler
	def_irq_handler		ESPI_OOB_UP_IRQHandler
	def_irq_handler		ESPI_OOB_DN_IRQHandler
	def_irq_handler		ESPI_FC_IRQHandler
	def_irq_handler		ESPI_RESET_IRQHandler
	def_irq_handler		RTMR_IRQHandler
	def_irq_handler		HTMR0_IRQHandler
	def_irq_handler		HTMR1_IRQHandler
	def_irq_handler		WKT_IRQHandler
	def_irq_handler		WKT_SUB_IRQHandler
	def_irq_handler		WKT_SEC_IRQHandler
	def_irq_handler		WKT_SUBSEC_IRQHandler
	def_irq_handler		WKT_SYSPWR_IRQHandler
	def_irq_handler		RTC_IRQHandler
	def_irq_handler		RTC_ALARM_IRQHandler
	def_irq_handler		VCI_OVRD_IN_IRQHandler
	def_irq_handler		VCI_IN0_IRQHandler
	def_irq_handler		VCI_IN1_IRQHandler
	def_irq_handler		VCI_IN2_IRQHandler
	def_irq_handler		VCI_IN3_IRQHandler
	def_irq_handler		VCI_IN4_IRQHandler
	def_irq_handler		PS2_0A_WAKE_IRQHandler
	def_irq_handler		PS2_0B_WAKE_IRQHandler
	def_irq_handler		ENVMON_IRQHandler
	def_irq_handler		KEYSCAN_IRQHandler
	def_irq_handler		BTMR0_IRQHandler
	def_irq_handler		BTMR1_IRQHandler
	def_irq_handler		BTMR2_IRQHandler
	def_irq_handler		BTMR3_IRQHandler
	def_irq_handler		BTMR4_IRQHandler
	def_irq_handler		BTMR5_IRQHandler
	def_irq_handler		CTMR0_IRQHandler
	def_irq_handler		CTMR1_IRQHandler
	def_irq_handler		CTMR2_IRQHandler
	def_irq_handler		CTMR3_IRQHandler
	def_irq_handler		CCT_IRQHandler
	def_irq_handler		CCT_CAP0_IRQHandler
	def_irq_handler		CCT_CAP1_IRQHandler
	def_irq_handler		CCT_CAP2_IRQHandler
	def_irq_handler		CCT_CAP3_IRQHandler
	def_irq_handler		CCT_CAP4_IRQHandler
	def_irq_handler		CCT_CAP5_IRQHandler
	def_irq_handler		CCT_CMP0_IRQHandler
	def_irq_handler		CCT_CMP1_IRQHandler
	def_irq_handler		EEPROM_CTRL_IRQHandler
	def_irq_handler		ESPI_VW_CHEN_IRQHandler
	def_irq_handler		SMB4_IRQHandler
	def_irq_handler		TACH3_IRQHandler
	def_irq_handler		SAFS_DONE_IRQHandler
	def_irq_handler		SAFS_ERR_IRQHandler
	def_irq_handler		SAFS_CACHE_IRQHandler
	def_irq_handler		WDT_IRQHandler
	def_irq_handler		GLUE_IRQHandler
	def_irq_handler		OTP_IRQHandler
	def_irq_handler		CLK_32K_MON_IRQHandler
	def_irq_handler		ACPI_EC0_IRQHandler
	def_irq_handler		ACPI_EC1_IRQHandler
	def_irq_handler		ACPI_EC2_IRQHandler
	def_irq_handler		ACPI_EC3_IRQHandler
	def_irq_handler		ACPI_EC4_IRQHandler
	def_irq_handler		ACPI_PM1_IRQHandler

	.end
