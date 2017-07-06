/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPC module for MEC17XX */

#include "common.h"
#include "acpi.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_protocol.h"
#include "lpc.h"
#include "lpc_chip.h"
#include "espi.h"
#include "port80.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "chipset.h"


/* Console output macros */

#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)

#define LPC_SYSJUMP_TAG 0x4c50  /* "LP" */

static uint8_t mem_mapped[0x200] __attribute__((section(".bss.big_align")));

static uint32_t host_events;     /* Currently pending SCI/SMI events */
static uint32_t event_mask[3];   /* Event masks for each type */
static struct host_packet lpc_packet;
static struct host_cmd_handler_args host_cmd_args;
static uint8_t host_cmd_flags;   /* Flags from host command */

static uint8_t params_copy[EC_LPC_HOST_PACKET_SIZE] __aligned(4);
static int init_done;

static struct ec_lpc_host_args * const lpc_host_args =
	(struct ec_lpc_host_args *)mem_mapped;

#ifdef CONFIG_BOARD_SKL_RVP3
static uint8_t custom_acpi_cmd;
static uint8_t custom_acpi_ec2os_cnt;
static uint8_t custom_apci_ec2os[4];
#endif


static void keyboard_irq_assert(void)
{
#ifdef CONFIG_KEYBOARD_IRQ_GPIO
	/*
	 * Enforce signal-high for long enough for the signal to be pulled high
	 * by the external pullup resistor.  This ensures the host will see the
	 * following falling edge, regardless of the line state before this
	 * function call.
	 */
	gpio_set_level(CONFIG_KEYBOARD_IRQ_GPIO, 1);
	udelay(4);
	/* Generate a falling edge */
	gpio_set_level(CONFIG_KEYBOARD_IRQ_GPIO, 0);
	udelay(4);

	/* Set signal high, now that we've generated the edge */
	gpio_set_level(CONFIG_KEYBOARD_IRQ_GPIO, 1);
#else
	/*
	 * SERIRQ is automatically sent by KBC
	 */
#endif
}

/**
 * Generate SMI pulse to the host chipset via GPIO.
 *
 * If the x86 is in S0, SMI# is sampled at 33MHz, so minimum pulse length is
 * 60ns.  If the x86 is in S3, SMI# is sampled at 32.768KHz, so we need pulse
 * length >61us.  Both are short enough and events are infrequent, so just
 * delay for 65us.
 */
static void lpc_generate_smi(void)
{
	/* CPRINTS("LPC Pulse SMI"); */
	trace0(0, LPC, 0, "LPC Pulse SMI");
#ifdef CONFIG_ESPI
	/* eSPI: pulse SMI# Virtual Wire */
	espi_vw_pulse_wire(VW_SMI_L);
#else
	gpio_set_level(GPIO_PCH_SMI_L, 0);
	udelay(65);
	gpio_set_level(GPIO_PCH_SMI_L, 1);
#endif
}

static void lpc_generate_sci(void)
{
	/* CPRINTS("LPC Pulse SCI"); */
	trace0(0, LPC, 0, "LPC Pulse SCI");
#ifdef CONFIG_SCI_GPIO
	gpio_set_level(CONFIG_SCI_GPIO, 0);
	udelay(65);
	gpio_set_level(CONFIG_SCI_GPIO, 1);
#else
#ifdef CONFIG_ESPI
	espi_vw_pulse_wire(VW_SCI_L);
#else
	MEC17XX_ACPI_PM_STS |= 1;
	udelay(65);
	MEC17XX_ACPI_PM_STS &= ~1;
#endif
#endif
}

/**
 * Update the level-sensitive wake signal to the AP.
 *
 * @param wake_events	Currently asserted wake events
 */
static void lpc_update_wake(uint32_t wake_events)
{
	/*
	 * Mask off power button event, since the AP gets that through a
	 * separate dedicated GPIO.
	 */
	wake_events &= ~EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON);

#ifdef CONFIG_ESPI
	espi_vw_set_wire(VW_WAKE_L, !wake_events);
#else
	/* Signal is asserted low when wake events is non-zero */
	gpio_set_level(GPIO_PCH_WAKE_L, !wake_events);
#endif
}

