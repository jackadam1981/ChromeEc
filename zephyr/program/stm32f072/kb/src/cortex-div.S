/* 
 * copy ec/core/cortex-m0/xxx.S to this file
 * this file need clean up:
 * 1. remove the unused thumb_func
 * 2. reorder the funcs
 * 3. implement the panic
*/

/* Holds software panic reason PANIC_SW_* */
#define SOFTWARE_PANIC_REASON_REG r4
#define SOFTWARE_PANIC_INFO_REG r5

#define PANIC_SW_BASE 0xDEAD6660

/* Software panic reasons */
#define PANIC_SW_DIV_ZERO (PANIC_SW_BASE + 0)
#define PANIC_SW_STACK_OVERFLOW (PANIC_SW_BASE + 1)
#define PANIC_SW_PD_CRASH (PANIC_SW_BASE + 2)
#define PANIC_SW_ASSERT (PANIC_SW_BASE + 3)
#define PANIC_SW_WATCHDOG (PANIC_SW_BASE + 4)
#define PANIC_SW_BAD_RNG (PANIC_SW_BASE + 5)
#define PANIC_SW_PMIC_FAULT (PANIC_SW_BASE + 6)
#define PANIC_SW_EXIT (PANIC_SW_BASE + 7)

	.syntax unified
	.text
	.thumb
	.cpu cortex-m0

@ int __aeabi_idiv(int num:r0, int denom:r1)
@
@ Divide r0 by r1 and return quotient in r0 (all signed).
@ Use __aeabi_uidivmod() but check signs before and change signs afterwards.
@
	.thumb_func
	.global __aeabi_idiv
__aeabi_idiv:

	cmp	r0, #0
	bge	L_num_pos

	rsbs	r0, r0, #0		@ num = -num

	cmp	r1, #0
	bge	L_neg_result

	rsbs	r1, r1, #0		@ den = -den
	b	__aeabi_uidivmod

L_num_pos:
	cmp	r1, #0
	bge	Luidivmod

	rsbs	r1, r1, #0		@ den = -den

L_neg_result:
	push	{lr}
	bl	__aeabi_uidivmod
	rsbs	r0, r0, #0		@ quot = -quot
	pop	{pc}



@ {int quotient:r0, int remainder:r1}
@ __aeabi_idivmod(int numerator:r0, int denominator:r1)
@
@ Divide r0 by r1 and return the quotient in r0 and the remainder in r1
@
	.thumb_func
	.global __aeabi_idivmod
__aeabi_idivmod:

	cmp	r0, #0
	bge	L_num_pos_bis

	rsbs	r0, r0, #0		@ num = -num

	cmp	r1, #0
	bge	L_neg_both

	rsbs	r1, r1, #0		@ den = -den
	push	{lr}
	bl	__aeabi_uidivmod
	rsbs	r1, r1, #0		@ rem = -rem
	pop	{pc}

L_neg_both:
	push	{lr}
	bl	__aeabi_uidivmod
	rsbs	r0, r0, #0		@ quot = -quot
	rsbs	r1, r1, #0		@ rem = -rem
	pop	{pc}

L_num_pos_bis:
	cmp	r1, #0
	bge	Luidivmod

	rsbs	r1, r1, #0		@ den = -den
	push	{lr}
	bl	__aeabi_uidivmod
	rsbs	r0, r0, #0		@ quot = -quot
	pop	{pc}


@ unsigned __aeabi_uidiv(unsigned num, unsigned denom)
@
@ Just an alias for __aeabi_uidivmod(), the remainder is ignored
@
	.thumb_func
	.global __aeabi_uidiv
__aeabi_uidiv:


@ {unsigned quotient:r0, unsigned remainder:r1}
@  __aeabi_uidivmod(unsigned numerator:r0, unsigned denominator:r1)
@
@ Divide r0 by r1 and return the quotient in r0 and the remainder in r1
@
	.thumb_func
	.global __aeabi_uidivmod
__aeabi_uidivmod:


Luidivmod:
	cmp	r1, #0
	bne	L_no_div0
	b	__aeabi_idiv0
L_no_div0:

	@ Shift left the denominator until it is greater than the numerator
	movs	r2, #1		@ counter
	movs	r3, #0		@ result
	cmp	r0, r1
	bls	L_sub_loop0
	adds	r1, #0		@ dont shift if denominator would overflow
	bmi	L_sub_loop0

