	.text
	.syntax unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.cpu	cortex-m4
	.eabi_attribute	6, 13	@ Tag_CPU_arch
	.eabi_attribute	7, 77	@ Tag_CPU_arch_profile
	.eabi_attribute	8, 0	@ Tag_ARM_ISA_use
	.eabi_attribute	9, 2	@ Tag_THUMB_ISA_use
	.fpu	fpv4-sp-d16
	.eabi_attribute	17, 1	@ Tag_ABI_PCS_GOT_use
	.eabi_attribute	20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute	21, 1	@ Tag_ABI_FP_exceptions
	.eabi_attribute	23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute	34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute	24, 1	@ Tag_ABI_align_needed
	.eabi_attribute	25, 1	@ Tag_ABI_align_preserved
	.eabi_attribute	27, 1	@ Tag_ABI_HardFP_use
	.eabi_attribute	28, 0	@ Tag_ABI_VFP_args
	.eabi_attribute	36, 1	@ Tag_FP_HP_extension
	.eabi_attribute	38, 1	@ Tag_ABI_FP_16bit_format
	.eabi_attribute	18, 2	@ Tag_ABI_PCS_wchar_t
	.eabi_attribute	26, 1	@ Tag_ABI_enum_size
	.eabi_attribute	14, 0	@ Tag_ABI_PCS_R9_use
	.file	"common/curve25519.c"
	.globl	X25519
	.p2align	1
	.type	X25519,%function
	.code	16                      @ @X25519
	.thumb_func
X25519:
	.fnstart
@ BB#0:
	.save	{r4, r6, r7, lr}
	push	{r4, r6, r7, lr}
	.setfp	r7, sp, #8
	add	r7, sp, #8
	mov	r4, r0
	bl	x25519_scalar_mult
	movw	r0, :lower16:X25519.kZeros
	mov	r1, r4
	movs	r2, #32
	movt	r0, :upper16:X25519.kZeros
	bl	memcmp
	cmp	r0, #0
	it	ne
	movne	r0, #1
	pop	{r4, r6, r7, pc}
.Lfunc_end0:
	.size	X25519, .Lfunc_end0-X25519
	.cantunwind
	.fnend

	.p2align	1
	.type	x25519_scalar_mult,%function
	.code	16                      @ @x25519_scalar_mult
	.thumb_func
x25519_scalar_mult:
	.fnstart