static uint8_t *lpc_get_hostcmd_data_range(void)
{
	return mem_mapped;
}


/**
 * Update the host event status.
 *
 * Sends a pulse if masked event status becomes non-zero:
 *   - SMI pulse via PCH_SMI_L GPIO
 *   - SCI pulse via PCH_SCI_L GPIO
 */
static void update_host_event_status(void)
{
	int need_sci = 0;
	int need_smi = 0;

	CPRINTS("LPC update_host_event_status");
	trace0(0, LPC, 0, "LPC update_host_event_status");

	if (!init_done)
		return;

	/* Disable LPC interrupt while updating status register */
	task_disable_irq(MEC17XX_IRQ_ACPIEC0_IBF);

	if (host_events & event_mask[LPC_HOST_EVENT_SMI]) {
		/* Only generate SMI for first event */
		if (!(MEC17XX_ACPI_EC_STATUS(0) & EC_LPC_STATUS_SMI_PENDING))
			need_smi = 1;
		MEC17XX_ACPI_EC_STATUS(0) |= EC_LPC_STATUS_SMI_PENDING;
	} else {
		MEC17XX_ACPI_EC_STATUS(0) &= ~EC_LPC_STATUS_SMI_PENDING;
	}

	if (host_events & event_mask[LPC_HOST_EVENT_SCI]) {
		/* Generate SCI for every event */
		need_sci = 1;
		MEC17XX_ACPI_EC_STATUS(0) |= EC_LPC_STATUS_SCI_PENDING;
	} else {
		MEC17XX_ACPI_EC_STATUS(0) &= ~EC_LPC_STATUS_SCI_PENDING;
	}

	/* Copy host events to mapped memory */
	*(uint32_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) = host_events;

	task_enable_irq(MEC17XX_IRQ_ACPIEC0_IBF);

	/* Process the wake events. */
	lpc_update_wake(host_events & event_mask[LPC_HOST_EVENT_WAKE]);

	/* Send pulse on SMI signal if needed */
	if (need_smi)
		lpc_generate_smi();

	/* ACPI 5.0-12.6.1: Generate SCI for SCI_EVT=1. */
	if (need_sci)
		lpc_generate_sci();
}

#ifdef CONFIG_ESPI
/*
 * Need public wrapper
 */
void lpc_update_host_event_status(void)
{
	update_host_event_status();
}
#endif

static void lpc_send_response_packet(struct host_packet *pkt)
{
	int i;
	uint8_t *p8;

	/* Ignore in-progress on LPC since interface is synchronous anyway */
	if (pkt->driver_result == EC_RES_IN_PROGRESS) {
		/* CPRINTS("LPC EC_RES_IN_PROGRESS"); */
		return;
	}

	/* CPRINTS("LPC Set EC2OS(1,0)=0x%02x",pkt->driver_result); */
	trace1(0, LPC, 0, "LPC Set EC2OS(1,0)=0x%02x", pkt->driver_result);

	p8 = (uint8_t *)pkt->response;
	if (p8 != NULL) {
		for (i = 0; i < pkt->response_size; i++)
			trace2(0, LPC, 0, "response[%d] = 0x%02x", i, *p8++);
	}

	/* Write result to the data byte. */
	MEC17XX_ACPI_EC_EC2OS(1, 0) = pkt->driver_result;

	/* Clear the busy bit, so the host knows the EC is done. */
	MEC17XX_ACPI_EC_STATUS(1) &= ~EC_LPC_STATUS_PROCESSING;
}

/**
 * Preserve event masks across a sysjump.
 */
static void lpc_sysjump(void)
{
	CPRINTS("LPC HOOK_SYSJUMP");
	trace0(0, LPC, 0, "LPC HOOK_SYSJUMP");
	system_add_jump_tag(LPC_SYSJUMP_TAG, 1,
				sizeof(event_mask), event_mask);
}
DECLARE_HOOK(HOOK_SYSJUMP, lpc_sysjump, HOOK_PRIO_DEFAULT);

/**
 * Restore event masks after a sysjump.
 */
static void lpc_post_sysjump(void)
{
	const uint32_t *prev_mask;
	int size, version;

	prev_mask = (const uint32_t *)system_get_jump_tag(LPC_SYSJUMP_TAG,
							  &version, &size);
	if (!prev_mask || version != 1 || size != sizeof(event_mask))
		return;

	memcpy(event_mask, prev_mask, sizeof(event_mask));
}

