#include <zephyr/arch/riscv/arch.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

uint32_t bench_harness(void (*loop_func)(), uint32_t end_ticks);

static void nop() {
}

static void nop32() {
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
}

static void nop1024() {
	for (volatile int i = 0; i < 1024; i++) {}
}

/* Read 32 bytes, bytewise in order */
static inline __attribute__((always_inline)) void readb32_linear(uintptr_t pp) {
	volatile uint8_t *p = (volatile uint8_t *)pp;
	for (int i = 0; i < 32; i++) {
		p[i];
	}
}

static void readb32_linear_flash() {
	readb32_linear(0x80001234);
}

static void readb32_linear_ram() {
	readb32_linear(0x80100000);
}

/* Read 32 bytes, wordwise in order */
static inline __attribute((always_inline)) void readw32_linear(uintptr_t pp) {
	volatile uint32_t *p = (volatile uint32_t *)pp;
	for (int i = 0; i < 32 / 4; i++) {
		p[i];
	}
}

static void readw32_linear_flash() {
	readw32_linear(0x80002344);
}

static void readw32_linear_ram() {
	readw32_linear(0x80100000);
}

/* Read 32 bytes, bytewise separated by 64 bytes each */
static inline __attribute__((always_inline)) void readb32_scatter(uintptr_t pp, uint32_t factor) {
	volatile uint8_t *p = (volatile uint8_t *)pp;
	for (int i = 0; i < 32; i++) {
		p[i * factor];
	}
}

static void readb32_scatter64_flash() {
	readb32_scatter(0x80007890, 64);
}

static void readb32_scatter64_ram() {
	readb32_scatter(0x80100000, 64);
}

static void readb32_scatter256_flash() {
	readb32_scatter(0x80007890, 256);
}

static void readb32_scatter256_ram() {
	readb32_scatter(0x80100000, 256);
}

static void itebench(const char *name, void (*loop_func)(), uint32_t us) {
	printk("%s.. ", name);
	int key = irq_lock();

	// Execute functions once to cache code
	bench_harness(loop_func, 0);

	uint32_t iteration_count = bench_harness(loop_func, us);

	irq_unlock(key);
	printk("%u iterations in %u us\n", iteration_count, us);
}


static void itebench_run(uint32_t us) {
#define BENCH(f) itebench(#f, f, us)

	BENCH(nop);
	BENCH(nop32);
	BENCH(nop1024);
	BENCH(readb32_linear_flash);
	BENCH(readb32_linear_ram);
	BENCH(readw32_linear_flash);
	BENCH(readw32_linear_ram);
	BENCH(readb32_scatter64_flash);
	BENCH(readb32_scatter64_ram);
	BENCH(readb32_scatter256_flash);
	BENCH(readb32_scatter256_ram);
}

static int cmd_itebench(int argc, char **argv) {
	uint32_t us = 100000;
	int err = 0;

	if (argc == 1) {
		us = shell_strtoul(argv[1], 10, &err);
	}
	if (err || argc > 1)
		return EINVAL;

	itebench_run(us);
	return 0;
}

SHELL_CMD_REGISTER(itebench, NULL, "Run benchmarks", cmd_itebench);