L_denom_shift_loop:
	lsls	r2, #1
	lsls	r1, #1
	bmi	L_sub_loop0
	cmp	r0, r1
	bhi	L_denom_shift_loop

L_sub_loop0:
	cmp	r0, r1
	bcc	L_dont_sub0	@ if (num>denom)

	subs	r0, r1		@ numerator -= denom
	orrs	r3, r2		@ result(r3) |= bitmask(r2)
L_dont_sub0:

	lsrs	r1, #1		@ denom(r1) >>= 1
	lsrs	r2, #1		@ bitmask(r2) >>= 1
	bne	L_sub_loop0

	mov	r1, r0		@ remainder(r1) = numerator(r0)
	mov	r0, r3		@ quotient(r0) = result(r3)
	bx	lr

__aeabi_idiv0:
	ldr	SOFTWARE_PANIC_REASON_REG, =PANIC_SW_DIV_ZERO
	mov	SOFTWARE_PANIC_INFO_REG, r14

	@ long long __aeabi_lmul(long long r1:r0, long long r3:r2)
@
@ Multiply r1:r0 and r3:r2 and return the product in r1:r0
@ Can also be used for unsigned long product
@
	.thumb_func
	.global __aeabi_lmul
__aeabi_lmul:

	push	{r4, lr}
	muls	r1, r2
	muls	r3, r0
	adds	r1, r3

	lsrs	r3, r0, #16
	lsrs	r4, r2, #16
	muls	r3, r4
	adds	r1, r3

	lsrs	r3, r0, #16
	uxth	r0, r0
	uxth	r2, r2
	muls	r3, r2
	muls	r4, r0
	muls	r0, r2

	movs	r2, #0
	adds	r3, r4
	adcs	r2, r2
	lsls	r2, #16
	adds	r1, r2

	lsls	r2, r3, #16
	lsrs	r3, #16
	adds	r0, r2
	adcs	r1, r3
	pop	{r4, pc}

.text

.syntax unified
.code 16

/*
 * Helpers for compact switch
 *
 * r0: the table index
 * lr: the table base address (need to clear bit 0)
 *
 * r0 and lr must be PRESERVED.
 * r12 can be clobbered.
 */
.global __gnu_thumb1_case_uqi
.thumb_func
__gnu_thumb1_case_uqi:
	mov     r12, r1
	mov     r1, lr
	lsrs    r1, r1, #1
	lsls    r1, r1, #1
	ldrb    r1, [r1, r0]
	lsls    r1, r1, #1
	add     lr, lr, r1
	mov     r1, r12
	bx      lr

.global __gnu_thumb1_case_sqi
.thumb_func
__gnu_thumb1_case_sqi:
	mov     r12, r1
	mov     r1, lr
	lsrs    r1, r1, #1
	lsls    r1, r1, #1
	ldrsb   r1, [r1, r0]
	lsls    r1, r1, #1
	add     lr, lr, r1
	mov     r1, r12
	bx      lr

.global __gnu_thumb1_case_uhi
.thumb_func
__gnu_thumb1_case_uhi:
	push    {r0, r1}
	mov     r1, lr
	lsrs    r1, r1, #1
	lsls    r0, r0, #1
	lsls    r1, r1, #1
	ldrh    r1, [r1, r0]
	lsls    r1, r1, #1
	add     lr, lr, r1
	pop     {r0, r1}
	bx      lr

.global __gnu_thumb1_case_shi
.thumb_func
__gnu_thumb1_case_shi:
	push    {r0, r1}
	mov     r1, lr
	lsrs    r1, r1, #1
	lsls    r0, r0, #1
	lsls    r1, r1, #1
	ldrsh   r1, [r1, r0]
	lsls    r1, r1, #1
	add     lr, lr, r1
	pop     {r0, r1}
	bx      lr

.global __gnu_thumb1_case_si
.thumb_func
__gnu_thumb1_case_si:
	push	{r0, r1}
	mov	r1, lr
	adds.n	r1, r1, #2
	lsrs	r1, r1, #2
	lsls	r0, r0, #2
	lsls	r1, r1, #2
	ldr	r0, [r1, r0]
	adds	r0, r0, r1
	mov	lr, r0
	pop	{r0, r1}
	mov	pc, lr