uint8_t *lpc_get_memmap_range(void)
{
	return mem_mapped + 0x100;
}

uint32_t lpc_mem_mapped_addr(void)
{
	return (uint32_t)mem_mapped;
}

void lpc_mem_mapped_init(void)
{
	/* We support LPC args and version 3 protocol */
	*(lpc_get_memmap_range() + EC_MEMMAP_HOST_CMD_FLAGS) =
		EC_HOST_CMD_FLAG_LPC_ARGS_SUPPORTED |
		EC_HOST_CMD_FLAG_VERSION_3;
}


/*
 * Most registers in LPC module are reset when the host is off. We need to
 * set up LPC again when the host is starting up.
 * MEC17xx does not appear to connect LRESET# to an interrupt!
 * MEC17xx LRESET# can be one of two pins
 *	GPIO_0052 Func 2
 *	GPIO_0064 Func 1
 * Use GPIO interrupt to detect LRESET# changes.
 * Use GPIO_0064 for LRESET#. Must update board/board_name/gpio.inc
 *
 * For eSPI PLATFORM_RESET# virtual wire is used as LRESET#
 *
 */
#ifndef CONFIG_ESPI
static void setup_lpc(void)
{
	gpio_config_module(MODULE_LPC, 1);

	/*
	 * MEC17xx LRESET# interrupt is GPIO interrupt
	 * and configured by GPIO table in board level gpio.inc
	 * Refer to lpcrst_interrupt() in this file.
	 */

	/* Set up ACPI0 for 0x62/0x66 */
	MEC17XX_LPC_ACPI_EC0_BAR = 0x00628304;

	/* Clear STATUS_PROCESSING bit in case it was set during sysjump */
	MEC17XX_ACPI_EC_STATUS(0) &= ~EC_LPC_STATUS_PROCESSING;
	MEC17XX_INT_ENABLE(MEC17XX_ACPI_EC_GIRQ) =
			MEC17XX_ACPI_EC_IBF_GIRQ_BIT(0);
	task_enable_irq(MEC17XX_IRQ_ACPIEC0_IBF);

	/* Set up ACPI1 for 0x200/0x204 */
	MEC17XX_LPC_ACPI_EC1_BAR = 0x02008407;

	MEC17XX_ACPI_EC_STATUS(1) &= ~EC_LPC_STATUS_PROCESSING;
	MEC17XX_INT_ENABLE(MEC17XX_ACPI_EC_GIRQ) =
			MEC17XX_ACPI_EC_IBF_GIRQ_BIT(1);
	task_enable_irq(MEC17XX_IRQ_ACPIEC1_IBF);

	/* Set up 8042 interface at 0x60/0x64 */
	MEC17XX_LPC_8042_BAR = 0x00608104;

	/* Set up indication of Auxiliary sts */
	MEC17XX_8042_KB_CTRL |= 1 << 7;

	MEC17XX_8042_ACT |= 1;

	MEC17XX_INT_ENABLE(MEC17XX_8042_GIRQ) = MEC17XX_8042_OBE_GIRQ_BIT +
			MEC17XX_8042_IBF_GIRQ_BIT;

	task_enable_irq(MEC17XX_IRQ_8042EM_IBF);
	task_enable_irq(MEC17XX_IRQ_8042EM_OBF);

#ifndef CONFIG_KEYBOARD_IRQ_GPIO
	/* Set up SERIRQ for keyboard */
	MEC17XX_8042_KB_CTRL |= (1 << 5);
	MEC17XX_LPC_SIRQ(1) = 0x01;
#endif

	/* Set up EMI module for memory mapped region, base address 0x800 */
	MEC17XX_LPC_EMI0_BAR = 0x0800800f;

	MEC17XX_INT_ENABLE(MEC17XX_EMI_GIRQ) = MEC17XX_EMI_GIRQ_BIT(0);
	task_enable_irq(MEC17XX_IRQ_EMI0);

	/*
	 * Access data RAM
	 * MEC17xx EMI Base address register = physical address of buffer
	 * in SRAM. EMI hardware adds 16-bit offset Host programs into
	 * EC_Address_LSB/MSB registers.
	 */
	MEC17XX_EMI_MBA0(0) = (uint32_t)mem_mapped;

	/*
	 * Limit EMI read / write range. First 256 bytes are RW for host
	 * commands. Second 256 bytes are RO for mem-mapped data.
	 */
	MEC17XX_EMI_MRL0(0) = 0x200;
	MEC17XX_EMI_MWL0(0) = 0x100;

	/* Setup Port80 Debug Hardware ports.
	 * First instance for I/O 80h only.
	 * Set FIFO interrupt threshold to maximum of 14 bytes.
	 */
	MEC17XX_P80_CFG(0) = MEC17XX_P80_FLUSH_FIFO_WO +
			MEC17XX_P80_RESET_TIMESTAMP_WO;

	MEC17XX_LPC_P80DBG0_BAR = (0x80ul << 16) + 0x01ul;

	MEC17XX_P80_CFG(0) = MEC17XX_P80_FIFO_THRHOLD_14 +
			MEC17XX_P80_TIMEBASE_1500KHZ +
			MEC17XX_P80_TIMER_ENABLE;

	task_enable_irq(MEC17XX_IRQ_PORT80DBG0);

	lpc_mem_mapped_init();

	/* Sufficiently initialized */
	init_done = 1;

	/* Update host events now that we can copy them to memmap */
	update_host_event_status();
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, setup_lpc, HOOK_PRIO_FIRST);
#endif

