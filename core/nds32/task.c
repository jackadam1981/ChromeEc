/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Task scheduling / events module for Chrome EC operating system */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "link_defs.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

typedef union {
	struct {
		/*
		 * Note that sp must be the first element in the task struct
		 * for __switchto() to work.
		 */
		uint32_t sp;       /* Saved stack pointer for context switch */
		uint32_t events;   /* Bitmaps of received events */
		uint64_t runtime;  /* Time spent in task */
		uint32_t *stack;   /* Start of stack */
	};
} task_;

/* Value to store in unused stack */
#define STACK_UNUSED_VALUE 0xdeadd00d

/* declare task routine prototypes */
#define TASK(n, r, d, s) int r(void *);
void __idle(void);
CONFIG_TASK_LIST
CONFIG_TEST_TASK_LIST
#undef TASK

/* Task names for easier debugging */
#define TASK(n, r, d, s)  #n,
static const char * const task_names[] = {
	"<< idle >>",
	CONFIG_TASK_LIST
	CONFIG_TEST_TASK_LIST
};
#undef TASK

extern int __task_start(void);

#ifndef CONFIG_LOW_POWER_IDLE
/* Idle task.  Executed when no tasks are ready to be scheduled. */
void __idle(void)
{
	/*
	 * Print when the idle task starts.  This is the lowest priority task,
	 * so this only starts once all other tasks have gotten a chance to do
	 * their task inits and have gone to sleep.
	 */
	cprintf(CC_TASK, "[%T idle task started]\n");

	while (1) {
		/*
		 * Wait for the next irq event.  This stops the CPU clock
		 * (sleep / deep sleep, depending on chip config).
		 */
		/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
	}
}
#endif /* !CONFIG_LOW_POWER_IDLE */

static void task_exit_trap(void)
{
	int i = task_get_current();
	cprintf(CC_TASK, "[%T Task %d (%s) exited!]\n", i, task_names[i]);
	/* Exited tasks simply sleep forever */
	while (1)
		task_wait_event(-1);
}

/* Startup parameters for all tasks. */
#define TASK(n, r, d, s)  {	\
	.r0 = (uint32_t)d,	\
	.pc = (uint32_t)r,	\
	.stack_size = s,	\
},
static const struct {
	uint32_t r0;
	uint32_t pc;
	uint16_t stack_size;
} const tasks_init[] = {
	TASK(IDLE, __idle, 0, IDLE_TASK_STACK_SIZE)
	CONFIG_TASK_LIST
	CONFIG_TEST_TASK_LIST
};
#undef TASK

/* Contexts for all tasks */
static task_ tasks[TASK_ID_COUNT];
/* Sanity checks about static task invariants */
BUILD_ASSERT(TASK_ID_COUNT <= sizeof(unsigned) * 8);
BUILD_ASSERT(TASK_ID_COUNT < (1 << (sizeof(task_id_t) * 8)));


/* Stacks for all tasks */
#define TASK(n, r, d, s)  + s
uint8_t task_stacks[0
		    TASK(IDLE, __idle, 0, IDLE_TASK_STACK_SIZE)
		    CONFIG_TASK_LIST
		    CONFIG_TEST_TASK_LIST
] __attribute__((aligned(8)));

#undef TASK

/* Reserve space to discard context on first context switch. */
#ifdef CONFIG_FPU
uint32_t scratchpad[17+18];
#else
uint32_t scratchpad[17];
#endif

task_ *current_task = (task_ *)scratchpad;

/*
 * Should IRQs chain to svc_handler()?  This should be set if either of the
 * following is true:
 *
 * 1) Task scheduling has started, and task profiling is enabled.  Task
 * profiling does its tracking in svc_handler().
 *
 * 2) An event was set by an interrupt; this could result in a higher-priority
 * task unblocking.  After checking for a task switch, svc_handler() will clear
 * the flag (unless profiling is also enabled; then the flag remains set).
 */
int need_resched = 0;

/*
 * Bitmap of all tasks ready to be run.
 *
 * Currently all tasks are enabled at startup.
 */
