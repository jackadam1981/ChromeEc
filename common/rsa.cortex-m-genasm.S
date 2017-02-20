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
	.file	"common/rsa.c"
	.globl	rsa_verify
	.p2align	1
	.type	rsa_verify,%function
	.code	16                      @ @rsa_verify
	.thumb_func
rsa_verify:
	.fnstart
@ BB#0:
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	.setfp	r7, sp, #12
	add	r7, sp, #12
	.save	{r8, r9, r10}
	push.w	{r8, r9, r10}
	.pad	#256
	sub	sp, #256
	mov	r4, sp
	mov	r8, r2
	mov	r10, r0
	mov.w	r2, #256
	mov	r9, r3
	mov	r0, r4
	bl	memcpy
	movs	r0, #0
	mov	r1, r9
.LBB0_1:                                @ =>This Inner Loop Header: Depth=1
	adds	r2, r4, r0
	subs	r0, #4
	ldrb.w	r5, [r2, #253]
	ldrb.w	r3, [r2, #252]
	ldrb.w	r6, [r2, #254]
	ldrb.w	r2, [r2, #255]
	cmn.w	r0, #256
	lsl.w	r5, r5, #16
	orr.w	r3, r5, r3, lsl #24
	orr.w	r3, r3, r6, lsl #8
	orr.w	r2, r2, r3
	str	r2, [r1], #4
	bne	.LBB0_1
@ BB#2:
	add.w	r5, r9, #256
	add.w	r3, r10, #256
	mov	r0, r10
	mov	r2, r9
	mov	r1, r5
	bl	mont_mul
	add.w	r4, r9, #512
	mov	r0, r10
	mov	r2, r5
	mov	r3, r5
	mov	r1, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r5
	bl	mont_mul
	mov	r0, r10
	mov	r1, r5
	mov	r2, r4
	mov	r3, r4
	bl	mont_mul
	mov	r0, r10
	mov	r1, r4
	mov	r2, r5
	mov	r3, r9
	bl	mont_mul
	add.w	r0, r9, #764
	add.w	r1, r10, #252
	movs	r2, #0
.LBB0_3:                                @ =>This Inner Loop Header: Depth=1
	adds.w	r3, r2, #64
	beq	.LBB0_6
@ BB#4:                                 @   in Loop: Header=BB0_3 Depth=1
	ldr.w	r3, [r1, r2, lsl #2]
	ldr.w	r6, [r0, r2, lsl #2]
	cmp	r6, r3
	blo	.LBB0_8
@ BB#5:                                 @   in Loop: Header=BB0_3 Depth=1
	sub.w	r2, r2, #1
	bls	.LBB0_3
.LBB0_6:
	movs	r0, #0
	movs	r1, #0
	movs	r2, #0
	movs	r3, #0
.LBB0_7:                                @ =>This Inner Loop Header: Depth=1
	ldr.w	r6, [r10, r1, lsl #2]
	ldr.w	r5, [r4, r1, lsl #2]
	subs	r6, r5, r6
	sbc	r5, r0, #0
	adds	r2, r2, r6
	str.w	r2, [r4, r1, lsl #2]
	adc.w	r2, r5, r3
	adds	r1, #1
	asrs	r3, r2, #31
	cmp	r1, #64
	bne	.LBB0_7
.LBB0_8:
	mov	r5, sp
	movs	r0, #191
	mov	r1, r5
.LBB0_9:                                @ =>This Inner Loop Header: Depth=1
	ldr.w	r2, [r9, r0, lsl #2]
	lsrs	r3, r2, #24
	strb	r3, [r1]
	lsrs	r3, r2, #16
	strb	r3, [r1, #1]
	lsrs	r3, r2, #8
	strb	r3, [r1, #2]
	strb	r2, [r1, #3]
	subs	r2, r0, #1
	subs	r0, #128
	adds	r1, #4
	cmp	r0, #0
	mov	r0, r2
	bgt	.LBB0_9
@ BB#10:
	ldrb.w	r1, [sp, #1]
	ldrb.w	r0, [sp]
	eor	r1, r1, #1
	orrs	r0, r1
	uxtb	r4, r0
	movs	r0, #2
.LBB0_11:                               @ =>This Inner Loop Header: Depth=1
	ldrb	r1, [r5, r0]
	adds	r0, #1
	cmp	r0, #204
	eor	r1, r1, #255
	orr.w	r4, r4, r1
	bne	.LBB0_11
@ BB#12:
	movw	r1, :lower16:sha256_tail
	add.w	r0, r5, #204
	movs	r2, #20
	movt	r1, :upper16:sha256_tail
	bl	memcmp
	orrs	r0, r4
	mov.w	r4, #0
	bne	.LBB0_14
@ BB#13:
	add.w	r0, r5, #224
	mov	r1, r8
	movs	r2, #32
	bl	memcmp
	cmp	r0, #0
	it	eq
	moveq	r4, #1
.LBB0_14:
	mov	r0, r4
	add	sp, #256
	pop.w	{r8, r9, r10}
	pop	{r4, r5, r6, r7, pc}
.Lfunc_end0:
	.size	rsa_verify, .Lfunc_end0-rsa_verify
	.cantunwind
	.fnend

	.p2align	1
	.type	mont_mul,%function
	.code	16                      @ @mont_mul
	.thumb_func
mont_mul:
	.fnstart
@ BB#0:
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	.setfp	r7, sp, #12
	add	r7, sp, #12
	.save	{r8, r9, r10, r11}
	push.w	{r8, r9, r10, r11}
	.pad	#8
	sub	sp, #8
	str	r2, [sp]                @ 4-byte Spill
	movs	r2, #0
	add.w	lr, r3, #4
	add.w	r9, r0, #4
	mov.w	r8, #0
	strd	r2, r2, [r1]
	strd	r2, r2, [r1, #8]
	strd	r2, r2, [r1, #16]
	strd	r2, r2, [r1, #24]
	strd	r2, r2, [r1, #32]
	strd	r2, r2, [r1, #40]
	strd	r2, r2, [r1, #48]
	strd	r2, r2, [r1, #56]
	strd	r2, r2, [r1, #64]
	strd	r2, r2, [r1, #72]
	strd	r2, r2, [r1, #80]
	strd	r2, r2, [r1, #88]
	strd	r2, r2, [r1, #96]
	strd	r2, r2, [r1, #104]
	strd	r2, r2, [r1, #112]
	strd	r2, r2, [r1, #120]
	strd	r2, r2, [r1, #128]
	strd	r2, r2, [r1, #136]
	strd	r2, r2, [r1, #144]
	strd	r2, r2, [r1, #152]
	strd	r2, r2, [r1, #160]
	strd	r2, r2, [r1, #168]
	strd	r2, r2, [r1, #176]
	strd	r2, r2, [r1, #184]
	strd	r2, r2, [r1, #192]
	strd	r2, r2, [r1, #200]
	strd	r2, r2, [r1, #208]
	strd	r2, r2, [r1, #216]
	strd	r2, r2, [r1, #224]
	strd	r2, r2, [r1, #232]
	strd	r2, r2, [r1, #240]
	strd	r2, r2, [r1, #248]
	str	r3, [sp, #4]            @ 4-byte Spill
.LBB1_1:                                @ =>This Loop Header: Depth=1
                                        @     Child Loop BB1_2 Depth 2
                                        @     Child Loop BB1_5 Depth 2
	ldr	r2, [sp]                @ 4-byte Reload
	ldr	r6, [r1]
	movs	r5, #0
	mov.w	r12, #0
	ldr.w	r10, [r2, r8, lsl #2]
	ldr	r2, [sp, #4]            @ 4-byte Reload
	ldr	r4, [r2]
	ldr.w	r2, [r0, #512]
	umlal	r6, r5, r4, r10
	ldr	r4, [r0]
	mul	r11, r6, r2
	umull	r2, r4, r11, r4
	adds	r2, r2, r6
	adc	r6, r4, #0
	movs	r4, #0
.LBB1_2:                                @   Parent Loop BB1_1 Depth=1
                                        @ =>  This Inner Loop Header: Depth=2
	ldr.w	r2, [lr, r4, lsl #2]
	mov	r3, r12
	umlal	r5, r3, r2, r10
	add.w	r2, r1, r4, lsl #2
	ldr	r2, [r2, #4]
	adds	r2, r2, r5
	adc	r5, r3, #0
	ldr.w	r3, [r9, r4, lsl #2]
	umlal	r6, r12, r3, r11
	adds	r2, r2, r6
	str.w	r2, [r1, r4, lsl #2]
	adc	r6, r12, #0
	adds	r4, #1
	mov.w	r12, #0
	cmp	r4, #63
	bne	.LBB1_2
@ BB#3:                                 @   in Loop: Header=BB1_1 Depth=1
	adds	r2, r6, r5
	mov.w	r12, #0
	str.w	r2, [r1, #252]
	adcs	r2, r12, #0
	beq	.LBB1_6
@ BB#4:                                 @   in Loop: Header=BB1_1 Depth=1
	movs	r6, #0
	movs	r5, #0
	movs	r4, #0
.LBB1_5:                                @   Parent Loop BB1_1 Depth=1
                                        @ =>  This Inner Loop Header: Depth=2
	ldr.w	r2, [r0, r6, lsl #2]
	ldr.w	r3, [r1, r6, lsl #2]
	subs	r2, r3, r2
	sbc	r3, r12, #0
	adds	r2, r2, r5
	adc.w	r5, r3, r4
	str.w	r2, [r1, r6, lsl #2]
	adds	r6, #1
	asrs	r4, r5, #31
	cmp	r6, #64
	bne	.LBB1_5
.LBB1_6:                                @   in Loop: Header=BB1_1 Depth=1
	add.w	r8, r8, #1
	cmp.w	r8, #64
	bne	.LBB1_1
@ BB#7:
	add	sp, #8
	pop.w	{r8, r9, r10, r11}
	pop	{r4, r5, r6, r7, pc}
.Lfunc_end1:
	.size	mont_mul, .Lfunc_end1-mont_mul
	.cantunwind
	.fnend

	.type	sha256_tail,%object     @ @sha256_tail
	.section	.rodata,"a",%progbits
sha256_tail:
	.ascii	"\000010\r\006\t`\206H\001e\003\004\002\001\005\000\004 "
	.size	sha256_tail, 20


	.ident	"Chromium OS 4.0_pre285905-r3 clang version 4.0.0 (/var/cache/chromeos-cache/distfiles/host/egit-src/clang.git 47592b1815f5423fa708fca47741886043e6bb32) (/var/cache/chromeos-cache/distfiles/host/egit-src/llvm.git 77e7778b3e097ad88c71c7dac789e245c8f3e33a) (based on LLVM 4.0.0svn)"
	.section	".note.GNU-stack","",%progbits
	.eabi_attribute	30, 2	@ Tag_ABI_optimization_goals