static void lpc_resume(void)
{
	CPRINTS("LPC HOOK_CHIPSET_RESUME");
	trace0(0, HOOK, 0, "HOOK_CHIPSET_RESUME - lpc_resume");
#ifdef CONFIG_POWER_S0IX
	if (chipset_in_state(CHIPSET_STATE_SUSPEND | CHIPSET_STATE_ON))
#endif
	{
		/* Mask all host events until the host unmasks them itself.  */
		lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, 0);
		lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, 0);
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, 0);
	}
	/* Store port 80 event so we know where resume happened */
	port_80_write(PORT_80_EVENT_RESUME);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, lpc_resume, HOOK_PRIO_DEFAULT);



static void lpc_init(void)
{
	CPRINTS("LPC HOOK_INIT");
	trace0(0, HOOK, 0, "HOOK_INIT - lpc_init");

	/* Initialize host args and memory map to all zero */
	memset(lpc_host_args, 0, sizeof(*lpc_host_args));
	memset(lpc_get_memmap_range(), 0, EC_MEMMAP_SIZE);

#ifdef CONFIG_ESPI

	espi_init();

#else
	/* Activate LPC interface */
	MEC17XX_LPC_ACT |= 1;

	/*
	 * Ring Oscillator not permitted to shut down
	 * until LPC activate bit is cleared
	 */
	MEC17XX_LPC_CLK_CTRL |= 3;

	setup_lpc();
#endif

	/* Restore event masks if needed */
	lpc_post_sysjump();

}
/*
 * Set prio to higher than default; this way LPC memory mapped data is ready
 * before other inits try to initialize their memmap data.
 */
DECLARE_HOOK(HOOK_INIT, lpc_init, HOOK_PRIO_INIT_LPC);

#ifdef CONFIG_CHIPSET_RESET_HOOK
static void lpc_chipset_reset(void)
{
	hook_notify(HOOK_CHIPSET_RESET);
}
DECLARE_DEFERRED(lpc_chipset_reset);
#endif



/*
 * Called from power/skylake.c chipset_reset()
 * For LPC it doesn't call here, instead it pulses RCIN# low for 10 us
 */
void lpc_host_reset(void)
{
	/* Host Reset Control will assert KBRST# (LPC) or RCIN# VW (eSPI) */
#ifdef CONFIG_ESPI_VW_SIGNALS
	espi_vw_pulse_wire(VW_RCIN_L);
#else
	#error "Function called only for eSPI!"
#endif
}


void lpc_set_init_done(int val)
{
	init_done = val;
}