@ {unsigned long long quotient, unsigned long long remainder}
@ __aeabi_uldivmod(unsigned long long numerator, unsigned long long denominator)
@
@ Divide r1:r0 by r3:r2 and return the quotient in r1:r0 and the remainder
@ in r3:r2 (all unsigned)
@
	.thumb_func
        .global __aeabi_uldivmod
__aeabi_uldivmod:

	cmp	r3, #0
	bne	L_large_denom
	cmp	r2, #0
	beq	L_divison_by_0
	cmp	r1, #0
	beq	L_fallback_32bits



	@ case 1: num >= 2^32 and denom < 2^32
	@ Result might be > 2^32, therefore we first calculate the upper 32
	@ bits of the result. It is done similar to the calculation of the
	@ lower 32 bits, but with a denominator that is shifted by 32.
	@ Hence the lower 32 bits of the denominator are always 0 and the
	@ costly 64 bit shift and sub operations can be replaced by cheap 32
	@ bit operations.

	push	{r4, r5, r6, r7, lr}

	@ shift left the denominator until it is greater than the numerator
	@ denom(r7:r6) = r3:r2 << 32

	movs	r5, #1		@ bitmask
	adds	r7, r2, #0	@ dont shift if denominator would overflow
	bmi	L_upper_result
	cmp	r1, r7
	blo	L_upper_result

L_denom_shift_loop1:
	lsls	r5, #1
	lsls	r7, #1
	bmi	L_upper_result	@ dont shift if overflow
	cmp	r1, r7
	bhs	L_denom_shift_loop1

L_upper_result:
	mov	r3, r1
	mov	r2, r0
	movs	r1, #0		@ upper result = 0

L_sub_loop1:
	cmp	r3, r7
	bcc	L_dont_sub1	@ if (num>denom)

	subs	r3, r7		@ num -= denom
	orrs	r1, r5		@ result(r7:r6) |= bitmask(r5)
L_dont_sub1:

	lsrs	r7, #1		@ denom(r7:r6) >>= 1
	lsrs	r5, #1		@ bitmask(r5) >>= 1
	bne	L_sub_loop1

	movs	r5, #1
	lsls	r5, #31
	movs	r6, #0
	b	L_lower_result



	@ case 2: division by 0
	@ call __aeabi_ldiv0

L_divison_by_0:
	b	__aeabi_ldiv0



	@ case 3: num < 2^32 and denom < 2^32
	@ fallback to 32 bit division

L_fallback_32bits:
	mov	r1, r2
	push	{lr}
	bl	__aeabi_uidivmod
	mov	r2, r1
	movs	r1, #0
	movs	r3, #0
	pop	{pc}



	@ case 4: denom >= 2^32
	@ result is smaller than 2^32

L_large_denom:
	push	{r4, r5, r6, r7, lr}

	mov	r7, r3
	mov	r6, r2
	mov	r3, r1
	mov	r2, r0

	@ Shift left the denominator until it is greater than the numerator

	movs	r1, #0		@ high word of result is 0
	movs	r5, #1		@ bitmask
	adds	r7, #0		@ dont shift if denominator would overflow
	bmi	L_lower_result
	cmp	r3, r7
	blo	L_lower_result

L_denom_shift_loop4:
	lsls	r5, #1
	lsls	r7, #1
	lsls	r6, #1
	adcs	r7, r1		@ r1=0
	bmi	L_lower_result	@ dont shift if overflow
	cmp	r3, r7
	bhs	L_denom_shift_loop4



L_lower_result:
	movs	r0, #0

L_sub_loop4:
	mov	r4, r3
	cmp	r2, r6
	sbcs	r4, r7
	bcc	L_dont_sub4	@ if (num>denom)

	subs	r2, r6		@ numerator -= denom
	sbcs	r3, r7
	orrs	r0, r5		@ result(r1:r0) |= bitmask(r5)
L_dont_sub4:

	lsls	r4, r7, #31	@ denom(r7:r6) >>= 1
	lsrs	r6, #1
	lsrs	r7, #1
	orrs	r6, r4
	lsrs	r5, #1		@ bitmask(r5) >>= 1
	bne	L_sub_loop4

	pop	{r4, r5, r6, r7, pc}

__aeabi_ldiv0:
	ldr	SOFTWARE_PANIC_REASON_REG, =PANIC_SW_DIV_ZERO
	mov	SOFTWARE_PANIC_INFO_REG, r14
