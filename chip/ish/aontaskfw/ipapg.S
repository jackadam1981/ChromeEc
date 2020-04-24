/* 
   INTEL CONFIDENTIAL
   Copyright (c) 2015 Intel Corporation. All Rights Reserved.

   The source code contained or described herein and all documents related
   to the source code ("Material") are owned by Intel Corporation or its
   suppliers or licensors. Title to the Material remains with Intel Corporation
   or its suppliers and licensors. The Material contains trade secrets and
   proprietary and confidential information of Intel or its suppliers and
   licensors. The Material is protected by worldwide copyright and trade secret
   laws and treaty provisions. No part of the Material may be used, copied,
   reproduced, modified, published, uploaded, posted, transmitted, distributed,
   or disclosed in any way without Intel's prior express written permission.
  
   No license under any patent, copyright, trade secret or other intellectual
   property right is granted to or conferred upon you by disclosure or delivery
   of the Materials, either expressly, by implication, inducement, estoppel or
   otherwise. Any license under such intellectual property rights must be
   express and approved by Intel in writing.

*/

	//LAPIC
	.equ	LAPIC_BASE,				0xFEE00000
	.equ	LAPIC_SPUR,				(LAPIC_BASE + 0x00F0)
	.equ	LAPIC_ENABLE,			0x00000100
	.equ	LAPIC_SPUR_RESET,		0x000000FF
	
	//PMU defs
	.equ	PMU_BASE,			    0x4200000
	.equ	PMU_STATUS_REG,			(PMU_BASE + 0xF00)
	.equ	PMU_SCRATCHPAD0,		(PMU_BASE + 0xF04)
	.equ	PMU_SCRATCHPAD1,		(PMU_BASE + 0xF08)
	.equ	PMU_PG_EN_REG,			(PMU_BASE + 0xF10)

    .equ	PMU_STATUS_PG_BIT,		    (1 << 4)
    .equ	PMU_STATUS_PG_AON_BIT,	    (1 << 5)

	.equ	PMU_PG_EN_BIT,			    (1 << 0)
	.equ	PMU_PG_EXIT_COMPLETE_BIT,	(1 << 8)
	
	//IPC defs
	.equ	FWST_AON_STATE_MASK,		(0x7 << 24)
	.equ	FWST_AON_STATE_HALT,		(0x2 << 24)
	
	.equ	EFLAGS_NT,			        (1 << 14)
	
	.equ	TSS_ESP0_OFFSET,		    0x4
	.equ	TSS_LDT_SEG_SEL_OFFSET,		0x60
	
	.equ	AON_CS,				0x4
	.equ	AON_DS,				0xc
	
	
	.global ipapg
ipapg:
	push	%ebp
	push	%edi
	push	%esi
	push	%ebx
	mov	%cr0, %eax
	push	%eax
	mov	%cr4, %eax
	push	%eax
	
	//save FPU context (this will change cr0 but we have the original value on stack)
	clts
	fnsave	fpu_reg_set
	
	//write down return address for ROM
	movl	$(PMU_STATUS_PG_BIT|PMU_STATUS_PG_AON_BIT), PMU_STATUS_REG
	movl	$out_of_pg, %eax
	movl	%eax, PMU_SCRATCHPAD0
	movl	(%eax), %eax
	movl	%eax, PMU_SCRATCHPAD1
	
	//enable IPAPG, we will actually enter PG on the next halt
	movl	$(PMU_PG_EN_BIT|PMU_PG_EXIT_COMPLETE_BIT), PMU_PG_EN_REG
		
	//save esp so we can restore stack after returning from ROM
	lea	aon_tss, %eax
	movl	%esp, TSS_ESP0_OFFSET(%eax)
			
	sti
	hlt
	
	//unreachable
	
	
	//got out of IPAPG, jumped here from ROM if there was no abort condition
out_of_pg:
	cli
	
	//restore stack
	lea	aon_tss, %eax
	movl	TSS_ESP0_OFFSET(%eax), %esp
	
	//set the nested task bit in eflags
	pushfl
	orl	$EFLAGS_NT, (%esp)
	popfl
	
	//init FPU and restore context
	clts
	fninit
	frstor	fpu_reg_set
	
	//restore non-volatile registers and CR0 & CR4 (CR3 will be restored by AON beore enabling paging)
	pop	%eax
	mov	%eax, %cr4
	pop	%eax
	mov	%eax, %cr0
	pop	%ebx
	pop	%esi
	pop	%edi
	pop	%ebp
	
	//check if we're indeed after IPAPG exit
	testl	$PMU_STATUS_PG_BIT, PMU_STATUS_REG
	jz	after_pg
	
	//we didn't go through ROM, clear PG_EN bit and return an abort condition to AON 
	movl	$0, PMU_PG_EN_REG
	movl	$0, %eax
	jmp	return_to_aon
	
after_pg:
	//return to caller that we got ouf of PG
	movl	$1, %eax
	
return_to_aon:	
	movl	$0, PMU_STATUS_REG

	//return to AON task (still with ROM GDT and segments in case of PG exit)
	ret
	
	.global pg_exit_save_ctx
pg_exit_save_ctx:
	sgdtl	mainfw_gdt
	str 	tr
	ret
	
	.global pg_exit_restore_ctx
pg_exit_restore_ctx:
	
	//load Viper GDT and AON task
	lgdtl	mainfw_gdt
	ltr	tr
	
	//load AON LDT and segments
	lea	aon_tss, %eax
	lldt	TSS_LDT_SEG_SEL_OFFSET(%eax)
	mov	$AON_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	ljmpl	$AON_CS, $cont
	
cont:
	nop
	ret