/*
 * MEC17xx LRESET# is function 2 of GPIO_0052 or function 1 of GPIO_0064
 * GPIO_0052 interrupt is GIRQ10 bit[10]
 * GPIO_0064 interrupt is GIRQ10 bit[20]
 * LRESET# can be monitored as bit[1](read-only) of LPC Bus Monitor Register.
 * bit[1]==0 -> LRESET# is high
 * bit[1]==1 -> LRESET# is low (active)
 * LRESET# active causes MEC17xx to activate internal signal RESET_HOST.
 * MEC17XX_PCR_PWR_RST_STS bit[3](read-only) = RESET_HOST_STATUS =
 * 0 = Reset active
 * 1 = Reset not active
 * MEC17xx is different than MEC1322 in that LRESET# is not hooked to a separate
 * interrupt source such as GIRQ19 bit[1].
 * MEC17xx GIRQ19 is dedicated to eSPI.
 * If using LPC mode the board design must use either GPIO_0064 or GPIO_0052 and
 * board level gpio.inc must install a handler for the chosen GPIO.
 * Example entry to be added to board level gpio.inc using GPIO_0064
 * GPIO_INT(PCH_PLTRST_L, PIN(064), GPIO_INT_BOTH, lpcrst_interrupt)
 */
void lpcrst_interrupt(enum gpio_signal signal)
{
#ifndef CONFIG_ESPI
	/* Initialize LPC module when LRESET# is deasserted */
	if (!lpc_get_pltrst_asserted()) {
		setup_lpc();
	} else {
		/* Store port 80 reset event */
		port_80_write(PORT_80_EVENT_RESET);

#ifdef CONFIG_CHIPSET_RESET_HOOK
		/* Notify HOOK_CHIPSET_RESET */
		hook_call_deferred(lpc_chipset_reset, MSEC);
#endif
	}

	CPRINTS("LPC RESET# %sasserted",
		lpc_get_pltrst_asserted() ? "" : "de");
#endif
}


void emi0_interrupt(void)
{
	uint8_t h2e;

	h2e = MEC17XX_EMI_H2E_MBX(0);
	CPRINTS("LPC Host 0x%02x -> EMI0 H2E(0)", h2e);
	trace1(0, LPC, 0, "EMI0 H2E = 0x%02x", h2e);
	port_80_write(h2e);
}
DECLARE_IRQ(MEC17XX_IRQ_EMI0, emi0_interrupt, 1);

/*
 * ISR empties BIOS Debug 0 FIFO and
 * writes data to circular buffer. How can we be
 * sure this routine can read the last Port 80h byte?
 */
int port_80_read(void)
{
	int data;

	data = PORT_80_IGNORE;
	if (MEC17XX_P80_STS(0) & MEC17XX_P80_STS_NOT_EMPTY)
		data = MEC17XX_P80_CAP(0) & 0xFF;

	return data;
}

#ifdef CONFIG_BOARD_SKL_RVP3
/*
 * Handle custom ACPI EC0 commands.
 * Some chipset's CoreBoot will send read board ID command expecting
 * a two byte response.
 */
static int acpi_ec0_custom(int is_cmd, uint8_t value, uint8_t *resultptr)
{
	int rval;

	rval = 0;
	custom_acpi_ec2os_cnt = 0;
	*resultptr = 0x00;

	if (is_cmd && (value == 0x0d)) {
		MEC17XX_INT_SOURCE(MEC17XX_ACPI_EC_GIRQ) =
				MEC17XX_ACPI_EC_OBE_GIRQ_BIT(0);
		/* Write two bytes sequence 0xC2, 0x04 to Host */
		if (MEC17XX_ACPI_EC_BYTE_CTL(0) & 0x01) {
			trace0(0, LPC, 0,
			     "AEC0 ISR: Cmd 0x0d 4-byte mode. Result=0x04c2");
			/* Host enabled 4-byte mode */
			MEC17XX_ACPI_EC_EC2OS(0, 0) = 0x02; /* was 0xc2 */
			MEC17XX_ACPI_EC_EC2OS(0, 1) = 0x04;
			MEC17XX_ACPI_EC_EC2OS(0, 2) = 0x00;
			MEC17XX_ACPI_EC_EC2OS(0, 3) = 0x00; /* OBF is set */
		} else {
			trace0(0, LPC, 0,
			  "AEC0 ISR: Cmd 0x0d 1-byte mode. Result=0xc2,0x04");
			/* single byte mode */
			*resultptr = 0x02; /* was 0xc2 */
			custom_acpi_ec2os_cnt = 1;
			custom_apci_ec2os[0] = 0x04;
			MEC17XX_ACPI_EC_EC2OS(0, 0) = 0x02; /* was 0xc2 */
			MEC17XX_INT_ENABLE(MEC17XX_ACPI_EC_GIRQ) =
					MEC17XX_ACPI_EC_OBE_GIRQ_BIT(0);
			task_enable_irq(MEC17XX_IRQ_ACPIEC0_OBE);
		}
		custom_acpi_cmd = 0;
		rval = 1;
	}

	return rval;
}
#endif

