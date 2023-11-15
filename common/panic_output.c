/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "base_state.h"
#include "body_detection.h"
#include "builtin/assert.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "extpower.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "math_util.h"
#include "panic.h"
#include "port80.h"
#include "power.h"
#include "printf.h"
#include "software_panic.h"
#include "sysjump.h"
#include "system.h"
#include "tablet_mode.h"
#include "task.h"
#include "usb_tc_sm.h"
#include "timer.h"
#include "uart.h"
#include "usb_console.h"
#include "usb_pd.h"
#include "util.h"

/*
 * For host tests, use a static area for panic data.
 */
#ifdef CONFIG_BOARD_NATIVE_POSIX
static struct panic_data zephyr_panic_data;
#undef PANIC_DATA_PTR
#undef CONFIG_PANIC_DATA_BASE
#define PANIC_DATA_PTR (&zephyr_panic_data)
#define CONFIG_PANIC_DATA_BASE (&zephyr_panic_data)
#endif
/* Panic data goes at the end of RAM. */
static struct panic_data *const pdata_ptr = PANIC_DATA_PTR;

/* Common SW Panic reasons strings */
const char *const panic_sw_reasons[] = {
	"PANIC_SW_DIV_ZERO",	 "PANIC_SW_STACK_OVERFLOW",
	"PANIC_SW_PD_CRASH",	 "PANIC_SW_ASSERT",
	"PANIC_SW_WATCHDOG",	 "PANIC_SW_RNG",
	"PANIC_SW_PMIC_FAULT",	 "PANIC_SW_EXIT",
	"PANIC_SW_WATCHDOG_WARN"
};

/**
 * Check an interrupt vector as being a valid software panic
 * @param reason	Reason for panic
 * @return 0 if not a valid software panic reason, otherwise non-zero.
 */
int panic_sw_reason_is_valid(uint32_t reason)
{
	return (reason >= PANIC_SW_BASE &&
		(reason - PANIC_SW_BASE) < ARRAY_SIZE(panic_sw_reasons));
}

/**
 * Add a character directly to the UART buffer.
 *
 * @param context	Context; ignored.
 * @param c		Character to write.
 * @return 0 if the character was transmitted, 1 if it was dropped.
 */
#ifndef CONFIG_DEBUG_PRINTF
static int panic_txchar(void *context, int c)
{
	if (c == '\n')
		panic_txchar(context, '\r');

	/* Wait for space in transmit FIFO */
	while (!uart_tx_ready())
		;

	/* Write the character directly to the transmit FIFO */
	uart_write_char(c);

	return 0;
}

void panic_puts(const char *outstr)
{
	/* Flush the output buffer */
	uart_flush_output();

	/* Put all characters in the output buffer */
	while (*outstr) {
		/* Send the message to the UART console */
		panic_txchar(NULL, *outstr);
#if defined(CONFIG_USB_CONSOLE) || defined(CONFIG_USB_CONSOLE_STREAM)
		/*
		 * Send the message to the USB console
		 * on platforms which support it.
		 */
		usb_puts(outstr);
#endif
		++outstr;
	}

	/* Flush the transmit FIFO */
	uart_tx_flush();
}

void panic_printf(const char *format, ...)
{
	va_list args;

	/* Flush the output buffer */
	uart_flush_output();

	va_start(args, format);
	/* Send the message to the UART console */
	vfnprintf(panic_txchar, NULL, format, args);
#if defined(CONFIG_USB_CONSOLE) || defined(CONFIG_USB_CONSOLE_STREAM)
	/* Send the message to the USB console on platforms which support it. */
	usb_vprintf(format, args);
#endif

	va_end(args);

	/* Flush the transmit FIFO */
	uart_tx_flush();
}
#endif

/**
 * Display a message and reboot
 */
void panic_reboot(void)
{
	panic_puts("\n\nRebooting...\n");
	system_reset(0);
}

