
.section .text.itebench
.option rvc
.global bench_harness

# uint32_t bench_harness(uint32_t (*loop_func)(), uint32_t n_ticks);
bench_harness:
	addi sp, sp, -32
	sw ra, 0(sp)
	sw s0, 4(sp)
	sw s1, 8(sp)
	sw s2, 12(sp)
	sw s3, 16(sp)
	sw s4, 20(sp)

	mv s0, a0	# loop_func
	mv s1, a1	# n_ticks
	mv s2, zero	# accumulated iterations
	la s3, 0xf01f00	# timer peripheral block
	# Zero timer so we don't need to think about overflow
	li t0, 0xf
	sb t0, 0x20(s3)	# timer5 control register; write 0xf to clear

bench_harness_loop:
	# loop_func()
	jalr s0
	# accumulate iteration count
	addi s2, s2, 1
	# Read timer value
	lw a0, 0x54(s3)
	bltu a0, s1, bench_harness_loop

bench_harness_done:
	mv a0, s2
	lw s4, 20(sp)
	lw s3, 16(sp)
	lw s2, 12(sp)
	lw s1, 8(sp)
	lw s0, 4(sp)
	lw ra, 0(sp)
	addi sp, sp, 32
	ret