void acpi_0_interrupt(void)
{
	uint8_t value, result, is_cmd;

	is_cmd = MEC17XX_ACPI_EC_STATUS(0);

	/* Set the bust bi */
	MEC17XX_ACPI_EC_STATUS(0) |= EC_LPC_STATUS_PROCESSING;

	result = MEC17XX_ACPI_EC_BYTE_CTL(0);

	/* Read command/data; this clears the FRMH bit. */
	value = MEC17XX_ACPI_EC_OS2EC(0, 0);

	/* CPRINTS("ACPI EC0 ISR sts=0x%02x OS2EC=0x%02x ",is_cmd,value); */
	trace3(0, LPC, 0,
	  "AEC0 ISR: sts=0x%02x O2SEC=0x%02x byte_ctrl=0x%02x",
	       is_cmd, value, result);

	is_cmd &= EC_LPC_STATUS_LAST_CMD;

	/* Handle whatever this was. */
	result = 0;
	if (acpi_ap_to_ec(is_cmd, value, &result))
		MEC17XX_ACPI_EC_EC2OS(0, 0) = result;
#ifdef CONFIG_BOARD_SKL_RVP3
	else
		acpi_ec0_custom(is_cmd, value, &result);
#endif
	/* Clear the busy bit */
	MEC17XX_ACPI_EC_STATUS(0) &= ~EC_LPC_STATUS_PROCESSING;

	/* Clear R/W1C status bit in Aggregator */
	MEC17XX_INT_SOURCE(MEC17XX_ACPI_EC_GIRQ) =
			MEC17XX_ACPI_EC_IBF_GIRQ_BIT(0);

	/*
	 * ACPI 5.0-12.6.1: Generate SCI for Input Buffer Empty / Output Buffer
	 * Full condition on the kernel channel.
	 */
	lpc_generate_sci();
}
DECLARE_IRQ(MEC17XX_IRQ_ACPIEC0_IBF, acpi_0_interrupt, 1);

#ifdef CONFIG_BOARD_SKL_RVP3
/*
 * ACPI EC0 output buffer empty ISR.
 * Used to handle custom ACPI EC0 command requiring
 * two byte response.
 */
void acpi_0_obe_isr(void)
{
	uint8_t sts, data;

	MEC17XX_INT_SOURCE(MEC17XX_ACPI_EC_GIRQ) =
			MEC17XX_ACPI_EC_OBE_GIRQ_BIT(0);

	sts = MEC17XX_ACPI_EC_STATUS(0);
	data = MEC17XX_ACPI_EC_BYTE_CTL(0);
	trace3(0, LPC, 0,
	       "AEC0 OBE ISR: sts=0x%02x bytectrl=0x%02x ec2os_cnt=0x%02x",
	       sts, data, custom_acpi_ec2os_cnt);

	data = sts;
	if (custom_acpi_ec2os_cnt) {
		custom_acpi_ec2os_cnt--;
		data = custom_apci_ec2os[custom_acpi_ec2os_cnt];
		trace1(0, LPC, 0,
		       "AEC0 OBE ISR: write EC2OS(0,0)=0x%02x", data);
		MEC17XX_ACPI_EC_EC2OS(0, 0) = data;
	}

	if (custom_acpi_ec2os_cnt == 0) { /* was last byte? */
		MEC17XX_INT_DISABLE(MEC17XX_ACPI_EC_GIRQ) =
				MEC17XX_ACPI_EC_OBE_GIRQ_BIT(0);
	}

	lpc_generate_sci();
}
DECLARE_IRQ(MEC17XX_IRQ_ACPIEC0_OBE, acpi_0_obe_isr, 1);
#endif