/* Complete the processing of a panic, after the initial message is shown */
test_mockable_static
#if !(defined(TEST_FUZZ) || defined(CONFIG_ZTEST))
	noreturn
#endif
	void
	complete_panic(const char *fname, int linenum)
{
	/* Top two bytes of info register is first two characters of file name.
	 * Bottom two bytes of info register is line number.
	 */
	software_panic(PANIC_SW_ASSERT, (fname[0] << 24) | (fname[1] << 16) |
						(linenum & 0xffff));
}

#ifdef CONFIG_DEBUG_ASSERT_BRIEF
void panic_assert_fail(const char *fname, int linenum)
{
	panic_printf("\nASSERTION FAILURE at %s:%d\n", fname, linenum);
	complete_panic(fname, linenum);
}
#else
void panic_assert_fail(const char *msg, const char *func, const char *fname,
		       int linenum)
{
	panic_printf("\nASSERTION FAILURE '%s' in %s() at %s:%d\n", msg, func,
		     fname, linenum);
	complete_panic(fname, linenum);
}
#endif

void panic(const char *msg)
{
	panic_printf("\n** PANIC: %s\n", msg);
	panic_reboot();
}

struct panic_data *panic_get_data(void)
{
	BUILD_ASSERT(sizeof(struct panic_data) <= CONFIG_PANIC_DATA_SIZE);

	if (pdata_ptr->magic != PANIC_DATA_MAGIC ||
	    pdata_ptr->struct_size != CONFIG_PANIC_DATA_SIZE)
		return NULL;

	return pdata_ptr;
}

/*
 * Returns pointer to beginning of panic data.
 * Please note that it is not safe to interpret this
 * pointer as panic_data structure.
 */
uintptr_t get_panic_data_start(void)
{
	if (pdata_ptr->magic != PANIC_DATA_MAGIC)
		return 0;

	if (IS_ENABLED(CONFIG_BOARD_NATIVE_POSIX))
		return (uintptr_t)pdata_ptr;

	/* LCOV_EXCL_START - Can't cover non posix lines (yet) */
	return ((uintptr_t)CONFIG_PANIC_DATA_BASE + CONFIG_PANIC_DATA_SIZE -
		pdata_ptr->struct_size);
	/* LCOV_EXCL_STOP */
}

static uint32_t get_panic_data_size(void)
{
	if (pdata_ptr->magic != PANIC_DATA_MAGIC)
		return 0;

	return pdata_ptr->struct_size;
}

