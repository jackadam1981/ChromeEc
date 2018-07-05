@ Silence ARMv8 deprecated IT instruction warnings. This file is used by both
@ ARMv7 and ARMv8 processors and does not use ARMv8 instructions. (ARMv8 PMULL
@ instructions are in aesv8-armx.pl.)

#define __ARM_ARCH__ 7

.text
#if defined(__thumb2__) || defined(__clang__)
.syntax	unified
#endif
#if defined(__thumb2__)
.thumb
#else
.code	32
#endif

#ifdef  __clang__
#define ldrplb  ldrbpl
#define ldrneb  ldrbne
#endif

.type	rem_4bit,%object
.align	5
rem_4bit:
.short	0x0000,0x1C20,0x3840,0x2460
.short	0x7080,0x6CA0,0x48C0,0x54E0
.short	0xE100,0xFD20,0xD940,0xC560
.short	0x9180,0x8DA0,0xA9C0,0xB5E0
.size	rem_4bit,.-rem_4bit

.type	rem_4bit_get,%function
rem_4bit_get:
#if defined(__thumb2__)
	adr	r2,rem_4bit
#else
	sub	r2,pc,#8+32	@ &rem_4bit
#endif
	b	.Lrem_4bit_got
	nop
	nop
.size	rem_4bit_get,.-rem_4bit_get

.global	gcm_ghash_4bit
.type	gcm_ghash_4bit,%function
.align	4
gcm_ghash_4bit:
#if defined(__thumb2__)
	adr	r12,rem_4bit
#else
	sub	r12,pc,#8+48		@ &rem_4bit