static uint32_t tasks_ready = (1<<TASK_ID_COUNT) - 1;

static int start_called;  /* Has task swapping started */

static inline task_ *__task_id_to_ptr(task_id_t id)
{
	return tasks + id;
}

static void set_psw(uint32_t val)
{
	asm volatile ("mtsr %0, $PSW \n"::"r"(val));
}

static uint32_t get_psw(void)
{
	uint32_t ret;
	asm volatile ("mfsr %0, $PSW \n":"=r"(ret));
	return ret;
}

void interrupt_disable(void)
{
	/* clear GIE (Global Interrupt Enable) bit */
	set_psw(get_psw() & ~1);
	/* TODO(crosbug.com/p/23574): that's not quite atomic ??? */
}

void interrupt_enable(void)
{
	/* set GIE (Global Interrupt Enable) bit */
	set_psw(get_psw() | 1);
	/* TODO(crosbug.com/p/23574): that's not quite atomic ??? */
}

inline int in_interrupt_context(void)
{
	/* check INTL (Interrupt Stack Level) bits */
	return get_psw() & 0x6;
}

inline int get_interrupt_context(void)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
	return 0;
}

task_id_t task_get_current(void)
{
	return current_task - tasks;
}

uint32_t *task_get_event_bitmap(task_id_t tskid)
{
	task_ *tsk = __task_id_to_ptr(tskid);
	return &tsk->events;
}

int task_start_called(void)
{
	return start_called;
}

/**
 * Scheduling system call
 */
void syscall_handler(int desched, task_id_t resched)
{
	if (desched && !current_task->events) {
		/*
		 * Remove our own ready bit (current - tasks is same as
		 * task_get_current())
		 */
		tasks_ready &= ~(1 << (current_task - tasks));
	}
	tasks_ready |= 1 << resched;

	/* trigger a re-scheduling on exit */
	need_resched = 1;

}

task_ *next_sched_task(void)
{
	return __task_id_to_ptr(31 - __builtin_clz(tasks_ready));
}

void __schedule(int desched, int resched)
{
	register int p0 asm("$r0") = desched;
	register int p1 asm("$r1") = resched;

	asm("syscall 0"::"r"(p0),"r"(p1));
}

static uint32_t __wait_evt(int timeout_us, task_id_t resched)
{
	task_ *tsk = current_task;
	task_id_t me = tsk - tasks;
	uint32_t evt;
	int ret;

	ASSERT(!in_interrupt_context());

	if (timeout_us > 0) {
		timestamp_t deadline = get_time();
		deadline.val += timeout_us;
		ret = timer_arm(deadline, me);
		ASSERT(ret == EC_SUCCESS);
	}
	while (!(evt = atomic_read_clear(&tsk->events)))
	{
		/* Remove ourself and get the next task in the scheduler */
		__schedule(1, resched);
		resched = TASK_ID_IDLE;
	}
	if (timeout_us > 0)
		timer_cancel(me);
	return evt;
}

uint32_t task_set_event(task_id_t tskid, uint32_t event, int wait)
{
	task_ *receiver = __task_id_to_ptr(tskid);
	ASSERT(receiver);

	/* Set the event bit in the receiver message bitmap */
	atomic_or(&receiver->events, event);

	/* Re-schedule if priorities have changed */
	if (in_interrupt_context()) {
		/* The receiver might run again */
		atomic_or(&tasks_ready, 1 << tskid);
		need_resched = 1;
	} else {
		if (wait)
			return __wait_evt(-1, tskid);
		else
			__schedule(0, tskid);
	}

	return 0;
}

uint32_t task_wait_event(int timeout_us)
{
	return __wait_evt(timeout_us, TASK_ID_IDLE);
}

static uint32_t get_int_mask(void)
{
	uint32_t ret;
	asm volatile ("mfsr %0, $INT_MASK \n":"=r"(ret));
	return ret;
}

static void set_int_mask(uint32_t val)
{
	asm volatile ("mtsr %0, $INT_MASK \n"::"r"(val));
}