void fill_panic_context(panic_context *context, uint8_t reason, uint8_t in_isr)
{
	struct batt_params battery_params;

	/* Clear context first */
	memset(context, 0, sizeof(*context));

	// /* Meta info */
	// uint8_t struct_version:4; /* Currently == 1 */
	// uint8_t reserved:4; /* Reserved for future meta info set to 0 */
	context->fields.struct_version = 1;
	context->fields.reserved = 0;

	// /* Panic info */
	// uint8_t reason:5; /* e.g. PANIC_SW_DIV_ZERO - PANIC_SW_BASE */
	context->fields.reason = reason;
	context->fields.in_isr = in_isr;

	// /* System info */
	// uint8_t rw_image:1; /* 0 = RO, 1 = RW*/
	context->fields.rw_image = system_get_image_copy() == EC_IMAGE_RW;
	// uint8_t current_task:5; /* Current or most recent task */
	context->fields.current_task = task_get_current();
	// uint8_t last_hook:5; /* e.g. HOOK_INIT */
	context->fields.last_hook = get_last_hook_notify();
	// uint8_t last_irq:8; /* IRQs above 0xff (unusual) are truncated to
	// 0xff */
	context->fields.last_irq = task_get_last_irq();
	// uint8_t last_irq_count_log2:5; /* Last IRQ count, log2 scaled */
	context->fields.last_irq_count_log2 = log2(task_get_last_irq_count());
	// uint8_t elapsed_time_log2us:6; /* Time in us, log2 scaled */
	context->fields.elapsed_time_log2us = log2(get_time().val);
	// uint8_t last_host_event:6; /* 0-64 */
	context->fields.last_host_event = get_last_host_event();
	// uint16_t last_host_command:12; /* Host commands above 0xFFF (unusual)
	// are truncated to 0xFFF */
	context->fields.last_host_command = get_last_host_command();
	// uint16_t last_port80:16; /* 4 byte port 80 codes are truncated to
	// 0xFFFF bytes */
	context->fields.last_port80 = port_80_last();
	// uint8_t reset_flag_l:5; /* Lowest reset flag that is set */
	context->fields.reset_flag_l = __builtin_ffs(system_get_reset_flags());
	// uint8_t reset_flag_h:5; /* Highest reset flag that is set */
	context->fields.reset_flag_h = __builtin_clz(system_get_reset_flags());

	// /* Power State */
	// uint8_t power_state:5; /* e.g. POWER_G3, POWER_S0ix */
	context->fields.power_state = power_get_state();
	// uint8_t power_signals:8;
	context->fields.power_signals = power_get_signals();

	// /* Charger and battery info */
	// uint8_t battery_level:7; /* Normalized to 0-100 */
	// uint8_t battery_status:2;
	// uint8_t charge_state:2; /* precharge, idle, charge, discharge */
	battery_get_params(&battery_params);
	context->fields.battery_level = battery_params.state_of_charge;
	context->fields.battery_status = battery_params.status;
	context->fields.charge_state = charge_get_status()->state;

	// /* Physical State */
	// uint8_t lid_open:1;
	// uint8_t tablet:1;
	// uint8_t detached:1;
	// uint8_t body_detect:1;
	context->fields.lid_open = lid_is_open();
	context->fields.tablet = IS_ENABLED(CONFIG_TABLET_MODE_SWITCH) &&
				 tablet_get_mode();
	context->fields.detached = IS_ENABLED(CONFIG_BASE_ATTACHED_SWITCH) &&
				   !base_get_state();
	context->fields.body_detect =
		IS_ENABLED(CONFIG_BODY_DETECTION_NOTIFY_MKBP) &&
		!body_detect_get_state();

	// /* PD State */
	// uint8_t pd0_state:6;
	// uint8_t pd1_state:6;
	context->fields.pd0_state = pd_get_task_state(0);
	context->fields.pd1_state = pd_get_task_state(1);
}

extern const char *const *power_state_names;
extern const char *const *charge_state_names;

void pretty_print_panic_context(const panic_context *context)
{
	const char *reason_to_str[] = { "DIV_ZERO",	"STACK_OVERFLOW",
					"PD_CRASH",	"ASSERT",
					"WATCHDOG",	"BAD_RNG",
					"PMIC_FAULT",	"EXIT",
					"WATCHDOG_WARN" };
	ccprintf("struct_version:   %d\n", context->fields.struct_version);
	if (context->fields.reason < ARRAY_SIZE(reason_to_str))
		ccprintf("reason:           %s\n",
			 reason_to_str[context->fields.reason]);
	else
		ccprintf("reason:           %s\n", "UNKNOWN");
	ccprintf("in_isr:           %d\n", context->fields.in_isr);

	ccprintf("image:            %s\n",
		 context->fields.rw_image ? "RW" : "RO");
	ccprintf("task:             %x\n", context->fields.current_task);
	ccprintf("last_hook:        0x%x\n", context->fields.last_hook);
	ccprintf("last_irq:         0x%x\n", context->fields.last_irq);
	ccprintf("last_irq_count:   %d\n",
		 1 << context->fields.last_irq_count_log2);
	ccprintf("elapsed_time      %llus\n",
		 (1ULL << context->fields.elapsed_time_log2us) / 1000 / 1000);
	ccprintf("last_host_event:  0x%x\n", context->fields.last_host_event);
	ccprintf("last_host_command:0x%x\n", context->fields.last_host_command);
	ccprintf("last_port80:      0x%x\n", context->fields.last_port80);
	ccprintf("reset_flags:      0x%x\n",
		 (1 << context->fields.reset_flag_l) |
			 (1 << context->fields.reset_flag_h));
	ccprintf("power_state:      %s\n",
		 power_state_names[context->fields.power_state]);
	ccprintf("power_signals:    0x%x\n", context->fields.power_signals);
	ccprintf("battery_level:    %d%%\n", context->fields.battery_level);
	ccprintf("battery_status:   0x%x\n", context->fields.battery_status);
	ccprintf("charge_state:     %s\n",
		 charge_state_names[context->fields.charge_state]);
	ccprintf("lid_open:         %d\n", context->fields.lid_open);
	ccprintf("tablet:           %d\n", context->fields.tablet);
	ccprintf("detached:         %d\n", context->fields.detached);
	ccprintf("body_detect:      %d\n", context->fields.body_detect);
	if (IS_ENABLED(USB_PD_DEBUG_LABELS)) {
		ccprintf("pd0_state:        %s\n",
			 tc_get_current_state(context->fields.pd0_state));
		ccprintf("pd0_state:        %s\n",
			 tc_get_current_state(context->fields.pd1_state));
	} else {
		ccprintf("pd0_state:        0x%x\n", context->fields.pd0_state);
		ccprintf("pd0_state:        0x%x\n", context->fields.pd1_state);
	}
}