void acpi_1_interrupt(void)
{
	const struct ec_host_request *r;

	uint8_t st = MEC17XX_ACPI_EC_STATUS(1);

	CPRINTS("ACPI EC1 ISR: sts=0x%02x", st);
	trace1(0, LPC, 0, "ACPI EC1 ISR: sts=0x%02x", st);

	if (!(st & EC_LPC_STATUS_FROM_HOST) ||
	    !(st & EC_LPC_STATUS_LAST_CMD))
		return;

	/* Set the busy bit */
	MEC17XX_ACPI_EC_STATUS(1) |= EC_LPC_STATUS_PROCESSING;

	/*
	 * Read the command byte.  This clears the FRMH bit in
	 * the status byte.
	 */
	host_cmd_args.command = MEC17XX_ACPI_EC_OS2EC(1, 0);
	CPRINTS("ACPI EC1 ISR: OS2EC0=0x%02x", host_cmd_args.command);
	trace1(0, LPC, 0,
	       "ACPI EC1 ISR OS2EC[0]=0x%02x", host_cmd_args.command);

	host_cmd_args.result = EC_RES_SUCCESS;
	host_cmd_flags = lpc_host_args->flags;

	/* We only support new style command (v3) now */
	if (host_cmd_args.command == EC_COMMAND_PROTOCOL_3) {
		lpc_packet.send_response = lpc_send_response_packet;

		lpc_packet.request = (const void *)lpc_get_hostcmd_data_range();
		lpc_packet.request_temp = params_copy;
		lpc_packet.request_max = sizeof(params_copy);
		/* Don't know the request size so pass in the entire buffer */
		lpc_packet.request_size = EC_LPC_HOST_PACKET_SIZE;

		lpc_packet.response = (void *)lpc_get_hostcmd_data_range();
		lpc_packet.response_max = EC_LPC_HOST_PACKET_SIZE;
		lpc_packet.response_size = 0;

		lpc_packet.driver_result = EC_RES_SUCCESS;

		r = lpc_packet.request;
		CPRINTS("ACPI EC1 Packet Command=0x%04x", r->command);
		trace1(0, LPC, 0,
		       "ACPI EC1 ISR: Packet Cmd = 0x%04x", r->command);

		host_packet_receive(&lpc_packet);

	} else {
		CPRINTS("ACPI EC1 ISR: Invalid protocol");
		trace0(0, LPC, 0, "ACPI EC1 ISR: Invalid protocol");
		/* Old style command unsupported */
		host_cmd_args.result = EC_RES_INVALID_COMMAND;

		/*
		 * Hand off to host command handler
		 * TODO - BUG!!! Observed usage fault when this
		 * routine is called.
		 */
		host_command_received(&host_cmd_args);


		MEC17XX_ACPI_EC_STATUS(1) &= ~EC_LPC_STATUS_PROCESSING;
		MEC17XX_INT_SOURCE(MEC17XX_ACPI_EC_GIRQ) =
				MEC17XX_ACPI_EC_IBF_GIRQ_BIT(1);
	}

	CPRINTS("ACPI EC1 ISR: Exit");
	trace0(0, LPC, 0, "ACPI EC1 ISR: Exit");
}
DECLARE_IRQ(MEC17XX_IRQ_ACPIEC1_IBF, acpi_1_interrupt, 1);

#ifdef HAS_TASK_KEYPROTO
void kb_ibf_interrupt(void)
{
	if (lpc_keyboard_input_pending())
		keyboard_host_write(MEC17XX_8042_H2E,
				    MEC17XX_8042_STS & (1 << 3));
	task_wake(TASK_ID_KEYPROTO);
}
DECLARE_IRQ(MEC17XX_IRQ_8042EM_IBF, kb_ibf_interrupt, 1);

void kb_obf_interrupt(void)
{
	task_wake(TASK_ID_KEYPROTO);
}
DECLARE_IRQ(MEC17XX_IRQ_8042EM_OBF, kb_obf_interrupt, 1);
#endif