void task_enable_irq(int irq)
{
	set_int_mask(get_int_mask() | (1 << irq));
}

void task_disable_irq(int irq)
{
	set_int_mask(get_int_mask() & ~(1 << irq));
}

void task_clear_pending_irq(int irq)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

void task_trigger_irq(int irq)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

/*
 * Initialize IRQs in the NVIC and set their priorities as defined by the
 * DECLARE_IRQ statements.
 */
static void __nvic_init_irqs(void)
{
	/* Get the IRQ priorities section from the linker */
	/*
	int exc_calls = __irqprio_end - __irqprio;
	*/

	/* Mask and clear all pending interrupts */
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */

	/*
	 * Re-enable global interrupts in case they're disabled.  On a reboot,
	 * they're already enabled; if we've jumped here from another image,
	 * they're not.
	 */
	interrupt_enable();

	/* Set priorities */
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

void mutex_lock(struct mutex *mtx)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

void mutex_unlock(struct mutex *mtx)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

void task_print_list(void)
{
	int i;

	ccputs("Task Ready Name         Events      Time (s)  StkUsed\n");

	for (i = 0; i < TASK_ID_COUNT; i++) {
		char is_ready = (tasks_ready & (1<<i)) ? 'R' : ' ';
		uint32_t *sp;

		int stackused = tasks_init[i].stack_size;

		for (sp = tasks[i].stack;
		     sp < (uint32_t *)tasks[i].sp && *sp == STACK_UNUSED_VALUE;
		     sp++)
			stackused -= sizeof(uint32_t);

		ccprintf("%4d %c %-16s %08x %11.6ld  %3d/%3d\n", i, is_ready,
			 task_names[i], tasks[i].events, tasks[i].runtime,
			 stackused, tasks_init[i].stack_size);
		cflush();
	}
}

int command_task_info(int argc, char **argv)
{
	task_print_list();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(taskinfo, command_task_info,
			NULL,
			"Print task info",
			NULL);

static int command_task_ready(int argc, char **argv)
{
	if (argc < 2) {
		ccprintf("tasks_ready: 0x%08x\n", tasks_ready);
	} else {
		tasks_ready = strtoi(argv[1], NULL, 16);
		ccprintf("Setting tasks_ready to 0x%08x\n", tasks_ready);
		__schedule(0, 0);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(taskready, command_task_ready,
			"[setmask]",
			"Print/set ready tasks",
			NULL);

void task_pre_init(void)
{
	uint32_t *stack_next = (uint32_t *)task_stacks;
	int i;

	/* Fill the task memory with initial values */
	for (i = 0; i < TASK_ID_COUNT; i++) {
		uint32_t *sp;
		/* Stack size in words */
		uint32_t ssize = tasks_init[i].stack_size / 4;

		tasks[i].stack = stack_next;

		/*
		 * Update stack used by first frame: 15 regs + PC
		 */
		sp = stack_next + ssize - 16;
		tasks[i].sp = (uint32_t)sp;

		/* Initial context on stack (see __switchto()) */
		sp[6] = tasks_init[i].r0;           /* r0 */
		sp[14] = (uint32_t)task_exit_trap;  /* lr */
		sp[0] = tasks_init[i].pc;           /* pc */
		sp[15] = (uint32_t)sp;              /* sp */

		/* Fill unused stack; also used to detect stack overflow. */
		for (sp = stack_next; sp < (uint32_t *)tasks[i].sp; sp++)
			*sp = STACK_UNUSED_VALUE;

		stack_next += ssize;
	}

	/*
	 * Fill in guard value in scratchpad to prevent stack overflow
	 * detection failure on the first context switch.  This works because
	 * the first word in the scratchpad is where the switcher will store
	 * sp, so it's ok to blow away.
	 */
	((task_ *)scratchpad)->stack = (uint32_t *)scratchpad;
	*(uint32_t *)scratchpad = STACK_UNUSED_VALUE;

	/* Initialize IRQs */
	__nvic_init_irqs();
}

int task_start(void)
{
	start_called = 1;

	return __task_start();
}