/*
 * Returns pointer to panic_data structure that can be safely written.
 * Please note that this function can move jump data and jump tags.
 * It can also delete panic data from previous boot, so this function
 * should be used when we are sure that we don't need it.
 */
#ifdef CONFIG_BOARD_NATIVE_POSIX
struct panic_data *test_get_panic_data_pointer(void)
{
	return pdata_ptr;
}
#endif

__overridable uint32_t get_panic_stack_pointer(const struct panic_data *pdata)
{
	/* Not Implemented */
	return 0;
}

test_mockable struct panic_data *get_panic_data_write(void)
{
	/*
	 * Pointer to panic_data structure. It may not point to
	 * the beginning of structure, but accessing struct_size
	 * and magic is safe because it is always placed at the
	 * end of RAM.
	 */
	struct panic_data *const pdata_ptr = PANIC_DATA_PTR;
	struct jump_data *jdata_ptr;
	uintptr_t data_begin;
	size_t move_size;
	int delta;

	/*
	 * If panic data exists, jump data and jump tags should be moved
	 * about difference between size of panic_data structure and size of
	 * structure that is present in memory.
	 *
	 * If panic data doesn't exist, lets create place for a one
	 */
	if (pdata_ptr->magic == PANIC_DATA_MAGIC)
		delta = CONFIG_PANIC_DATA_SIZE - pdata_ptr->struct_size;
	else
		delta = CONFIG_PANIC_DATA_SIZE;

	/* If delta is 0, there is no need to move anything */
	if (delta == 0)
		return pdata_ptr;

	/*
	 * Expecting get_panic_data_start() will return a pointer to
	 * the beginning of panic data, or NULL if no panic data available
	 */
	data_begin = get_panic_data_start();
	if (!data_begin)
		data_begin = CONFIG_RAM_BASE + CONFIG_RAM_SIZE;

	jdata_ptr = (struct jump_data *)(data_begin - sizeof(struct jump_data));

	/*
	 * If we don't have valid jump_data structure we don't need to move
	 * anything and can just return pdata_ptr (clear memory, set magic
	 * and struct_size first).
	 */
	if (jdata_ptr->magic != JUMP_DATA_MAGIC || jdata_ptr->version < 1 ||
	    jdata_ptr->version > 3) {
		memset(pdata_ptr, 0, CONFIG_PANIC_DATA_SIZE);
		pdata_ptr->magic = PANIC_DATA_MAGIC;
		pdata_ptr->struct_size = CONFIG_PANIC_DATA_SIZE;

		return pdata_ptr;
	}

	move_size = 0;
	if (jdata_ptr->version == 1)
		move_size = JUMP_DATA_SIZE_V1;
	else if (jdata_ptr->version == 2)
		move_size = JUMP_DATA_SIZE_V2 + jdata_ptr->jump_tag_total;
	else if (jdata_ptr->version == 3)
		move_size = jdata_ptr->struct_size + jdata_ptr->jump_tag_total;

	/* Check if there's enough space for jump tags after move */
	if (data_begin - move_size < JUMP_DATA_MIN_ADDRESS) {
		/* Not enough room for jump tags, clear tags.
		 * TODO(b/251190975): This failure should be reported
		 * in the panic data structure for more visibility.
		 */
		/* LCOV_EXCL_START - JUMP_DATA_MIN_ADDRESS is 0 in test builds
		 * and we cannot go negative by subtracting unsigned ints.
		 */
		move_size -= jdata_ptr->jump_tag_total;
		jdata_ptr->jump_tag_total = 0;
		/* LCOV_EXCL_STOP */
	}

	data_begin -= move_size;

	if (move_size != 0) {
		/* Move jump_tags and jump_data */
		memmove((void *)(data_begin - delta), (void *)data_begin,
			move_size);
	}

	/*
	 * Now we are sure that there is enough space for current
	 * panic_data structure.
	 */
	memset(pdata_ptr, 0, CONFIG_PANIC_DATA_SIZE);
	pdata_ptr->magic = PANIC_DATA_MAGIC;
	pdata_ptr->struct_size = CONFIG_PANIC_DATA_SIZE;

	return pdata_ptr;
}