int lpc_keyboard_has_char(void)
{
	return (MEC17XX_8042_STS & (1 << 0)) ? 1 : 0;
}

int lpc_keyboard_input_pending(void)
{
	return (MEC17XX_8042_STS & (1 << 1)) ? 1 : 0;
}

/*
 * called from common/keyboard_8042.c
 */
void lpc_keyboard_put_char(uint8_t chr, int send_irq)
{
	trace2(0, KBD, 0, "chr=0x%02x send_irq=%d", chr, send_irq);
	MEC17XX_8042_E2H = chr;
	if (send_irq)
		keyboard_irq_assert();
}

/*
 * Read 8042 register and write to read-only register
 * insuring compiler does not optimize out the read.
 */
void lpc_keyboard_clear_buffer(void)
{
	MEC17XX_PCR_CHIP_OSC_ID = MEC17XX_8042_OBF_CLR;
}

void lpc_keyboard_resume_irq(void)
{
	if (lpc_keyboard_has_char())
		keyboard_irq_assert();
}

void lpc_set_host_event_state(uint32_t mask)
{
	if (mask != host_events) {
		host_events = mask;
		update_host_event_status();
	}
}

int lpc_query_host_event_state(void)
{
	const uint32_t any_mask = event_mask[0] | event_mask[1] | event_mask[2];
	int evt_index = 0;
	int i;

	for (i = 0; i < 32; i++) {
		const uint32_t e = (1 << i);

		if (host_events & e) {
			host_clear_events(e);

			/*
			 * If host hasn't unmasked this event, drop it.  We do
			 * this at query time rather than event generation time
			 * so that the host has a chance to unmask events
			 * before they're dropped by a query.
			 */
			if (!(e & any_mask))
				continue;

			evt_index = i + 1;	/* Events are 1-based */
			break;
		}
	}

	return evt_index;
}

void lpc_set_host_event_mask(enum lpc_host_event_type type, uint32_t mask)
{
	event_mask[type] = mask;
	update_host_event_status();
}

uint32_t lpc_get_host_event_mask(enum lpc_host_event_type type)
{
	return event_mask[type];
}

void lpc_set_acpi_status_mask(uint8_t mask)
{
	MEC17XX_ACPI_EC_STATUS(0) |= mask;
}

void lpc_clear_acpi_status_mask(uint8_t mask)
{
	MEC17XX_ACPI_EC_STATUS(0) &= ~mask;
}

int lpc_get_pltrst_asserted(void)
{
#ifdef CONFIG_ESPI
	/*
	 * eSPI PLTRST# a VWire or side-band signal
	 * Controlled by CONFIG_ESPI_PLTRST_IS_VWIRE
	 */
	return espi_vw_get_wire(VW_PLTRST_L);
#else
	/* returns 1 if LRESET# pin is asserted(low) else 0 */
	return (MEC17XX_LPC_BUS_MONITOR & (1<<1)) ? 1 : 0;
#endif
}

/* Enable LPC ACPI-EC0 interrupts */
void lpc_enable_acpi_interrupts(void)
{
	task_enable_irq(MEC17XX_IRQ_ACPIEC0_IBF);
}

/* Disable LPC ACPI-EC0 interrupts */
void lpc_disable_acpi_interrupts(void)
{
	task_disable_irq(MEC17XX_IRQ_ACPIEC0_IBF);
}

/* On boards without a host, this command is used to set up LPC */
static int lpc_command_init(int argc, char **argv)
{
	lpc_init();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(lpcinit, lpc_command_init, NULL, NULL);

/* Get protocol information */
static int lpc_get_protocol_info(struct host_cmd_handler_args *args)
{
	struct ec_response_get_protocol_info *r = args->response;

	CPRINTS("MEC1701 Handler EC_CMD_GET_PROTOCOL_INFO");
	trace0(0, LPC, 0, "Handler EC_CMD_GET_PROTOCOL_INFO");

	memset(r, 0, sizeof(*r));
	r->protocol_versions = (1 << 3);
	r->max_request_packet_size = EC_LPC_HOST_PACKET_SIZE;
	r->max_response_packet_size = EC_LPC_HOST_PACKET_SIZE;
	r->flags = 0;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO,
		lpc_get_protocol_info,
		EC_VER_MASK(0));