#endif
	add	r3,r2,r3		@ r3 to point at the end
	stmdb	sp!,{r3-r11,lr}		@ save r3/end too

	ldmia	r12,{r4-r11}		@ copy rem_4bit ...
	stmdb	sp!,{r4-r11}		@ ... to stack

	ldrb	r12,[r2,#15]
	ldrb	r14,[r0,#15]
.Louter:
	eor	r12,r12,r14
	and	r14,r12,#0xf0
	and	r12,r12,#0x0f
	mov	r3,#14

	add	r7,r1,r12,lsl#4
	ldmia	r7,{r4-r7}	@ load Htbl[nlo]
	add	r11,r1,r14
	ldrb	r12,[r2,#14]

	and	r14,r4,#0xf		@ rem
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	add	r14,r14,r14
	eor	r4,r8,r4,lsr#4
	ldrh	r8,[sp,r14]		@ rem_4bit[rem]
	eor	r4,r4,r5,lsl#28
	ldrb	r14,[r0,#14]
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4
	eor	r12,r12,r14
	and	r14,r12,#0xf0
	and	r12,r12,#0x0f
	eor	r7,r7,r8,lsl#16

.Linner:
	add	r11,r1,r12,lsl#4
	and	r12,r4,#0xf		@ rem
	subs	r3,r3,#1
	add	r12,r12,r12
	ldmia	r11,{r8-r11}	@ load Htbl[nlo]
	eor	r4,r8,r4,lsr#4
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	ldrh	r8,[sp,r12]		@ rem_4bit[rem]
	eor	r6,r10,r6,lsr#4
#ifdef	__thumb2__
	it	pl
#endif
	ldrbpl	r12,[r2,r3]
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4

	add	r11,r1,r14
	and	r14,r4,#0xf		@ rem
	eor	r7,r7,r8,lsl#16	@ ^= rem_4bit[rem]
	add	r14,r14,r14
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	eor	r4,r8,r4,lsr#4
#ifdef	__thumb2__
	it	pl
#endif
	ldrbpl	r8,[r0,r3]
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	ldrh	r9,[sp,r14]
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
#ifdef	__thumb2__
	it	pl
#endif
	eorpl	r12,r12,r8
	eor	r7,r11,r7,lsr#4
#ifdef	__thumb2__
	itt	pl
#endif
	andpl	r14,r12,#0xf0
	andpl	r12,r12,#0x0f
	eor	r7,r7,r9,lsl#16	@ ^= rem_4bit[rem]
	bpl	.Linner

	ldr	r3,[sp,#32]		@ re-load r3/end
	add	r2,r2,#16
	mov	r14,r4
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r4,r4
	str	r4,[r0,#12]
#elif defined(__ARMEB__)
	str	r4,[r0,#12]
#else
	mov	r9,r4,lsr#8
	strb	r4,[r0,#12+3]
	mov	r10,r4,lsr#16
	strb	r9,[r0,#12+2]
	mov	r11,r4,lsr#24
	strb	r10,[r0,#12+1]
	strb	r11,[r0,#12]
#endif
	cmp	r2,r3
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r5,r5
	str	r5,[r0,#8]
#elif defined(__ARMEB__)
	str	r5,[r0,#8]
#else
	mov	r9,r5,lsr#8
	strb	r5,[r0,#8+3]
	mov	r10,r5,lsr#16
	strb	r9,[r0,#8+2]
	mov	r11,r5,lsr#24
	strb	r10,[r0,#8+1]
	strb	r11,[r0,#8]
#endif
	
#ifdef __thumb2__
	it	ne
#endif
	ldrbne	r12,[r2,#15]
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r6,r6
	str	r6,[r0,#4]
#elif defined(__ARMEB__)
	str	r6,[r0,#4]
#else
	mov	r9,r6,lsr#8
	strb	r6,[r0,#4+3]
	mov	r10,r6,lsr#16
	strb	r9,[r0,#4+2]
	mov	r11,r6,lsr#24
	strb	r10,[r0,#4+1]
	strb	r11,[r0,#4]
#endif
	
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r7,r7
	str	r7,[r0,#0]
#elif defined(__ARMEB__)
	str	r7,[r0,#0]
#else
	mov	r9,r7,lsr#8
	strb	r7,[r0,#0+3]
	mov	r10,r7,lsr#16
	strb	r9,[r0,#0+2]
	mov	r11,r7,lsr#24
	strb	r10,[r0,#0+1]
	strb	r11,[r0,#0]
#endif
	
	bne	.Louter

	add	sp,sp,#36
#if __ARM_ARCH__>=5
	ldmia	sp!,{r4-r11,pc}
#else
	ldmia	sp!,{r4-r11,lr}
	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	.word	0xe12fff1e			@ interoperable with Thumb ISA:-)
#endif
.size	gcm_ghash_4bit,.-gcm_ghash_4bit

.global	gcm_gmult_4bit
.type	gcm_gmult_4bit,%function
gcm_gmult_4bit:
	stmdb	sp!,{r4-r11,lr}
	ldrb	r12,[r0,#15]
	b	rem_4bit_get
.Lrem_4bit_got:
	and	r14,r12,#0xf0
	and	r12,r12,#0x0f
	mov	r3,#14

	add	r7,r1,r12,lsl#4
	ldmia	r7,{r4-r7}	@ load Htbl[nlo]
	ldrb	r12,[r0,#14]

	add	r11,r1,r14
	and	r14,r4,#0xf		@ rem
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	add	r14,r14,r14
	eor	r4,r8,r4,lsr#4
	ldrh	r8,[r2,r14]	@ rem_4bit[rem]
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4
	and	r14,r12,#0xf0
	eor	r7,r7,r8,lsl#16
	and	r12,r12,#0x0f

.Loop:
	add	r11,r1,r12,lsl#4
	and	r12,r4,#0xf		@ rem
	subs	r3,r3,#1
	add	r12,r12,r12
	ldmia	r11,{r8-r11}	@ load Htbl[nlo]
	eor	r4,r8,r4,lsr#4
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	eor	r5,r5,r6,lsl#28
	ldrh	r8,[r2,r12]	@ rem_4bit[rem]
	eor	r6,r10,r6,lsr#4
#ifdef	__thumb2__
	it	pl
#endif
	ldrbpl	r12,[r0,r3]
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4

	add	r11,r1,r14
	and	r14,r4,#0xf		@ rem
	eor	r7,r7,r8,lsl#16	@ ^= rem_4bit[rem]
	add	r14,r14,r14
	ldmia	r11,{r8-r11}	@ load Htbl[nhi]
	eor	r4,r8,r4,lsr#4
	eor	r4,r4,r5,lsl#28
	eor	r5,r9,r5,lsr#4
	ldrh	r8,[r2,r14]	@ rem_4bit[rem]
	eor	r5,r5,r6,lsl#28
	eor	r6,r10,r6,lsr#4
	eor	r6,r6,r7,lsl#28
	eor	r7,r11,r7,lsr#4
#ifdef	__thumb2__
	itt	pl
#endif
	andpl	r14,r12,#0xf0
	andpl	r12,r12,#0x0f
	eor	r7,r7,r8,lsl#16	@ ^= rem_4bit[rem]
	bpl	.Loop
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r4,r4
	str	r4,[r0,#12]
#elif defined(__ARMEB__)
	str	r4,[r0,#12]
#else
	mov	r9,r4,lsr#8
	strb	r4,[r0,#12+3]
	mov	r10,r4,lsr#16
	strb	r9,[r0,#12+2]
	mov	r11,r4,lsr#24
	strb	r10,[r0,#12+1]
	strb	r11,[r0,#12]
#endif
	
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r5,r5
	str	r5,[r0,#8]
#elif defined(__ARMEB__)
	str	r5,[r0,#8]
#else
	mov	r9,r5,lsr#8
	strb	r5,[r0,#8+3]
	mov	r10,r5,lsr#16
	strb	r9,[r0,#8+2]
	mov	r11,r5,lsr#24
	strb	r10,[r0,#8+1]
	strb	r11,[r0,#8]
#endif
	
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r6,r6
	str	r6,[r0,#4]
#elif defined(__ARMEB__)
	str	r6,[r0,#4]
#else
	mov	r9,r6,lsr#8
	strb	r6,[r0,#4+3]
	mov	r10,r6,lsr#16
	strb	r9,[r0,#4+2]
	mov	r11,r6,lsr#24
	strb	r10,[r0,#4+1]
	strb	r11,[r0,#4]
#endif
	
#if __ARM_ARCH__>=7 && defined(__ARMEL__)
	rev	r7,r7
	str	r7,[r0,#0]
#elif defined(__ARMEB__)
	str	r7,[r0,#0]
#else
	mov	r9,r7,lsr#8
	strb	r7,[r0,#0+3]
	mov	r10,r7,lsr#16
	strb	r9,[r0,#0+2]
	mov	r11,r7,lsr#24
	strb	r10,[r0,#0+1]
	strb	r11,[r0,#0]
#endif
	
#if __ARM_ARCH__>=5
	ldmia	sp!,{r4-r11,pc}
#else
	ldmia	sp!,{r4-r11,lr}
	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	.word	0xe12fff1e			@ interoperable with Thumb ISA:-)
#endif
.size	gcm_gmult_4bit,.-gcm_gmult_4bit
.asciz  "GHASH for ARMv4/NEON, CRYPTOGAMS by <appro@openssl.org>"
.align  2