static void panic_init(void)
{
#ifdef CONFIG_HOSTCMD_EVENTS
	struct panic_data *addr = panic_get_data();

	/* Notify host of new panic event */
	if (addr && !(addr->flags & PANIC_DATA_FLAG_OLD_HOSTEVENT)) {
		host_set_single_event(EC_HOST_EVENT_PANIC);
		addr->flags |= PANIC_DATA_FLAG_OLD_HOSTEVENT;
	}
#endif
}
DECLARE_HOOK(HOOK_INIT, panic_init, HOOK_PRIO_LAST);
DECLARE_HOOK(HOOK_CHIPSET_RESET, panic_init, HOOK_PRIO_LAST);

#ifdef CONFIG_CMD_CRASH
/*
 * Disable infinite recursion warning, since we're intentionally doing that
 * here.
 */
DISABLE_CLANG_WARNING("-Winfinite-recursion")
#if __GNUC__ >= 12
DISABLE_GCC_WARNING("-Winfinite-recursion")
#endif
static void stack_overflow_recurse(int n)
{
	panic_printf("+%d", n);

	/*
	 * Force task context switch, since that's where we do stack overflow
	 * checking.
	 */
	msleep(10);

	stack_overflow_recurse(n + 1);

	/*
	 * Do work after the recursion, or else the compiler uses tail-chaining
	 * and we don't actually consume additional stack.
	 */
	panic_printf("-%d", n);
}
ENABLE_CLANG_WARNING("-Winfinite-recursion")
#if __GNUC__ >= 12
ENABLE_GCC_WARNING("-Winfinite-recursion")
#endif