@ BB#0:
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	.setfp	r7, sp, #12
	add	r7, sp, #12
	.save	{r8, r9, r10, r11}
	push.w	{r8, r9, r10, r11}
	.pad	#572
	sub.w	sp, sp, #572
	mov	r4, r2
	str	r0, [sp, #4]            @ 4-byte Spill
	add	r0, sp, #100
	movs	r2, #32
	bl	memcpy
	ldrb.w	r0, [sp, #100]
	movs	r6, #1
	mov.w	r8, #0
	and	r0, r0, #248
	strb.w	r0, [sp, #100]
	ldrb.w	r0, [sp, #131]
	bfi	r0, r6, #6, #26
	strb.w	r0, [sp, #131]
	ldrb	r0, [r4]
	ldrb	r1, [r4, #1]
	ldrb	r2, [r4, #2]
	ldrb	r3, [r4, #3]
	orr.w	r0, r0, r1, lsl #8
	ldrb	r1, [r4, #30]
	orr.w	r0, r0, r2, lsl #16
	ldrb	r2, [r4, #29]
	orr.w	r0, r0, r3, lsl #24
	orr.w	r1, r2, r1, lsl #8
	ldrb	r2, [r4, #31]
	orr.w	r1, r1, r2, lsl #16
	movw	r2, #65532
	movt	r2, #511
	and.w	r1, r2, r1, lsl #2
	str	r1, [sp, #96]           @ 4-byte Spill
	add.w	r1, r1, #16777216
	str	r1, [sp, #92]           @ 4-byte Spill
	sub.w	r1, r8, r1, lsr #25
	and	r1, r1, #19
	adds	r2, r1, r0
	ldrb	r0, [r4, #28]
	adc	r10, r8, #0
	adds.w	r12, r2, #33554432
	and	r3, r12, #-67108864
	sub.w	r5, r2, r3
	lsr.w	r3, r12, #26
	str	r0, [sp, #88]           @ 4-byte Spill
	ldrb	r0, [r4, #27]
	str	r0, [sp, #84]           @ 4-byte Spill
	ldrb	r0, [r4, #26]
	str	r0, [sp, #80]           @ 4-byte Spill
	ldrb	r0, [r4, #25]
	str	r0, [sp, #76]           @ 4-byte Spill
	ldrb	r0, [r4, #24]
	str	r0, [sp, #72]           @ 4-byte Spill
	ldrb	r0, [r4, #23]
	str	r0, [sp, #68]           @ 4-byte Spill
	ldrb	r0, [r4, #22]
	str	r0, [sp, #64]           @ 4-byte Spill
	ldrb	r0, [r4, #21]
	str	r0, [sp, #60]           @ 4-byte Spill
	ldrb	r0, [r4, #20]
	str	r0, [sp, #56]           @ 4-byte Spill
	ldrb	r0, [r4, #19]
	str	r0, [sp, #52]           @ 4-byte Spill
	ldrb	r0, [r4, #18]
	str	r0, [sp, #48]           @ 4-byte Spill
	ldrb	r0, [r4, #17]
	str	r0, [sp, #44]           @ 4-byte Spill
	ldrb	r0, [r4, #16]
	str	r0, [sp, #40]           @ 4-byte Spill
	ldrb	r0, [r4, #15]
	str	r0, [sp, #36]           @ 4-byte Spill
	ldrb	r0, [r4, #14]
	str	r0, [sp, #32]           @ 4-byte Spill
	ldrb	r0, [r4, #13]
	str	r0, [sp, #28]           @ 4-byte Spill
	ldrb	r0, [r4, #12]
	str	r0, [sp, #24]           @ 4-byte Spill
	ldrb	r0, [r4, #11]
	str	r0, [sp, #20]           @ 4-byte Spill
	ldrb.w	r11, [r4, #10]
	ldrb.w	r9, [r4, #9]
	ldrb.w	lr, [r4, #8]
	ldrb	r2, [r4, #7]
	ldrb	r1, [r4, #6]
	ldrb	r0, [r4, #5]
	ldrb	r4, [r4, #4]
	str	r5, [sp, #372]
	adc	r5, r10, #0
	orr.w	r3, r3, r5, lsl #6
	orr.w	r0, r4, r0, lsl #8
	mov.w	r4, #16777216
	orr.w	r2, r2, lr, lsl #8
	orr.w	r0, r0, r1, lsl #16
	orr.w	r2, r2, r9, lsl #16
	adds.w	r1, r4, r0, lsl #6
	and	r5, r1, #2113929216
	lsr.w	r1, r1, #25
	rsb	r0, r5, r0, lsl #6
	add	r0, r3
	str	r0, [sp, #376]
	adc	r0, r8, #0
	orr.w	r1, r1, r0, lsl #7
	lsrs	r0, r0, #25
	adds.w	r1, r1, r2, lsl #5
	adc	r0, r0, #0
	adds.w	r2, r1, #33554432
	and	r3, r2, #-67108864
	adc	r0, r0, #0
	subs	r1, r1, r3
	str	r1, [sp, #380]
	lsrs	r1, r2, #26
	ldr	r2, [sp, #24]           @ 4-byte Reload
	orr.w	r0, r1, r0, lsl #6
	ldr	r1, [sp, #20]           @ 4-byte Reload
	orr.w	r1, r11, r1, lsl #8
	orr.w	r1, r1, r2, lsl #16
	add.w	r0, r0, r1, lsl #3
	adds.w	r1, r4, r1, lsl #3
	and	r2, r1, #234881024
	lsr.w	r1, r1, #25
	sub.w	r0, r0, r2
	str	r0, [sp, #384]
	adc	r0, r8, #0
	ldrd	r3, r2, [sp, #28]
	orr.w	r1, r1, r0, lsl #7
	lsrs	r0, r0, #25
	orr.w	r2, r3, r2, lsl #8
	ldr	r3, [sp, #36]           @ 4-byte Reload
	orr.w	r2, r2, r3, lsl #16
	adds.w	r1, r1, r2, lsl #2
	adc	r0, r0, #0
	adds.w	r2, r1, #33554432
	and	r3, r2, #-67108864
	adc	r0, r0, #0
	subs	r1, r1, r3
	str	r1, [sp, #388]
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	ldrd	r2, r1, [sp, #40]
	orr.w	r1, r2, r1, lsl #8
	ldr	r2, [sp, #48]           @ 4-byte Reload
	orr.w	r1, r1, r2, lsl #16
	ldr	r2, [sp, #52]           @ 4-byte Reload
	orr.w	r1, r1, r2, lsl #24
	add	r0, r1
	adds.w	r1, r1, #16777216
	and	r2, r1, #-33554432
	lsr.w	r1, r1, #25
	sub.w	r0, r0, r2
	str	r0, [sp, #392]
	adc	r0, r8, #0
	orr.w	r0, r1, r0, lsl #7
	ldrd	r2, r1, [sp, #56]
	orr.w	r1, r2, r1, lsl #8
	ldr	r2, [sp, #64]           @ 4-byte Reload
	orr.w	r1, r1, r2, lsl #16
	adds.w	r0, r0, r1, lsl #7
	adc	r1, r8, #0
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	str	r0, [sp, #396]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	ldrd	r2, r1, [sp, #68]
	orr.w	r1, r2, r1, lsl #8
	ldr	r2, [sp, #76]           @ 4-byte Reload
	orr.w	r1, r1, r2, lsl #16
	add.w	r0, r0, r1, lsl #5
	adds.w	r1, r4, r1, lsl #5
	and	r2, r1, #1040187392
	lsr.w	r1, r1, #25
	sub.w	r0, r0, r2
	str	r0, [sp, #400]
	adc	r0, r8, #0
	ldrd	r3, r2, [sp, #80]
	orr.w	r1, r1, r0, lsl #7
	lsrs	r0, r0, #25
	orr.w	r2, r3, r2, lsl #8
	ldr	r3, [sp, #88]           @ 4-byte Reload
	orr.w	r2, r2, r3, lsl #16
	adds.w	r1, r1, r2, lsl #4
	adc	r0, r0, #0
	adds.w	r2, r1, #33554432
	and	r3, r2, #-67108864
	adc	r0, r0, #0
	subs	r1, r1, r3
	str	r1, [sp, #404]
	lsrs	r1, r2, #26
	movs	r2, #40
	orr.w	r0, r1, r0, lsl #6
	ldr	r1, [sp, #96]           @ 4-byte Reload
	add	r0, r1
	ldr	r1, [sp, #92]           @ 4-byte Reload
	and	r1, r1, #33554432
	subs	r0, r0, r1
	movs	r1, #0
	str	r0, [sp, #408]
	add	r0, sp, #332
	bl	memset
	add	r0, sp, #292
	movs	r1, #0
	movs	r2, #40
	str	r6, [sp, #332]
	bl	memset
	add	r0, sp, #252
	add	r1, sp, #372
	movs	r2, #40
	bl	memmove
	add	r0, sp, #212
	movs	r1, #0
	movs	r2, #40
	bl	memset
	movs	r0, #254
	str	r6, [sp, #212]
	b	.LBB1_2
.LBB1_1:                                @   in Loop: Header=BB1_2 Depth=1
	ldr	r6, [sp, #212]
	ldr.w	r8, [sp, #92]           @ 4-byte Reload
	subs	r0, #1
.LBB1_2:                                @ =>This Inner Loop Header: Depth=1
	str	r0, [sp, #96]           @ 4-byte Spill
	add	r5, sp, #100
	ldr	r1, [sp, #332]
	ldr	r2, [sp, #252]
	ldr	r0, [sp, #96]           @ 4-byte Reload
	eor.w	r3, r2, r1
	lsrs	r0, r0, #3
	ldrb	r0, [r5, r0]
	ldr	r5, [sp, #96]           @ 4-byte Reload
	and	r5, r5, #7
	lsrs	r0, r5
	and	r0, r0, #1
	str	r0, [sp, #92]           @ 4-byte Spill
	eor.w	r0, r0, r8
	rsbs	r0, r0, #0
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #332]
	eor.w	r1, r3, r2
	str	r1, [sp, #252]
	ldr	r1, [sp, #336]
	ldr	r2, [sp, #256]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #336]
	eor.w	r1, r3, r2
	str	r1, [sp, #256]
	ldr	r1, [sp, #340]
	ldr	r2, [sp, #260]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #340]
	eor.w	r1, r3, r2
	str	r1, [sp, #260]
	ldr	r1, [sp, #344]
	ldr	r2, [sp, #264]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #344]
	eor.w	r1, r3, r2
	str	r1, [sp, #264]
	ldr	r1, [sp, #348]
	ldr	r2, [sp, #268]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #348]
	eor.w	r1, r3, r2
	str	r1, [sp, #268]
	ldr	r1, [sp, #352]
	ldr	r2, [sp, #272]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #352]
	eor.w	r1, r3, r2
	str	r1, [sp, #272]
	ldr	r1, [sp, #356]
	ldr	r2, [sp, #276]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #356]
	eor.w	r1, r3, r2
	str	r1, [sp, #276]
	ldr	r1, [sp, #360]
	ldr	r2, [sp, #280]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #360]
	eor.w	r1, r3, r2
	str	r1, [sp, #280]
	ldr	r1, [sp, #364]
	ldr	r2, [sp, #284]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #364]
	eor.w	r1, r3, r2
	str	r1, [sp, #284]
	ldr	r1, [sp, #368]
	ldr	r2, [sp, #288]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #368]
	eor.w	r1, r3, r2
	str	r1, [sp, #288]
	ldr	r1, [sp, #292]
	eor.w	r2, r1, r6
	ands	r2, r0
	eors	r1, r2
	eor.w	r9, r2, r6
	str	r1, [sp, #292]
	str.w	r9, [sp, #212]
	ldr	r1, [sp, #296]
	ldr	r2, [sp, #216]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	eor.w	lr, r3, r2
	str	r1, [sp, #296]
	str.w	lr, [sp, #216]
	ldr	r1, [sp, #300]
	ldr	r2, [sp, #220]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	eor.w	r8, r3, r2
	str.w	r8, [sp, #220]
	str	r1, [sp, #300]
	str	r1, [sp, #88]           @ 4-byte Spill
	ldr	r1, [sp, #304]
	ldr	r2, [sp, #224]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	eor.w	r12, r3, r2
	str.w	r12, [sp, #224]
	str	r1, [sp, #304]
	str	r1, [sp, #84]           @ 4-byte Spill
	ldr	r1, [sp, #308]
	ldr	r2, [sp, #228]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	eor.w	r10, r3, r2
	str.w	r10, [sp, #228]
	str	r1, [sp, #308]
	str	r1, [sp, #32]           @ 4-byte Spill
	ldr	r1, [sp, #312]
	ldr	r2, [sp, #232]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	eor.w	r11, r3, r2
	str.w	r11, [sp, #232]
	str	r1, [sp, #312]
	str	r1, [sp, #40]           @ 4-byte Spill
	ldr	r1, [sp, #316]
	ldr	r2, [sp, #236]
	eor.w	r3, r2, r1
	and.w	r6, r3, r0
	eors	r1, r6
	eor.w	r3, r6, r2
	str	r3, [sp, #236]
	str	r1, [sp, #316]
	str	r1, [sp, #36]           @ 4-byte Spill
	ldr	r1, [sp, #320]
	ldr	r2, [sp, #240]
	eor.w	r6, r2, r1
	ands	r6, r0
	eors	r1, r6
	eors	r2, r6
	str	r1, [sp, #320]
	str	r2, [sp, #240]
	str	r1, [sp, #28]           @ 4-byte Spill
	ldr	r6, [sp, #324]
	ldr	r1, [sp, #244]
	eor.w	r5, r1, r6
	ands	r5, r0
	eor.w	r4, r5, r6
	eors	r1, r5
	str	r4, [sp, #324]
	str	r1, [sp, #244]
	str	r4, [sp, #24]           @ 4-byte Spill
	ldr	r5, [sp, #328]
	ldr	r6, [sp, #248]
	eor.w	r4, r6, r5
	ands	r4, r0
	eor.w	r0, r4, r6
	eors	r4, r5
	str	r4, [sp, #20]           @ 4-byte Spill
	str	r4, [sp, #328]
	ldr	r4, [sp, #252]
	str	r0, [sp, #248]
	str	r4, [sp, #80]           @ 4-byte Spill
	sub.w	r4, r4, r9
	ldr.w	r9, [sp, #352]
	str	r4, [sp, #172]
	ldr	r4, [sp, #256]
	sub.w	r6, r4, lr
	ldr.w	lr, [sp, #360]
	str	r4, [sp, #76]           @ 4-byte Spill
	str	r6, [sp, #176]
	ldr	r6, [sp, #260]
	str	r6, [sp, #72]           @ 4-byte Spill
	sub.w	r6, r6, r8
	ldr.w	r8, [sp, #348]
	str	r6, [sp, #180]
	ldr	r6, [sp, #264]
	str	r6, [sp, #68]           @ 4-byte Spill
	sub.w	r6, r6, r12
	ldr.w	r12, [sp, #356]
	str	r6, [sp, #184]
	ldr	r6, [sp, #268]
	str	r6, [sp, #64]           @ 4-byte Spill
	sub.w	r6, r6, r10
	str	r6, [sp, #188]
	ldr	r6, [sp, #272]
	str	r6, [sp, #60]           @ 4-byte Spill
	sub.w	r6, r6, r11
	str	r6, [sp, #192]
	ldr	r6, [sp, #276]
	subs	r3, r6, r3
	str	r6, [sp, #56]           @ 4-byte Spill
	ldr	r6, [sp, #340]
	str	r3, [sp, #196]
	ldr	r3, [sp, #280]
	subs	r2, r3, r2
	str	r3, [sp, #52]           @ 4-byte Spill
	ldr	r3, [sp, #336]
	str	r2, [sp, #200]
	ldr	r2, [sp, #284]
	subs	r1, r2, r1
	str	r2, [sp, #48]           @ 4-byte Spill
	str	r1, [sp, #204]
	ldr	r1, [sp, #288]
	subs	r0, r1, r0
	str	r1, [sp, #44]           @ 4-byte Spill
	ldr	r1, [sp, #332]
	str	r0, [sp, #208]
	ldr	r0, [sp, #292]
	str	r1, [sp, #12]           @ 4-byte Spill
	str	r0, [sp, #16]           @ 4-byte Spill
	subs	r0, r1, r0
	ldr	r1, [sp, #84]           @ 4-byte Reload
	str	r0, [sp, #132]
	ldr	r0, [sp, #296]
	str	r0, [sp, #8]            @ 4-byte Spill
	subs	r0, r3, r0
	str	r0, [sp, #136]
	ldr	r0, [sp, #88]           @ 4-byte Reload
	subs	r0, r6, r0
	str	r0, [sp, #140]
	ldr	r0, [sp, #344]
	subs	r1, r0, r1
	str	r0, [sp, #88]           @ 4-byte Spill
	str	r1, [sp, #144]
	ldr	r1, [sp, #32]           @ 4-byte Reload
	sub.w	r2, r8, r1
	ldr	r1, [sp, #40]           @ 4-byte Reload
	str	r2, [sp, #148]
	ldr	r2, [sp, #24]           @ 4-byte Reload
	sub.w	r5, r9, r1
	ldr	r1, [sp, #36]           @ 4-byte Reload
	str	r5, [sp, #152]
	sub.w	r5, r12, r1
	ldr	r1, [sp, #28]           @ 4-byte Reload
	str	r5, [sp, #156]
	sub.w	r5, lr, r1
	ldr	r1, [sp, #20]           @ 4-byte Reload
	str	r5, [sp, #160]
	ldr	r5, [sp, #364]
	sub.w	r10, r5, r2
	str.w	r10, [sp, #164]
	ldr.w	r10, [sp, #368]
	sub.w	r11, r10, r1
	str.w	r11, [sp, #168]
	ldrd	r0, r4, [sp, #12]
	add	r4, r0
	ldr	r0, [sp, #8]            @ 4-byte Reload
	str	r4, [sp, #332]
	add	r3, r0
	ldr	r0, [sp, #88]           @ 4-byte Reload
	str	r3, [sp, #336]
	ldr	r3, [sp, #300]
	add	r3, r6
	str	r3, [sp, #340]
	ldr	r3, [sp, #304]
	add	r0, r3
	str	r0, [sp, #344]
	ldr	r0, [sp, #308]
	add	r0, r8
	str	r0, [sp, #348]
	ldr	r0, [sp, #312]
	add	r0, r9
	str	r0, [sp, #352]
	ldr	r0, [sp, #316]
	add	r0, r12
	str	r0, [sp, #356]
	ldr	r0, [sp, #320]
	add	r0, lr
	str	r0, [sp, #360]
	adds	r0, r5, r2
	add	r2, sp, #332
	str	r0, [sp, #364]
	add.w	r0, r10, r1
	ldr	r1, [sp, #80]           @ 4-byte Reload
	mov	r6, r2
	str	r0, [sp, #368]
	ldr	r0, [sp, #212]
	add	r0, r1
	ldr	r1, [sp, #76]           @ 4-byte Reload
	str	r0, [sp, #292]
	ldr	r0, [sp, #216]
	add	r0, r1
	ldr	r1, [sp, #72]           @ 4-byte Reload
	str	r0, [sp, #296]
	ldr	r0, [sp, #220]
	add	r0, r1
	ldr	r1, [sp, #68]           @ 4-byte Reload
	str	r0, [sp, #300]
	ldr	r0, [sp, #224]
	add	r0, r1
	ldr	r1, [sp, #64]           @ 4-byte Reload
	str	r0, [sp, #304]
	ldr	r0, [sp, #228]
	add	r0, r1
	ldr	r1, [sp, #60]           @ 4-byte Reload
	str	r0, [sp, #308]
	ldr	r0, [sp, #232]
	add	r0, r1
	ldr	r1, [sp, #56]           @ 4-byte Reload
	str	r0, [sp, #312]
	ldr	r0, [sp, #236]
	add	r0, r1
	ldr	r1, [sp, #52]           @ 4-byte Reload
	str	r0, [sp, #316]
	ldr	r0, [sp, #240]
	add	r0, r1
	ldr	r1, [sp, #48]           @ 4-byte Reload
	str	r0, [sp, #320]
	ldr	r0, [sp, #244]
	add	r0, r1
	ldr	r1, [sp, #44]           @ 4-byte Reload
	str	r0, [sp, #324]
	ldr	r0, [sp, #248]
	add	r0, r1
	add	r1, sp, #172
	str	r0, [sp, #328]
	add	r0, sp, #212
	mov	r4, r1
	bl	fe_mul
	add	r0, sp, #292
	add	r2, sp, #132
	mov	r1, r0
	mov	r5, r2
	bl	fe_mul
	mov	r0, r4
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r6
	bl	fe_sq
	ldr	r1, [sp, #212]
	ldr	r0, [sp, #292]
	ldr.w	r8, [sp, #232]
	ldr.w	r12, [sp, #312]
	ldr	r2, [sp, #320]
	str	r0, [sp, #88]           @ 4-byte Spill
	add	r0, r1
	str	r1, [sp, #84]           @ 4-byte Spill
	ldr	r1, [sp, #216]
	add.w	r5, r12, r8
	str	r0, [sp, #252]
	ldr	r0, [sp, #296]
	str	r1, [sp, #76]           @ 4-byte Spill
	str	r0, [sp, #80]           @ 4-byte Spill
	add	r0, r1
	ldr	r1, [sp, #220]
	str	r0, [sp, #256]
	ldr	r0, [sp, #300]
	str	r1, [sp, #68]           @ 4-byte Spill
	str	r0, [sp, #72]           @ 4-byte Spill
	add	r0, r1
	ldr	r1, [sp, #304]
	str	r0, [sp, #260]
	ldr	r0, [sp, #224]
	str	r1, [sp, #64]           @ 4-byte Spill
	add	r1, r0
	str	r0, [sp, #60]           @ 4-byte Spill
	ldr	r0, [sp, #228]
	str	r1, [sp, #264]
	ldr	r1, [sp, #308]
	str	r0, [sp, #52]           @ 4-byte Spill
	adds	r3, r1, r0
	str	r1, [sp, #56]           @ 4-byte Spill
	ldr	r1, [sp, #324]
	ldr	r0, [sp, #328]
	str	r3, [sp, #268]
	str	r5, [sp, #272]
	ldr	r5, [sp, #236]
	ldr	r3, [sp, #316]
	add.w	lr, r3, r5
	subs	r3, r5, r3
	str.w	lr, [sp, #276]
	ldr.w	lr, [sp, #240]
	add.w	r9, r2, lr
	sub.w	r2, lr, r2
	str.w	r9, [sp, #280]
	ldr.w	r9, [sp, #244]
	add.w	r11, r1, r9
	sub.w	r1, r9, r1
	str.w	r11, [sp, #284]
	ldr.w	r11, [sp, #248]
	add.w	r10, r0, r11
	sub.w	r0, r11, r0
	str.w	r10, [sp, #288]
	ldrd	r6, r4, [sp, #84]
	subs	r4, r6, r4
	str	r4, [sp, #292]
	ldrd	r6, r4, [sp, #76]
	subs	r4, r6, r4
	str	r4, [sp, #296]
	ldrd	r6, r4, [sp, #68]
	subs	r4, r6, r4
	str	r4, [sp, #300]
	ldrd	r6, r4, [sp, #60]
	subs	r4, r6, r4
	str	r4, [sp, #304]
	ldrd	r6, r4, [sp, #52]
	subs	r4, r6, r4
	sub.w	r6, r8, r12
	str	r4, [sp, #308]
	str	r6, [sp, #312]
	str	r3, [sp, #316]
	str	r2, [sp, #320]
	str	r1, [sp, #324]
	str	r0, [sp, #328]
	add	r0, sp, #332
	add	r1, sp, #132
	add	r2, sp, #172
	bl	fe_mul
	ldr	r0, [sp, #172]
	ldr	r1, [sp, #132]
	sub.w	r9, r1, r0
	ldr	r0, [sp, #176]
	str.w	r9, [sp, #132]
	ldr	r1, [sp, #136]
	subs	r6, r1, r0
	ldr	r0, [sp, #180]
	str	r6, [sp, #136]
	ldr	r1, [sp, #140]
	sub.w	r8, r1, r0
	ldr	r0, [sp, #184]
	str.w	r8, [sp, #140]
	ldr	r1, [sp, #144]
	sub.w	r11, r1, r0
	ldr	r0, [sp, #188]
	str.w	r11, [sp, #144]
	ldr	r1, [sp, #148]
	subs	r4, r1, r0
	ldr	r0, [sp, #192]
	str	r4, [sp, #148]
	ldr	r1, [sp, #152]
	subs	r0, r1, r0
	str	r0, [sp, #152]
	str	r0, [sp, #76]           @ 4-byte Spill
	ldr	r0, [sp, #196]
	ldr	r1, [sp, #156]
	subs	r0, r1, r0
	str	r0, [sp, #156]
	str	r0, [sp, #80]           @ 4-byte Spill
	ldr	r0, [sp, #200]
	ldr	r1, [sp, #160]
	subs	r0, r1, r0
	str	r0, [sp, #160]
	str	r0, [sp, #84]           @ 4-byte Spill
	ldr	r0, [sp, #204]
	ldr	r1, [sp, #164]
	subs	r0, r1, r0
	str	r0, [sp, #164]
	str	r0, [sp, #88]           @ 4-byte Spill
	ldr	r0, [sp, #208]
	ldr	r1, [sp, #168]
	sub.w	r10, r1, r0
	add	r0, sp, #292
	mov	r1, r0
	str.w	r10, [sp, #168]
	bl	fe_sq
	movw	r12, #56130
	asr.w	r2, r10, #31
	asr.w	r5, r9, #31
	movt	r12, #1
	umull	r0, r1, r10, r12
	mla	r2, r2, r12, r1
	adds.w	lr, r0, #16777216
	mov.w	r1, #19
	mov	r10, r0
	adc	r2, r2, #0
	lsr.w	r3, lr, #25
	orr.w	r3, r3, r2, lsl #7
	asrs	r2, r2, #25
	umull	r3, r0, r3, r1
	smlabb	r0, r2, r1, r0
	umull	r2, r1, r9, r12
	mla	r1, r5, r12, r1
	adds	r2, r2, r3
	adcs	r0, r1
	adds.w	r1, r2, #33554432
	and	r3, r1, #-67108864
	adc	r0, r0, #0
	lsrs	r1, r1, #26
	subs	r2, r2, r3
	orr.w	r0, r1, r0, lsl #6
	str	r2, [sp, #212]
	umull	r1, r2, r6, r12
	adds.w	r3, r1, #16777216
	and	r5, r3, #-33554432
	lsr.w	r3, r3, #25
	sub.w	r1, r1, r5
	asr.w	r5, r8, #31
	add	r0, r1
	str	r0, [sp, #216]
	umull	r0, r1, r8, r12
	mla	r1, r5, r12, r1
	asr.w	r5, r6, #31
	mla	r2, r5, r12, r2
	ldr	r5, [sp, #76]           @ 4-byte Reload
	adc	r2, r2, #0
	orr.w	r3, r3, r2, lsl #7
	adds	r0, r0, r3
	adc.w	r1, r1, r2, asr #25
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	str	r0, [sp, #220]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	umull	r1, r2, r11, r12
	add	r0, r1
	adds.w	r1, r1, #16777216
	and	r3, r1, #-33554432
	lsr.w	r1, r1, #25
	sub.w	r0, r0, r3
	str	r0, [sp, #224]
	umull	r0, r3, r4, r12
	asr.w	r4, r4, #31
	mla	r3, r4, r12, r3
	asr.w	r4, r11, #31
	mla	r2, r4, r12, r2
	ldr	r4, [sp, #80]           @ 4-byte Reload
	adc	r2, r2, #0
	orr.w	r1, r1, r2, lsl #7
	adds	r0, r0, r1
	adc.w	r1, r3, r2, asr #25
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	str	r0, [sp, #228]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	umull	r1, r2, r5, r12
	add	r0, r1
	adds.w	r1, r1, #16777216
	and	r3, r1, #-33554432
	lsr.w	r1, r1, #25
	sub.w	r0, r0, r3
	str	r0, [sp, #232]
	umull	r0, r3, r4, r12
	asr.w	r4, r4, #31
	mla	r3, r4, r12, r3
	asr.w	r4, r5, #31
	ldr	r5, [sp, #84]           @ 4-byte Reload
	mla	r2, r4, r12, r2
	ldr	r4, [sp, #88]           @ 4-byte Reload
	adc	r2, r2, #0
	orr.w	r1, r1, r2, lsl #7
	adds	r0, r0, r1
	adc.w	r1, r3, r2, asr #25
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	str	r0, [sp, #236]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	umull	r1, r2, r5, r12
	add	r0, r1
	adds.w	r1, r1, #16777216
	and	r3, r1, #-33554432
	lsr.w	r1, r1, #25
	sub.w	r0, r0, r3
	str	r0, [sp, #240]
	umull	r0, r3, r4, r12
	asr.w	r4, r4, #31
	mla	r3, r4, r12, r3
	asr.w	r4, r5, #31
	mla	r2, r4, r12, r2
	adc	r2, r2, #0
	orr.w	r1, r1, r2, lsl #7
	adds	r0, r0, r1
	adc.w	r1, r3, r2, asr #25
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	str	r0, [sp, #244]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	and	r1, lr, #-33554432
	add	r0, r10
	subs	r0, r0, r1
	str	r0, [sp, #248]
	add	r0, sp, #252
	mov	r1, r0
	bl	fe_sq
	ldr	r0, [sp, #172]
	ldr	r1, [sp, #212]
	add	r4, sp, #292
	mov	r2, r4
	add	r0, r1
	ldr	r1, [sp, #216]
	str	r0, [sp, #172]
	ldr	r0, [sp, #176]
	add	r0, r1
	ldr	r1, [sp, #220]
	str	r0, [sp, #176]
	ldr	r0, [sp, #180]
	add	r0, r1
	ldr	r1, [sp, #224]
	str	r0, [sp, #180]
	ldr	r0, [sp, #184]
	add	r0, r1
	ldr	r1, [sp, #228]
	str	r0, [sp, #184]
	ldr	r0, [sp, #188]
	add	r0, r1
	ldr	r1, [sp, #232]
	str	r0, [sp, #188]
	ldr	r0, [sp, #192]
	add	r0, r1
	ldr	r1, [sp, #236]
	str	r0, [sp, #192]
	ldr	r0, [sp, #196]
	add	r0, r1
	ldr	r1, [sp, #240]
	str	r0, [sp, #196]
	ldr	r0, [sp, #200]
	add	r0, r1
	ldr	r1, [sp, #244]
	str	r0, [sp, #200]
	ldr	r0, [sp, #204]
	add	r0, r1
	ldr	r1, [sp, #248]
	str	r0, [sp, #204]
	ldr	r0, [sp, #208]
	add	r0, r1
	add	r1, sp, #372
	str	r0, [sp, #208]
	add	r0, sp, #212
	bl	fe_mul
	mov	r0, r4
	add	r1, sp, #132
	add	r2, sp, #172
	bl	fe_mul
	ldr	r0, [sp, #96]           @ 4-byte Reload
	cmp	r0, #1
	bge.w	.LBB1_1
@ BB#3:
	ldr	r0, [sp, #92]           @ 4-byte Reload
	ldr	r1, [sp, #332]
	ldr	r2, [sp, #252]
	add	r6, sp, #532
	add	r5, sp, #292
	eor.w	r3, r2, r1
	rsbs	r0, r0, #0
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #332]
	eor.w	r1, r3, r2
	str	r1, [sp, #252]
	ldr	r1, [sp, #336]
	ldr	r2, [sp, #256]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #336]
	eor.w	r1, r3, r2
	str	r1, [sp, #256]
	ldr	r1, [sp, #340]
	ldr	r2, [sp, #260]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #340]
	eor.w	r1, r3, r2
	str	r1, [sp, #260]
	ldr	r1, [sp, #344]
	ldr	r2, [sp, #264]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #344]
	eor.w	r1, r3, r2
	str	r1, [sp, #264]
	ldr	r1, [sp, #348]
	ldr	r2, [sp, #268]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #348]
	eor.w	r1, r3, r2
	str	r1, [sp, #268]
	ldr	r1, [sp, #352]
	ldr	r2, [sp, #272]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #352]
	eor.w	r1, r3, r2
	str	r1, [sp, #272]
	ldr	r1, [sp, #356]
	ldr	r2, [sp, #276]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #356]
	eor.w	r1, r3, r2
	str	r1, [sp, #276]
	ldr	r1, [sp, #360]
	ldr	r2, [sp, #280]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #360]
	eor.w	r1, r3, r2
	str	r1, [sp, #280]
	ldr	r1, [sp, #364]
	ldr	r2, [sp, #284]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #364]
	eor.w	r1, r3, r2
	str	r1, [sp, #284]
	ldr	r1, [sp, #368]
	ldr	r2, [sp, #288]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #368]
	eor.w	r1, r3, r2
	ldr	r2, [sp, #212]
	str	r1, [sp, #288]
	ldr	r1, [sp, #292]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #292]
	eor.w	r1, r3, r2
	str	r1, [sp, #212]
	ldr	r1, [sp, #296]
	ldr	r2, [sp, #216]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #296]
	eor.w	r1, r3, r2
	str	r1, [sp, #216]
	ldr	r1, [sp, #300]
	ldr	r2, [sp, #220]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #300]
	eor.w	r1, r3, r2
	str	r1, [sp, #220]
	ldr	r1, [sp, #304]
	ldr	r2, [sp, #224]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #304]
	eor.w	r1, r3, r2
	str	r1, [sp, #224]
	ldr	r1, [sp, #308]
	ldr	r2, [sp, #228]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #308]
	eor.w	r1, r3, r2
	str	r1, [sp, #228]
	ldr	r1, [sp, #312]
	ldr	r2, [sp, #232]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #312]
	eor.w	r1, r3, r2
	str	r1, [sp, #232]
	ldr	r1, [sp, #316]
	ldr	r2, [sp, #236]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #316]
	eor.w	r1, r3, r2
	str	r1, [sp, #236]
	ldr	r1, [sp, #320]
	ldr	r2, [sp, #240]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #320]
	eor.w	r1, r3, r2
	str	r1, [sp, #240]
	ldr	r1, [sp, #324]
	ldr	r2, [sp, #244]
	eor.w	r3, r2, r1
	ands	r3, r0
	eors	r1, r3
	str	r1, [sp, #324]
	eor.w	r1, r3, r2
	str	r1, [sp, #244]
	ldr	r1, [sp, #328]
	ldr	r2, [sp, #248]
	eor.w	r3, r2, r1
	ands	r0, r3
	eors	r1, r0
	eors	r0, r2
	str	r1, [sp, #328]
	str	r0, [sp, #248]
	mov	r0, r6
	mov	r1, r5
	bl	fe_sq
	add	r4, sp, #492
	mov	r1, r6
	mov	r0, r4
	bl	fe_sq
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	mov	r0, r4
	mov	r1, r5
	mov	r2, r4
	bl	fe_mul
	mov	r0, r6
	mov	r1, r6
	mov	r2, r4
	bl	fe_mul
	add	r5, sp, #452
	mov	r1, r6
	mov	r0, r5
	bl	fe_sq
	mov	r0, r4
	mov	r1, r4
	mov	r2, r5
	bl	fe_mul
	mov	r0, r5
	mov	r1, r4
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r4
	mov	r1, r5
	mov	r2, r4
	bl	fe_mul
	mov	r0, r5
	mov	r1, r4
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	mov	r2, r4
	bl	fe_mul
	add	r6, sp, #412
	mov	r1, r5
	mov	r0, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r6
	mov	r1, r6
	bl	fe_sq
	mov	r0, r5
	mov	r1, r6
	mov	r2, r5
	bl	fe_mul
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	mov	r0, r4
	mov	r1, r5
	mov	r2, r4
	bl	fe_mul
	mov	r0, r5
	mov	r1, r4
	bl	fe_sq
	movs	r4, #49
.LBB1_4:                                @ =>This Inner Loop Header: Depth=1
	mov	r0, r5
	mov	r1, r5
	bl	fe_sq
	subs	r4, #1
	bne	.LBB1_4
@ BB#5:
	add	r5, sp, #452
	add	r2, sp, #492
	mov	r0, r5
	mov	r1, r5
	bl	fe_mul
	add	r4, sp, #412
	mov	r1, r5
	mov	r0, r4
	bl	fe_sq
	movs	r5, #99
.LBB1_6:                                @ =>This Inner Loop Header: Depth=1
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	subs	r5, #1
	bne	.LBB1_6
@ BB#7:
	add	r4, sp, #452
	add	r1, sp, #412
	mov	r0, r4
	mov	r2, r4
	bl	fe_mul
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	movs	r5, #49
.LBB1_8:                                @ =>This Inner Loop Header: Depth=1
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	subs	r5, #1
	bne	.LBB1_8
@ BB#9:
	add	r4, sp, #492
	add	r1, sp, #452
	mov	r0, r4
	mov	r2, r4
	bl	fe_mul
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	mov	r0, r4
	mov	r1, r4
	bl	fe_sq
	add	r5, sp, #292
	add	r2, sp, #532
	mov	r1, r4
	mov	r0, r5
	bl	fe_mul
	add	r0, sp, #332
	mov	r2, r5
	mov	r1, r0
	bl	fe_mul
	ldr.w	r8, [sp, #368]
	mov.w	lr, #19
	ldrd	r0, r3, [sp, #332]
	mul	r1, r8, lr
	add.w	r2, r1, #16777216
	ldrd	r1, r4, [sp, #340]
	ldr	r5, [sp, #348]
	ldr.w	r11, [sp, #352]
	ldr.w	r10, [sp, #356]
	ldr.w	r9, [sp, #360]
	ldr.w	r12, [sp, #364]
	ldr	r6, [sp, #4]            @ 4-byte Reload
	add.w	r2, r0, r2, asr #25
	add.w	r2, r3, r2, asr #26
	add.w	r2, r1, r2, asr #25
	add.w	r2, r4, r2, asr #26
	add.w	r2, r5, r2, asr #25
	add.w	r2, r11, r2, asr #26
	add.w	r2, r10, r2, asr #25
	add.w	r2, r9, r2, asr #26
	add.w	r2, r12, r2, asr #25
	add.w	r2, r8, r2, asr #26
	asrs	r2, r2, #25
	smlabb	r0, r2, lr, r0
	lsrs	r2, r0, #8
	strb	r0, [r6]
	strb	r2, [r6, #1]
	lsrs	r2, r0, #16
	strb	r2, [r6, #2]
	ubfx	r2, r0, #24, #2
	add.w	r0, r3, r0, asr #26
	bic	r3, r0, #-33554432
	orr.w	r2, r2, r3, lsl #2
	strb	r2, [r6, #3]
	lsrs	r2, r0, #6
	strb	r2, [r6, #4]
	lsrs	r2, r0, #14
	add.w	r0, r1, r0, asr #25
	bic	r1, r0, #-67108864
	strb	r2, [r6, #5]
	lsls	r2, r1, #3
	orr.w	r2, r2, r3, lsr #22
	strb	r2, [r6, #6]
	lsrs	r2, r0, #5
	strb	r2, [r6, #7]
	lsrs	r2, r0, #13
	add.w	r0, r4, r0, asr #26
	strb	r2, [r6, #8]
	bic	r2, r0, #-33554432
	lsls	r3, r2, #5
	orr.w	r1, r3, r1, lsr #21
	strb	r1, [r6, #9]
	lsrs	r1, r0, #3
	strb	r1, [r6, #10]
	lsrs	r1, r0, #11
	add.w	r0, r5, r0, asr #25
	strb	r1, [r6, #11]
	lsls	r1, r0, #6
	orr.w	r1, r1, r2, lsr #19
	strb	r1, [r6, #12]
	lsrs	r1, r0, #2
	strb	r1, [r6, #13]
	lsrs	r1, r0, #10
	strb	r1, [r6, #14]
	lsrs	r1, r0, #18
	add.w	r0, r11, r0, asr #26
	strb	r1, [r6, #15]
	lsrs	r1, r0, #8
	strb	r0, [r6, #16]
	strb	r1, [r6, #17]
	lsrs	r1, r0, #16
	strb	r1, [r6, #18]
	ubfx	r1, r0, #24, #1
	add.w	r0, r10, r0, asr #25
	bic	r2, r0, #-67108864
	orr.w	r1, r1, r2, lsl #1
	strb	r1, [r6, #19]
	lsrs	r1, r0, #7
	strb	r1, [r6, #20]
	lsrs	r1, r0, #15
	add.w	r0, r9, r0, asr #26
	strb	r1, [r6, #21]
	bic	r1, r0, #-33554432
	lsls	r3, r1, #3
	orr.w	r2, r3, r2, lsr #23
	strb	r2, [r6, #22]
	lsrs	r2, r0, #5
	strb	r2, [r6, #23]
	lsrs	r2, r0, #13
	add.w	r0, r12, r0, asr #25
	strb	r2, [r6, #24]
	bic	r2, r0, #-67108864
	lsls	r3, r2, #4
	orr.w	r1, r3, r1, lsr #21
	strb	r1, [r6, #25]
	lsrs	r1, r0, #4
	strb	r1, [r6, #26]
	lsrs	r1, r0, #12
	add.w	r0, r8, r0, asr #26
	strb	r1, [r6, #27]
	bic	r1, r0, #-33554432
	lsls	r3, r1, #6
	orr.w	r2, r3, r2, lsr #20
	strb	r2, [r6, #28]
	lsrs	r2, r0, #2
	lsrs	r0, r0, #10
	strb	r2, [r6, #29]
	strb	r0, [r6, #30]
	lsrs	r0, r1, #18
	strb	r0, [r6, #31]
	add.w	sp, sp, #572
	pop.w	{r8, r9, r10, r11}
	pop	{r4, r5, r6, r7, pc}
.Lfunc_end1:
	.size	x25519_scalar_mult, .Lfunc_end1-x25519_scalar_mult
	.cantunwind
	.fnend

	.globl	X25519_public_from_private
	.p2align	1
	.type	X25519_public_from_private,%function
	.code	16                      @ @X25519_public_from_private
	.thumb_func
X25519_public_from_private:
	.fnstart
@ BB#0:
	movw	r2, :lower16:X25519_public_from_private.kMongomeryBasePoint
	movt	r2, :upper16:X25519_public_from_private.kMongomeryBasePoint
	b	x25519_scalar_mult
.Lfunc_end2:
	.size	X25519_public_from_private, .Lfunc_end2-X25519_public_from_private
	.cantunwind
	.fnend

	.p2align	1
	.type	fe_mul,%function
	.code	16                      @ @fe_mul
	.thumb_func
fe_mul:
	.fnstart
@ BB#0:
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	.setfp	r7, sp, #12
	add	r7, sp, #12
	.save	{r8, r9, r10, r11}
	push.w	{r8, r9, r10, r11}
	.pad	#180
	sub	sp, #180
	str	r0, [sp, #68]           @ 4-byte Spill
	ldr	r0, [r1, #16]
	str	r0, [sp, #168]          @ 4-byte Spill
	ldr	r3, [r2]
	str	r3, [sp, #132]          @ 4-byte Spill
	smull	r0, r3, r3, r0
	ldr.w	r8, [r2, #4]
	str.w	r8, [sp, #136]          @ 4-byte Spill
	ldr.w	lr, [r2, #8]
	str.w	lr, [sp, #144]          @ 4-byte Spill
	ldr.w	r12, [r2, #12]
	str.w	r12, [sp, #108]         @ 4-byte Spill
	ldr.w	r10, [r1]
	str.w	r10, [sp, #176]         @ 4-byte Spill
	ldr	r6, [r1, #4]
	str	r6, [sp, #148]          @ 4-byte Spill
	ldr	r4, [r1, #8]
	str	r4, [sp, #164]          @ 4-byte Spill
	ldr	r5, [r1, #12]
	str	r5, [sp, #152]          @ 4-byte Spill
	lsls	r5, r5, #1
	str	r5, [sp, #128]          @ 4-byte Spill
	smlal	r0, r3, r8, r5
	smull	r5, r4, lr, r4
	mov.w	lr, #19
	adds	r0, r0, r5
	adc.w	r5, r3, r4
	lsls	r3, r6, #1
	str	r3, [sp, #112]          @ 4-byte Spill
	smlal	r0, r5, r12, r3
	ldr	r3, [r2, #16]
	str	r3, [sp, #96]           @ 4-byte Spill
	smull	r4, r12, r3, r10
	ldr	r3, [r2, #20]
	adds	r4, r4, r0
	adc.w	r12, r12, r5
	mul	r0, r3, lr
	mov	r10, r3
	str.w	r10, [sp, #72]          @ 4-byte Spill
	str	r0, [sp, #64]           @ 4-byte Spill
	ldr	r6, [r1, #32]
	str	r6, [sp, #172]          @ 4-byte Spill
	ldr.w	r8, [r1, #36]
	lsl.w	r5, r8, #1
	str.w	r8, [sp, #80]           @ 4-byte Spill
	str	r5, [sp, #92]           @ 4-byte Spill
	smlal	r4, r12, r0, r5
	ldr	r0, [r2, #24]
	str	r0, [sp, #76]           @ 4-byte Spill
	mul	r0, r0, lr
	str	r0, [sp, #120]          @ 4-byte Spill
	smull	r6, r0, r0, r6
	ldr	r3, [r2, #28]
	adds	r4, r4, r6
	adc.w	r0, r0, r12
	str	r3, [sp, #88]           @ 4-byte Spill
	mul	r3, r3, lr
	str	r3, [sp, #84]           @ 4-byte Spill
	ldr	r6, [r1, #28]
	lsls	r5, r6, #1
	str	r6, [sp, #160]          @ 4-byte Spill
	str	r5, [sp, #104]          @ 4-byte Spill
	smlal	r4, r0, r3, r5
	ldr	r3, [r2, #32]
	str	r3, [sp, #8]            @ 4-byte Spill
	ldr	r5, [r2, #36]
	mul	r2, r3, lr
	str	r2, [sp, #100]          @ 4-byte Spill
	str	r5, [sp, #12]           @ 4-byte Spill
	ldr.w	r9, [r1, #24]
	ldr	r1, [r1, #20]
	smull	r2, r6, r2, r9
	str	r1, [sp, #116]          @ 4-byte Spill
	adds	r2, r2, r4
	adc.w	r3, r0, r6
	mul	r0, r5, lr
	lsls	r6, r1, #1
	smlal	r2, r3, r0, r6
	str	r0, [sp, #156]          @ 4-byte Spill
	str	r6, [sp, #124]          @ 4-byte Spill
	strd	r3, r2, [sp, #56]
	adds.w	r0, r2, #33554432
	ldr.w	r12, [sp, #132]         @ 4-byte Reload
	adc	r11, r3, #0
	str	r0, [sp, #52]           @ 4-byte Spill
	lsrs	r0, r0, #26
	ldr	r3, [sp, #168]          @ 4-byte Reload
	ldr	r6, [sp, #144]          @ 4-byte Reload
	ldr.w	lr, [sp, #108]          @ 4-byte Reload
	ldr	r5, [sp, #96]           @ 4-byte Reload
	orr.w	r0, r0, r11, lsl #6
	str.w	r11, [sp, #40]          @ 4-byte Spill
	str	r0, [sp, #48]           @ 4-byte Spill
	smull	r0, r2, r12, r1
	ldr	r1, [sp, #136]          @ 4-byte Reload
	smlal	r0, r2, r1, r3
	ldr	r1, [sp, #152]          @ 4-byte Reload
	smull	r4, r1, r6, r1
	adds	r3, r0, r4
	ldr	r0, [sp, #164]          @ 4-byte Reload
	adcs	r1, r2
	smlal	r3, r1, lr, r0
	ldr	r0, [sp, #148]          @ 4-byte Reload
	smull	r2, r4, r5, r0
	adds	r0, r3, r2
	ldr	r2, [sp, #176]          @ 4-byte Reload
	mov	r3, r9
	ldr.w	r9, [sp, #156]          @ 4-byte Reload
	adcs	r1, r4
	str	r3, [sp, #140]          @ 4-byte Spill
	smlal	r0, r1, r10, r2
	ldr	r2, [sp, #120]          @ 4-byte Reload
	ldr.w	r10, [sp, #84]          @ 4-byte Reload
	smull	r2, r4, r2, r8
	ldr.w	r8, [sp, #100]          @ 4-byte Reload
	adds	r0, r0, r2
	ldr	r2, [sp, #172]          @ 4-byte Reload
	adcs	r1, r4
	smlal	r0, r1, r10, r2
	ldr	r2, [sp, #160]          @ 4-byte Reload
	smull	r2, r4, r8, r2
	adds	r0, r0, r2
	ldr	r2, [sp, #48]           @ 4-byte Reload
	adcs	r1, r4
	smlal	r0, r1, r9, r3
	adds	r2, r2, r0
	adc.w	r0, r1, r11, asr #26
	adds.w	r1, r2, #16777216
	str	r2, [sp, #48]           @ 4-byte Spill
	ldr.w	r11, [sp, #136]         @ 4-byte Reload
	adc	r2, r0, #0
	lsrs	r0, r1, #25
	str	r1, [sp, #44]           @ 4-byte Spill
	ldr	r1, [sp, #124]          @ 4-byte Reload
	orr.w	r0, r0, r2, lsl #7
	str	r0, [sp, #36]           @ 4-byte Spill
	smull	r0, r3, r12, r3
	ldr.w	r12, [sp, #76]          @ 4-byte Reload
	smlal	r0, r3, r11, r1
	ldr	r1, [sp, #168]          @ 4-byte Reload
	smull	r4, r1, r6, r1
	ldr	r6, [sp, #72]           @ 4-byte Reload
	adds	r0, r0, r4
	adcs	r1, r3
	ldr	r3, [sp, #128]          @ 4-byte Reload
	smlal	r0, r1, lr, r3
	ldr	r3, [sp, #164]          @ 4-byte Reload
	smull	r3, r4, r5, r3
	ldr	r5, [sp, #144]          @ 4-byte Reload
	adds	r0, r0, r3
	ldr	r3, [sp, #112]          @ 4-byte Reload
	adcs	r1, r4
	smlal	r0, r1, r6, r3
	ldr	r3, [sp, #176]          @ 4-byte Reload
	ldr	r6, [sp, #132]          @ 4-byte Reload
	smull	r3, r4, r12, r3
	adds	r0, r0, r3
	ldr	r3, [sp, #92]           @ 4-byte Reload
	adcs	r1, r4
	smlal	r0, r1, r10, r3
	ldr	r3, [sp, #172]          @ 4-byte Reload
	ldr.w	r10, [sp, #164]         @ 4-byte Reload
	smull	r3, r4, r8, r3
	ldr.w	r8, [sp, #140]          @ 4-byte Reload
	adds	r0, r0, r3
	ldr	r3, [sp, #104]          @ 4-byte Reload
	adcs	r1, r4
	smlal	r0, r1, r9, r3
	ldr	r3, [sp, #36]           @ 4-byte Reload
	ldr.w	r9, [sp, #168]          @ 4-byte Reload
	adds	r3, r3, r0
	adc.w	r0, r1, r2, asr #25
	adds.w	r2, r3, #33554432
	str	r3, [sp, #36]           @ 4-byte Spill
	adc	r1, r0, #0
	lsrs	r0, r2, #26
	str	r2, [sp, #32]           @ 4-byte Spill
	orr.w	r0, r0, r1, lsl #6
	str	r1, [sp, #24]           @ 4-byte Spill
	ldr	r1, [sp, #116]          @ 4-byte Reload
	str	r0, [sp, #28]           @ 4-byte Spill
	ldr	r0, [sp, #160]          @ 4-byte Reload
	smull	r4, r2, r5, r1
	ldr	r1, [sp, #96]           @ 4-byte Reload
	smull	r0, r3, r6, r0
	smlal	r0, r3, r11, r8
	ldr.w	r11, [sp, #172]         @ 4-byte Reload
	adds	r0, r0, r4
	adcs	r2, r3
	ldr	r3, [sp, #152]          @ 4-byte Reload
	smlal	r0, r2, lr, r9
	smull	r3, lr, r1, r3
	adds	r0, r0, r3
	ldr	r3, [sp, #148]          @ 4-byte Reload
	adc.w	r2, r2, lr
	ldr.w	lr, [sp, #72]           @ 4-byte Reload
	smull	r3, r4, r12, r3
	ldr.w	r12, [sp, #80]          @ 4-byte Reload
	smlal	r0, r2, lr, r10
	adds	r0, r0, r3
	ldr	r3, [sp, #88]           @ 4-byte Reload
	adcs	r2, r4
	ldr	r4, [sp, #176]          @ 4-byte Reload
	smlal	r0, r2, r3, r4
	ldr	r3, [sp, #100]          @ 4-byte Reload
	smull	r3, r4, r3, r12
	adds	r0, r0, r3
	ldr	r3, [sp, #156]          @ 4-byte Reload
	adcs	r2, r4
	ldr	r4, [sp, #136]          @ 4-byte Reload
	smlal	r0, r2, r3, r11
	ldr	r3, [sp, #28]           @ 4-byte Reload
	adds	r3, r3, r0
	ldr	r0, [sp, #24]           @ 4-byte Reload
	str	r3, [sp, #28]           @ 4-byte Spill
	adc.w	r0, r2, r0, asr #26
	adds.w	r2, r3, #16777216
	adc	r3, r0, #0
	lsrs	r0, r2, #25
	str	r2, [sp, #24]           @ 4-byte Spill
	ldr	r2, [sp, #104]          @ 4-byte Reload
	orr.w	r0, r0, r3, lsl #7
	str	r3, [sp, #16]           @ 4-byte Spill
	str	r0, [sp, #20]           @ 4-byte Spill
	smull	r0, r3, r6, r11
	mov	r6, r11
	ldr.w	r11, [sp, #92]          @ 4-byte Reload
	smlal	r0, r3, r4, r2
	smull	r4, r2, r5, r8
	ldr	r5, [sp, #108]          @ 4-byte Reload
	ldr.w	r8, [sp, #8]            @ 4-byte Reload
	adds	r0, r0, r4
	adcs	r2, r3
	ldr	r3, [sp, #124]          @ 4-byte Reload
	smlal	r0, r2, r5, r3
	smull	r3, r4, r1, r9
	ldr	r1, [sp, #76]           @ 4-byte Reload
	adds	r0, r0, r3
	ldr	r3, [sp, #128]          @ 4-byte Reload
	adcs	r2, r4
	smlal	r0, r2, lr, r3
	smull	r3, r4, r1, r10
	ldr	r1, [sp, #112]          @ 4-byte Reload
	ldr.w	r10, [sp, #132]         @ 4-byte Reload
	ldr.w	lr, [sp, #144]          @ 4-byte Reload
	adds	r0, r0, r3
	ldr	r3, [sp, #88]           @ 4-byte Reload
	adcs	r2, r4
	smlal	r0, r2, r3, r1
	ldr	r1, [sp, #176]          @ 4-byte Reload
	smull	r3, r4, r8, r1
	ldr	r1, [sp, #156]          @ 4-byte Reload
	adds	r0, r0, r3
	adcs	r2, r4
	smlal	r0, r2, r1, r11
	ldr	r1, [sp, #20]           @ 4-byte Reload
	adds	r1, r1, r0
	ldr	r0, [sp, #16]           @ 4-byte Reload
	str	r1, [sp, #20]           @ 4-byte Spill
	adc.w	r0, r2, r0, asr #25
	adds.w	r1, r1, #33554432
	adc	r2, r0, #0
	lsrs	r0, r1, #26
	str	r1, [sp, #16]           @ 4-byte Spill
	ldr	r1, [sp, #160]          @ 4-byte Reload
	orr.w	r0, r0, r2, lsl #6
	str	r2, [sp]                @ 4-byte Spill
	str	r0, [sp, #4]            @ 4-byte Spill
	smull	r0, r3, r10, r12
	ldr.w	r12, [sp, #136]         @ 4-byte Reload
	smull	r4, r2, lr, r1
	ldr	r1, [sp, #140]          @ 4-byte Reload
	smlal	r0, r3, r12, r6
	adds	r0, r0, r4
	adcs	r2, r3
	ldr	r3, [sp, #96]           @ 4-byte Reload
	smlal	r0, r2, r5, r1
	ldr	r1, [sp, #116]          @ 4-byte Reload
	ldr	r5, [sp, #164]          @ 4-byte Reload
	smull	r3, r4, r3, r1
	ldr	r1, [sp, #72]           @ 4-byte Reload
	adds	r0, r0, r3
	ldr	r3, [sp, #76]           @ 4-byte Reload
	adcs	r2, r4
	smlal	r0, r2, r1, r9
	ldr	r1, [sp, #152]          @ 4-byte Reload
	smull	r3, r4, r3, r1
	ldr	r1, [sp, #88]           @ 4-byte Reload
	adds	r0, r0, r3
	adcs	r2, r4
	smlal	r0, r2, r1, r5
	ldr	r1, [sp, #148]          @ 4-byte Reload
	smull	r3, r4, r8, r1
	ldr	r1, [sp, #12]           @ 4-byte Reload
	ldr.w	r8, [sp, #64]           @ 4-byte Reload
	adds	r0, r0, r3
	ldr	r3, [sp, #176]          @ 4-byte Reload
	adcs	r2, r4
	smlal	r0, r2, r1, r3
	ldr	r1, [sp, #4]            @ 4-byte Reload
	adds	r1, r1, r0
	ldr	r0, [sp]                @ 4-byte Reload
	str	r1, [sp, #76]           @ 4-byte Spill
	adc.w	r0, r2, r0, asr #26
	adds.w	r1, r1, #16777216
	mov.w	r2, #19
	str	r1, [sp, #72]           @ 4-byte Spill
	adc	r0, r0, #0
	lsrs	r1, r1, #25
	orr.w	r1, r1, r0, lsl #7
	asrs	r0, r0, #25
	umull	r4, r1, r1, r2
	smlabb	r0, r0, r2, r1
	mul	r1, r12, r2
	str	r4, [sp, #12]           @ 4-byte Spill
	str	r0, [sp, #8]            @ 4-byte Spill
	smull	r0, r3, r10, r3
	smlal	r0, r3, r1, r11
	mul	r1, lr, r2
	ldr.w	r11, [sp, #140]         @ 4-byte Reload
	ldr.w	lr, [sp, #84]           @ 4-byte Reload
	smull	r6, r4, r1, r6
	adds	r0, r0, r6
	ldr	r6, [sp, #104]          @ 4-byte Reload
	adcs	r4, r3
	ldr	r3, [sp, #108]          @ 4-byte Reload
	muls	r3, r2, r3
	movs	r2, #19
	str	r3, [sp, #88]           @ 4-byte Spill
	smlal	r0, r4, r3, r6
	ldr	r3, [sp, #96]           @ 4-byte Reload
	muls	r2, r3, r2
	ldr	r3, [sp, #124]          @ 4-byte Reload
	str	r2, [sp, #96]           @ 4-byte Spill
	smull	r6, r2, r2, r11
	adds	r0, r0, r6
	adcs	r2, r4
	smlal	r0, r2, r8, r3
	ldr	r3, [sp, #120]          @ 4-byte Reload
	smull	r4, r6, r3, r9
	ldr	r3, [sp, #128]          @ 4-byte Reload
	adds	r0, r0, r4
	adcs	r2, r6
	smlal	r0, r2, lr, r3
	ldr	r3, [sp, #100]          @ 4-byte Reload
	smull	r4, r6, r3, r5
	ldr	r3, [sp, #112]          @ 4-byte Reload
	adds	r4, r4, r0
	ldr	r0, [sp, #156]          @ 4-byte Reload
	adcs	r2, r6
	smlal	r4, r2, r0, r3
	ldr	r3, [sp, #12]           @ 4-byte Reload
	adds.w	r6, r4, #33554432
	and	r5, r6, #-67108864
	adc	r0, r2, #0
	subs	r4, r4, r5
	sbcs	r2, r0
	adds	r5, r3, r4
	ldr	r3, [sp, #8]            @ 4-byte Reload
	adcs	r2, r3
	adds.w	r4, r5, #33554432
	and	r3, r4, #-67108864
	adc	r2, r2, #0
	subs	r3, r5, r3
	ldr	r5, [sp, #68]           @ 4-byte Reload
	str	r3, [r5]
	lsrs	r3, r4, #26
	orr.w	r2, r3, r2, lsl #6
	lsrs	r3, r6, #26
	ldr	r5, [sp, #80]           @ 4-byte Reload
	orr.w	r4, r3, r0, lsl #6
	ldr	r3, [sp, #148]          @ 4-byte Reload
	smull	r1, r9, r1, r5
	ldr	r5, [sp, #88]           @ 4-byte Reload
	smull	r3, r6, r10, r3
	ldr.w	r10, [sp, #176]         @ 4-byte Reload
	smlal	r3, r6, r12, r10
	adds	r1, r1, r3
	adc.w	r3, r6, r9
	ldr	r6, [sp, #172]          @ 4-byte Reload
	smlal	r1, r3, r5, r6
	ldr	r5, [sp, #160]          @ 4-byte Reload
	ldr	r6, [sp, #96]           @ 4-byte Reload
	smull	r6, r5, r6, r5
	adds	r1, r1, r6
	adcs	r3, r5
	ldrd	r5, r9, [sp, #116]
	smlal	r1, r3, r8, r11
	ldr.w	r8, [sp, #156]          @ 4-byte Reload
	smull	r6, r5, r9, r5
	adds	r1, r1, r6
	adcs	r3, r5
	ldr	r5, [sp, #168]          @ 4-byte Reload
	smlal	r1, r3, lr, r5
	ldr.w	lr, [sp, #100]          @ 4-byte Reload
	ldr	r5, [sp, #152]          @ 4-byte Reload
	smull	r6, r5, lr, r5
	adds	r1, r1, r6
	adcs	r3, r5
	ldr	r5, [sp, #164]          @ 4-byte Reload
	smlal	r1, r3, r8, r5
	adds	r1, r1, r4
	ldr	r4, [sp, #68]           @ 4-byte Reload
	adc.w	r0, r3, r0, asr #26
	adds.w	r3, r1, #16777216
	and	r6, r3, #-33554432
	adc	r11, r0, #0
	subs	r1, r1, r6
	add	r1, r2
	str	r1, [r4, #4]
	lsrs	r1, r3, #25
	ldr	r2, [sp, #132]          @ 4-byte Reload
	ldr	r0, [sp, #144]          @ 4-byte Reload
	orr.w	r1, r1, r11, lsl #7
	smull	r2, r3, r2, r5
	ldr	r5, [sp, #112]          @ 4-byte Reload
	smlal	r2, r3, r12, r5
	smull	r6, r5, r0, r10
	adds	r2, r2, r6
	adcs	r3, r5
	ldrd	r5, r6, [sp, #88]
	ldr.w	r12, [sp, #96]          @ 4-byte Reload
	ldr	r0, [sp, #172]          @ 4-byte Reload
	ldr.w	r10, [sp, #64]          @ 4-byte Reload
	smlal	r2, r3, r5, r6
	smull	r6, r5, r12, r0
	ldr	r0, [sp, #140]          @ 4-byte Reload
	adds	r2, r2, r6
	adcs	r3, r5
	ldr	r5, [sp, #104]          @ 4-byte Reload
	smlal	r2, r3, r10, r5
	smull	r6, r5, r9, r0
	ldr.w	r9, [sp, #84]           @ 4-byte Reload
	ldr	r0, [sp, #168]          @ 4-byte Reload
	adds	r2, r2, r6
	adcs	r3, r5
	ldr	r5, [sp, #124]          @ 4-byte Reload
	smlal	r2, r3, r9, r5
	smull	r6, r5, lr, r0
	adds	r2, r2, r6
	adcs	r3, r5
	ldr	r5, [sp, #128]          @ 4-byte Reload
	smlal	r2, r3, r8, r5
	adds	r1, r1, r2
	adc.w	r0, r3, r11, asr #25
	adds.w	r2, r1, #33554432
	and	r3, r2, #-67108864
	adc	r0, r0, #0
	subs	r1, r1, r3
	str	r1, [r4, #8]
	lsrs	r1, r2, #26
	ldr	r2, [sp, #152]          @ 4-byte Reload
	ldr	r3, [sp, #132]          @ 4-byte Reload
	ldr	r5, [sp, #164]          @ 4-byte Reload
	ldr	r6, [sp, #136]          @ 4-byte Reload
	orr.w	r1, r1, r0, lsl #6
	smull	r2, r3, r3, r2
	smlal	r2, r3, r6, r5
	ldrd	r6, r5, [sp, #144]
	smull	r6, r5, r6, r5
	adds	r2, r2, r6
	ldr	r6, [sp, #176]          @ 4-byte Reload
	adcs	r3, r5
	ldr	r5, [sp, #108]          @ 4-byte Reload
	smlal	r2, r3, r5, r6
	ldr	r5, [sp, #80]           @ 4-byte Reload
	smull	r6, r5, r12, r5
	adds	r2, r2, r6
	ldr	r6, [sp, #172]          @ 4-byte Reload
	adcs	r3, r5
	ldr	r5, [sp, #160]          @ 4-byte Reload
	smlal	r2, r3, r10, r6
	ldr	r6, [sp, #120]          @ 4-byte Reload
	smull	r6, r5, r6, r5
	adds	r2, r2, r6
	ldr	r6, [sp, #140]          @ 4-byte Reload
	adcs	r3, r5
	smlal	r2, r3, r9, r6
	ldr	r6, [sp, #116]          @ 4-byte Reload
	smull	r6, r5, lr, r6
	adds	r2, r2, r6
	ldr	r6, [sp, #168]          @ 4-byte Reload
	adcs	r3, r5
	smlal	r2, r3, r8, r6
	mov	r6, r4
	adds	r1, r1, r2
	adc.w	r0, r3, r0, asr #26
	adds.w	r2, r1, #16777216
	and	r3, r2, #-33554432
	adc	r0, r0, #0
	subs	r1, r1, r3
	str	r1, [r6, #12]
	lsrs	r1, r2, #25
	asrs	r2, r0, #25
	orr.w	r0, r1, r0, lsl #7
	ldr	r1, [sp, #52]           @ 4-byte Reload
	ldr	r3, [sp, #60]           @ 4-byte Reload
	ldr	r5, [sp, #40]           @ 4-byte Reload
	and	r1, r1, #-67108864
	subs	r1, r3, r1
	ldr	r3, [sp, #56]           @ 4-byte Reload
	sbcs	r3, r5
	adds	r0, r0, r1
	adc.w	r1, r2, r3
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	mov	r3, r6
	str	r0, [r3, #16]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	ldr	r1, [sp, #44]           @ 4-byte Reload
	ldr	r2, [sp, #48]           @ 4-byte Reload
	and	r1, r1, #-33554432
	subs	r1, r2, r1
	add	r0, r1
	str	r0, [r3, #20]
	ldr	r0, [sp, #32]           @ 4-byte Reload
	ldr	r1, [sp, #36]           @ 4-byte Reload
	and	r0, r0, #-67108864
	subs	r0, r1, r0
	str	r0, [r3, #24]
	ldr	r0, [sp, #24]           @ 4-byte Reload
	ldr	r1, [sp, #28]           @ 4-byte Reload
	and	r0, r0, #-33554432
	subs	r0, r1, r0
	str	r0, [r3, #28]
	ldr	r0, [sp, #16]           @ 4-byte Reload
	ldr	r1, [sp, #20]           @ 4-byte Reload
	ldr	r2, [sp, #76]           @ 4-byte Reload
	and	r0, r0, #-67108864
	subs	r0, r1, r0
	ldr	r1, [sp, #72]           @ 4-byte Reload
	and	r1, r1, #-33554432
	subs	r1, r2, r1
	strd	r0, r1, [r3, #32]
	add	sp, #180
	pop.w	{r8, r9, r10, r11}
	pop	{r4, r5, r6, r7, pc}
.Lfunc_end3:
	.size	fe_mul, .Lfunc_end3-fe_mul
	.cantunwind
	.fnend

	.p2align	1
	.type	fe_sq,%function
	.code	16                      @ @fe_sq
	.thumb_func
fe_sq:
	.fnstart
@ BB#0:
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	.setfp	r7, sp, #12
	add	r7, sp, #12
	.save	{r8, r9, r10, r11}
	push.w	{r8, r9, r10, r11}
	.pad	#128
	sub	sp, #128
	str	r0, [sp, #92]           @ 4-byte Spill
	ldr	r4, [r1]
	str	r4, [sp, #16]           @ 4-byte Spill
	ldr	r5, [r1, #4]
	str	r5, [sp, #36]           @ 4-byte Spill
	lsls	r6, r5, #1
	ldr	r0, [r1, #8]
	str	r0, [sp, #104]          @ 4-byte Spill
	smull	r0, r2, r0, r0
	ldr	r3, [r1, #12]
	str	r6, [sp]                @ 4-byte Spill
	lsls	r5, r3, #1
	mov	r11, r3
	lsls	r3, r4, #1
	str	r5, [sp, #112]          @ 4-byte Spill
	smlal	r0, r2, r5, r6
	ldr	r6, [r1, #16]
	str	r3, [sp, #108]          @ 4-byte Spill
	mov	r9, r6
	smull	r3, r12, r6, r3
	movs	r6, #38
	str.w	r9, [sp, #124]          @ 4-byte Spill
	adds	r0, r0, r3
	ldr.w	r8, [r1, #28]
	adc.w	lr, r2, r12
	movs	r2, #38
	mul	r2, r8, r2
	str.w	r8, [sp, #72]           @ 4-byte Spill
	str	r2, [sp, #88]           @ 4-byte Spill
	smlal	r0, lr, r2, r8
	movs	r2, #19
	ldr	r3, [r1, #32]
	str	r3, [sp, #80]           @ 4-byte Spill
	muls	r2, r3, r2
	ldr	r5, [r1, #36]
	str	r2, [sp, #120]          @ 4-byte Spill
	str	r5, [sp, #12]           @ 4-byte Spill
	ldr	r3, [r1, #24]
	str	r3, [sp, #100]          @ 4-byte Spill
	lsls	r4, r3, #1
	mul	r3, r5, r6
	ldr	r5, [r1, #20]
	smull	r4, r10, r2, r4
	adds	r0, r0, r4
	adc.w	r2, lr, r10
	mov	r10, r11
	lsls	r1, r5, #1
	str	r5, [sp, #84]           @ 4-byte Spill
	smlal	r0, r2, r3, r1
	str	r1, [sp, #116]          @ 4-byte Spill
	strd	r2, r0, [sp, #64]
	adds.w	r0, r0, #33554432
	str.w	r10, [sp, #24]          @ 4-byte Spill
	str	r0, [sp, #60]           @ 4-byte Spill
	adc	r4, r2, #0
	lsrs	r0, r0, #26
	ldr.w	r12, [sp]               @ 4-byte Reload
	ldr.w	lr, [sp, #120]          @ 4-byte Reload
	orr.w	r0, r0, r4, lsl #6
	str	r4, [sp, #48]           @ 4-byte Spill
	str	r0, [sp, #56]           @ 4-byte Spill
	ldr	r0, [sp, #104]          @ 4-byte Reload
	lsls	r2, r0, #1
	smull	r0, r1, r2, r11
	mov	r6, r2
	lsl.w	r2, r8, #1
	smlal	r0, r1, r9, r12
	ldr.w	r9, [sp, #108]          @ 4-byte Reload
	str	r2, [sp, #76]           @ 4-byte Spill
	smull	r11, r2, lr, r2
	str	r6, [sp, #96]           @ 4-byte Spill
	smlal	r0, r1, r5, r9
	mov	r5, r3
	ldr	r3, [sp, #100]          @ 4-byte Reload
	adds.w	r0, r0, r11
	mov	r11, r9
	str	r5, [sp, #4]            @ 4-byte Spill
	adcs	r1, r2
	ldr	r2, [sp, #56]           @ 4-byte Reload
	smlal	r0, r1, r5, r3
	adds	r2, r2, r0
	adc.w	r0, r1, r4, asr #26
	adds.w	r1, r2, #16777216
	str	r2, [sp, #56]           @ 4-byte Spill
	str	r1, [sp, #52]           @ 4-byte Spill
	adc	r8, r0, #0
	lsrs	r1, r1, #25
	orr.w	r0, r1, r8, lsl #7
	ldr	r1, [sp, #116]          @ 4-byte Reload
	str	r0, [sp, #44]           @ 4-byte Spill
	ldr	r0, [sp, #124]          @ 4-byte Reload
	smull	r2, r4, r0, r6
	ldr	r0, [sp, #112]          @ 4-byte Reload
	smull	r6, r1, r1, r12
	smlal	r2, r4, r0, r10
	ldr.w	r10, [sp, #80]          @ 4-byte Reload
	adds	r2, r2, r6
	adcs	r1, r4
	smlal	r2, r1, r3, r9
	ldr	r3, [sp, #76]           @ 4-byte Reload
	ldr.w	r9, [sp, #100]          @ 4-byte Reload
	smull	r4, r6, lr, r10
	adds	r2, r2, r4
	adcs	r1, r6
	smlal	r2, r1, r5, r3
	ldr	r3, [sp, #44]           @ 4-byte Reload
	adds	r2, r2, r3
	ldr	r3, [sp, #96]           @ 4-byte Reload
	adc.w	r1, r1, r8, asr #25
	str	r2, [sp, #44]           @ 4-byte Spill
	adds.w	r2, r2, #33554432
	ldr.w	r8, [sp, #124]          @ 4-byte Reload
	adc	lr, r1, #0
	lsrs	r1, r2, #26
	str	r2, [sp, #40]           @ 4-byte Spill
	orr.w	r1, r1, lr, lsl #6
	str	r1, [sp, #32]           @ 4-byte Spill
	ldr	r1, [sp, #84]           @ 4-byte Reload
	smull	r2, r4, r0, r8
	ldr	r0, [sp, #72]           @ 4-byte Reload
	smlal	r2, r4, r1, r3
	smull	r6, r1, r0, r11
	smlal	r2, r4, r9, r12
	ldr	r0, [sp, #32]           @ 4-byte Reload
	mov	r11, r8
	adds	r2, r2, r6
	adcs	r1, r4
	smlal	r2, r1, r5, r10
	adds	r2, r2, r0
	adc.w	r6, r1, lr, asr #26
	adds.w	r1, r2, #16777216
	str	r2, [sp, #32]           @ 4-byte Spill
	smull	r2, r4, r8, r8
	str	r1, [sp, #28]           @ 4-byte Spill
	adc	lr, r6, #0
	lsrs	r1, r1, #25
	smlal	r2, r4, r9, r3
	orr.w	r0, r1, lr, lsl #7
	str	r0, [sp, #20]           @ 4-byte Spill
	ldrd	r0, r1, [sp, #112]
	ldr	r3, [sp, #76]           @ 4-byte Reload
	smull	r6, r8, r1, r0
	adds	r2, r2, r6
	adc.w	r1, r4, r8
	ldr.w	r8, [sp, #12]           @ 4-byte Reload
	smlal	r2, r1, r3, r12
	ldr	r3, [sp, #108]          @ 4-byte Reload
	smull	r4, r6, r10, r3
	ldr.w	r10, [sp, #84]          @ 4-byte Reload
	adds	r2, r2, r4
	ldr	r4, [sp, #20]           @ 4-byte Reload
	adcs	r1, r6
	smlal	r2, r1, r5, r8
	adds	r2, r2, r4
	adc.w	r6, r1, lr, asr #25
	adds.w	r1, r2, #33554432
	str	r2, [sp, #76]           @ 4-byte Spill
	adc	lr, r6, #0
	lsl.w	r6, r11, #1
	str	r1, [sp, #20]           @ 4-byte Spill
	lsrs	r1, r1, #26
	smull	r2, r4, r6, r10
	mov	r11, r6
	ldr	r6, [sp, #72]           @ 4-byte Reload
	orr.w	r1, r1, lr, lsl #6
	smlal	r2, r4, r9, r0
	ldr	r0, [sp, #96]           @ 4-byte Reload
	str.w	r11, [sp, #8]           @ 4-byte Spill
	smlal	r2, r4, r6, r0
	ldr	r0, [sp, #80]           @ 4-byte Reload
	smull	r5, r6, r0, r12
	adds	r2, r2, r5
	adcs	r4, r6
	smlal	r2, r4, r8, r3
	ldr.w	r8, [sp, #4]            @ 4-byte Reload
	adds	r1, r1, r2
	mov.w	r2, #19
	adc.w	r0, r4, lr, asr #26
	str	r1, [sp, #80]           @ 4-byte Spill
	adds.w	r1, r1, #16777216
	ldr.w	lr, [sp, #88]           @ 4-byte Reload
	str	r1, [sp, #72]           @ 4-byte Spill
	adc	r0, r0, #0
	lsrs	r1, r1, #25
	orr.w	r1, r1, r0, lsl #7
	asrs	r0, r0, #25
	umull	r3, r1, r1, r2
	smlabb	r4, r0, r2, r1
	ldr	r1, [sp, #16]           @ 4-byte Reload
	str	r3, [sp, #12]           @ 4-byte Spill
	mov	r3, r10
	movs	r0, #38
	movs	r2, #19
	muls	r0, r3, r0
	smull	r5, r6, r1, r1
	ldr	r1, [sp, #120]          @ 4-byte Reload
	smlal	r5, r6, r0, r3
	mul	r0, r9, r2
	ldr.w	r9, [sp, #112]          @ 4-byte Reload
	ldr	r3, [sp, #96]           @ 4-byte Reload
	str	r0, [sp, #84]           @ 4-byte Spill
	smull	r0, r2, r0, r11
	mov	r11, r12
	adds	r0, r0, r5
	adcs	r2, r6
	smull	r3, r6, r1, r3
	smlal	r0, r2, lr, r9
	ldr	r1, [sp, #12]           @ 4-byte Reload
	adds	r3, r3, r0
	adcs	r2, r6
	smlal	r3, r2, r8, r12
	adds.w	r6, r3, #33554432
	and	r5, r6, #-67108864
	adc	r0, r2, #0
	subs	r3, r3, r5
	sbcs	r2, r0
	adds	r3, r3, r1
	ldr	r1, [sp, #92]           @ 4-byte Reload
	adcs	r2, r4
	adds.w	r5, r3, #33554432
	and	r4, r5, #-67108864
	adc	r2, r2, #0
	subs	r3, r3, r4
	str	r3, [r1]
	lsrs	r3, r5, #26
	orr.w	r2, r3, r2, lsl #6
	ldr.w	r10, [sp, #36]          @ 4-byte Reload
	lsrs	r3, r6, #26
	ldr.w	r12, [sp, #116]         @ 4-byte Reload
	str	r2, [sp, #96]           @ 4-byte Spill
	ldr	r2, [sp, #108]          @ 4-byte Reload
	orr.w	r3, r3, r0, lsl #6
	smull	r6, r5, r2, r10
	ldr	r2, [sp, #84]           @ 4-byte Reload
	smlal	r6, r5, r2, r12
	ldr	r2, [sp, #124]          @ 4-byte Reload
	smlal	r6, r5, lr, r2
	ldr	r2, [sp, #120]          @ 4-byte Reload
	smull	r4, lr, r2, r9
	adds	r4, r4, r6
	adc.w	r2, r5, lr
	ldr	r5, [sp, #104]          @ 4-byte Reload
	mov	lr, r8
	smlal	r4, r2, r8, r5
	adds	r3, r3, r4
	adc.w	r0, r2, r0, asr #26
	adds.w	r2, r3, #16777216
	and	r4, r2, #-33554432
	adc	r0, r0, #0
	lsrs	r2, r2, #25
	subs	r3, r3, r4
	ldr	r4, [sp, #96]           @ 4-byte Reload
	orr.w	r2, r2, r0, lsl #7
	add	r3, r4
	mov	r4, lr
	str	r3, [r1, #4]
	smull	r3, r6, r11, r10
	ldr.w	r9, [sp, #108]          @ 4-byte Reload
	ldr.w	r10, [sp, #100]         @ 4-byte Reload
	ldr	r1, [sp, #84]           @ 4-byte Reload
	ldr.w	r8, [sp, #88]           @ 4-byte Reload
	ldr.w	lr, [sp, #92]           @ 4-byte Reload
	smlal	r3, r6, r5, r9
	smull	r1, r5, r1, r10
	adds	r1, r1, r3
	adc.w	r3, r6, r5
	ldr	r5, [sp, #8]            @ 4-byte Reload
	smlal	r1, r3, r8, r12
	ldr.w	r12, [sp, #120]         @ 4-byte Reload
	smull	r6, r5, r12, r5
	adds	r1, r1, r6
	ldr	r6, [sp, #112]          @ 4-byte Reload
	adcs	r3, r5
	smlal	r1, r3, r4, r6
	adds	r1, r1, r2
	adc.w	r0, r3, r0, asr #25
	adds.w	r2, r1, #33554432
	and	r3, r2, #-67108864
	adc	r0, r0, #0
	subs	r1, r1, r3
	str.w	r1, [lr, #8]
	lsrs	r1, r2, #26
	ldr	r2, [sp, #104]          @ 4-byte Reload
	ldr	r5, [sp, #24]           @ 4-byte Reload
	orr.w	r1, r1, r0, lsl #6
	smull	r2, r3, r11, r2
	smlal	r2, r3, r5, r9
	ldr	r5, [sp, #116]          @ 4-byte Reload
	smlal	r2, r3, r8, r10
	smull	r6, r5, r12, r5
	adds	r2, r2, r6
	ldr	r6, [sp, #124]          @ 4-byte Reload
	adcs	r3, r5
	smlal	r2, r3, r4, r6
	adds	r1, r1, r2
	adc.w	r0, r3, r0, asr #26
	adds.w	r2, r1, #16777216
	and	r3, r2, #-33554432
	adc	r0, r0, #0
	subs	r1, r1, r3
	str.w	r1, [lr, #12]
	lsrs	r1, r2, #25
	asrs	r2, r0, #25
	orr.w	r0, r1, r0, lsl #7
	ldr	r1, [sp, #60]           @ 4-byte Reload
	ldr	r3, [sp, #68]           @ 4-byte Reload
	ldr	r6, [sp, #48]           @ 4-byte Reload
	and	r1, r1, #-67108864
	subs	r1, r3, r1
	ldr	r3, [sp, #64]           @ 4-byte Reload
	sbcs	r3, r6
	adds	r0, r0, r1
	adc.w	r1, r2, r3
	adds.w	r2, r0, #33554432
	and	r3, r2, #-67108864
	sub.w	r0, r0, r3
	str.w	r0, [lr, #16]
	adc	r0, r1, #0
	lsrs	r1, r2, #26
	orr.w	r0, r1, r0, lsl #6
	ldr	r1, [sp, #52]           @ 4-byte Reload
	ldr	r2, [sp, #56]           @ 4-byte Reload
	and	r1, r1, #-33554432
	subs	r1, r2, r1
	add	r0, r1
	str.w	r0, [lr, #20]
	ldr	r0, [sp, #40]           @ 4-byte Reload
	ldr	r1, [sp, #44]           @ 4-byte Reload
	and	r0, r0, #-67108864
	subs	r0, r1, r0
	str.w	r0, [lr, #24]
	ldr	r0, [sp, #28]           @ 4-byte Reload
	ldr	r1, [sp, #32]           @ 4-byte Reload
	and	r0, r0, #-33554432
	subs	r0, r1, r0
	str.w	r0, [lr, #28]
	ldr	r0, [sp, #20]           @ 4-byte Reload
	ldr	r1, [sp, #76]           @ 4-byte Reload
	ldr	r2, [sp, #80]           @ 4-byte Reload
	and	r0, r0, #-67108864
	subs	r0, r1, r0
	ldr	r1, [sp, #72]           @ 4-byte Reload
	and	r1, r1, #-33554432
	subs	r1, r2, r1
	strd	r0, r1, [lr, #32]
	add	sp, #128
	pop.w	{r8, r9, r10, r11}
	pop	{r4, r5, r6, r7, pc}
.Lfunc_end4:
	.size	fe_sq, .Lfunc_end4-fe_sq
	.cantunwind
	.fnend

	.type	X25519.kZeros,%object   @ @X25519.kZeros
	.section	.rodata,"a",%progbits
X25519.kZeros:
	.zero	32
	.size	X25519.kZeros, 32

	.type	X25519_public_from_private.kMongomeryBasePoint,%object @ @X25519_public_from_private.kMongomeryBasePoint
X25519_public_from_private.kMongomeryBasePoint:
	.asciz	"\t\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	.size	X25519_public_from_private.kMongomeryBasePoint, 32


	.ident	"Chromium OS 4.0_pre285905-r3 clang version 4.0.0 (/var/cache/chromeos-cache/distfiles/host/egit-src/clang.git 47592b1815f5423fa708fca47741886043e6bb32) (/var/cache/chromeos-cache/distfiles/host/egit-src/llvm.git 77e7778b3e097ad88c71c7dac789e245c8f3e33a) (based on LLVM 4.0.0svn)"
	.section	".note.GNU-stack","",%progbits
	.eabi_attribute	30, 2	@ Tag_ABI_optimization_goals