/*****************************************************************************/
/* Console commands */
static int command_crash(int argc, const char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM1;

	if (!strcasecmp(argv[1], "assert")) {
		ASSERT(0);
	} else if (!strcasecmp(argv[1], "divzero")) {
		volatile int zero = 0;

		cflush();
		ccprintf("%08x", 1 / zero);
	} else if (!strcasecmp(argv[1], "udivzero")) {
		volatile int zero = 0;

		cflush();
		ccprintf("%08x", 1U / zero);
	} else if (!strcasecmp(argv[1], "stack")) {
		stack_overflow_recurse(1);
#ifndef CONFIG_ALLOW_UNALIGNED_ACCESS
	} else if (!strcasecmp(argv[1], "unaligned")) {
		volatile intptr_t unaligned_ptr = 0xcdef;
		cflush();
		ccprintf("%08x", *(volatile int *)unaligned_ptr);
#endif /* !CONFIG_ALLOW_UNALIGNED_ACCESS */
	} else if (!strcasecmp(argv[1], "watchdog")) {
		while (1) {
/* Yield on native posix to avoid locking up the simulated sys clock */
#ifdef CONFIG_ARCH_POSIX
			k_cpu_idle();
#endif
		}
	} else if (!strcasecmp(argv[1], "hang")) {
		uint32_t lock_key = irq_lock();

		while (1) {
/* Yield on native posix to avoid locking up the simulated sys clock */
#ifdef CONFIG_ARCH_POSIX
			k_cpu_idle();
#endif
		}

		/* Unreachable, but included for consistency */
		irq_unlock(lock_key);
	} else if (!strcasecmp(argv[1], "null")) {
		volatile uintptr_t null_ptr = 0x0;
		cflush();
		ccprintf("%08x\n", *(volatile unsigned int *)null_ptr);
	} else {
		return EC_ERROR_PARAM1;
	}

	/* Everything crashes, so shouldn't get back here */
	return EC_ERROR_UNKNOWN;
}

DECLARE_CONSOLE_COMMAND(crash, command_crash,
			"[assert | divzero | udivzero | stack"
			" | unaligned | watchdog | hang | null]",
			"Crash the system (for testing)");

#ifdef TEST_BUILD
int test_command_crash(int argc, const char **argv)
{
	return command_crash(argc, argv);
}
#endif /* TEST_BUILD*/
#endif /* CONFIG_CMD_CRASH */

static int command_panicinfo(int argc, const char **argv)
{
	struct panic_data *const pdata_ptr = panic_get_data();

	if (argc == 2) {
		if (!strcasecmp(argv[1], "clear")) {
			memset(get_panic_data_write(), 0,
			       CONFIG_PANIC_DATA_SIZE);
			ccprintf("Panic info cleared\n");
			return EC_SUCCESS;
		} else {
			return EC_ERROR_PARAM1;
		}
	} else if (argc != 1)
		return EC_ERROR_PARAM_COUNT;

	if (pdata_ptr) {
		ccprintf("Saved panic data: 0x%02X %s\n", pdata_ptr->flags,
			 (pdata_ptr->flags & PANIC_DATA_FLAG_OLD_CONSOLE ?
				  "" :
				  "(NEW)"));

		panic_data_print(pdata_ptr);

		/* Data has now been printed */
		pdata_ptr->flags |= PANIC_DATA_FLAG_OLD_CONSOLE;
	} else {
		ccprintf("No saved panic data available "
			 "or panic data can't be safely interpreted.\n");
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(panicinfo, command_panicinfo, "[clear]",
			"Print info from a previous panic");

/*****************************************************************************/
/* Host commands */

static enum ec_status
host_command_panic_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_get_panic_info_v1 *p = args->params;
	uint32_t pdata_size = get_panic_data_size();
	uintptr_t pdata_start = get_panic_data_start();
	struct panic_data *pdata = panic_get_data();

	if (pdata_start && pdata_size > 0) {
		if (pdata_size > args->response_max) {
			panic_printf("Panic data size %d is too "
				     "large, truncating to %d\n",
				     pdata_size, args->response_max);
			pdata_size = args->response_max;
			if (pdata) {
				pdata->flags |= PANIC_DATA_FLAG_TRUNCATED;
			}
		}
		memcpy(args->response, (void *)pdata_start, pdata_size);
		args->response_size = pdata_size;

		if (pdata &&
		    !(args->version > 0 && p->preserve_old_hostcmd_flag)) {
			/* Data has now been returned */
			pdata->flags |= PANIC_DATA_FLAG_OLD_HOSTCMD;
		}
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PANIC_INFO, host_command_panic_info,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
