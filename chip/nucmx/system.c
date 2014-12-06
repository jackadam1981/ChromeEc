/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System module for Chrome EC : NUCMX hardware specific implementation */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "host_command.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "hwtimer_chip.h"

/* Indices for battery-backed ram (BBRAM) data position */
enum bbram_data_index {
	BBRM_DATA_INDEX_SCRATCHPAD 			= 0,        /* General-purpose scratchpad */
	BBRM_DATA_INDEX_SAVED_RESET_FLAGS  	= 4,		/* Saved reset flags */
	BBRM_DATA_INDEX_WAKE				= 8,    	/* Wake reasons for hibernate */
};

/* Flags for BBRM_DATA_INDEX_WAKE */
#define PSLDATA_WAKE_MTC        (1 << 0)  /* LCT alarm */
#define PSLDATA_WAKE_PIN        (1 << 1)  /* Wake pin */

/* Super-IO index and register definitions */
#define SIO_OFFSET				0x4E
#define INDEX_SID 				0x20
#define INDEX_CHPREV 			0x24
#define INDEX_SRID 				0x27

/* equivalent to 250us according to 48MHz core clock */
#define MTC_TTC_LOAD_DELAY  	1500
#define MTC_ALARM_MASK      	((1 << 25) - 1)
#define MTC_WUI_GROUP			MIWU_GROUP_4
#define MTC_WUI_MASK			MASK_PIN7

uint32_t flag_hibernate;
/*****************************************************************************/
/* Internal functions */

/* Super-IO read/write function */
void system_sib_write_reg(uint8_t io_offset, uint8_t index_value, uint8_t io_data)
{
	/* Disable interrupts */
	interrupt_disable();

	/* Lock host CFG module */
    SET_BIT(NUCMX_LKSIOHA, NUCMX_LKSIOHA_LKCFG);
    /* Enable Core-to-Host Modules Access */
    SET_BIT(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSAE);
    /* Enable Core access to CFG module */
    SET_BIT(NUCMX_CRSMAE, NUCMX_CRSMAE_CFGAE);
    /* Verify Core read/write to host modules is not in progress */
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSRD)) ;
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSWR)) ;


    /* Specify the io_offset A0 = 0. the index register is accessed */
    NUCMX_IHIOA = io_offset;
    /* Write the data. This starts the write access to the host module */
    NUCMX_IHD = index_value;
    /* Wait while Core write operation is in progress */
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSWR)) ;

    /* Specify the io_offset A0 = 1. the data register is accessed */
    NUCMX_IHIOA = io_offset+1;
     /* Write the data. This starts the write access to the host module */
    NUCMX_IHD = io_data;
	/* Wait while Core write operation is in progress */
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSWR)) ;


    /* Disable Core access to CFG module */
    CLEAR_BIT(NUCMX_CRSMAE, NUCMX_CRSMAE_CFGAE);
    /* Disable Core-to-Host Modules Access */
    CLEAR_BIT(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSAE);
    /* unlock host CFG  module */
    CLEAR_BIT(NUCMX_LKSIOHA, NUCMX_LKSIOHA_LKCFG);

    /* Enable interrupts */
	interrupt_enable();
}

uint8_t system_sib_read_reg(uint8_t io_offset, uint8_t index_value)
{
	uint8_t data_value;

	/* Disable interrupts */
	interrupt_disable();

	/* Lock host CFG module */
    SET_BIT(NUCMX_LKSIOHA, NUCMX_LKSIOHA_LKCFG);
    /* Enable Core-to-Host Modules Access */
    SET_BIT(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSAE);
    /* Enable Core access to CFG module */
    SET_BIT(NUCMX_CRSMAE, NUCMX_CRSMAE_CFGAE);
    /* Verify Core read/write to host modules is not in progress */
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSRD)) ;
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSWR)) ;


    /* Specify the io_offset A0 = 0. the index register is accessed */
    NUCMX_IHIOA = io_offset;
    /* Write the data. This starts the write access to the host module */
    NUCMX_IHD = index_value;
    /* Wait while Core write operation is in progress */
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSWR)) ;

    /* Specify the io_offset A0 = 1. the data register is accessed */
    NUCMX_IHIOA = io_offset+1;
    /* Start a Core read from host module */
    SET_BIT(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSRD);
    /* Wait while Core read operation is in progress */
    while (IS_BIT_SET(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSRD)) ;
    /* Read the data */
    data_value = NUCMX_IHD;


    /* Disable Core access to CFG module */
    CLEAR_BIT(NUCMX_CRSMAE, NUCMX_CRSMAE_CFGAE);
    /* Disable Core-to-Host Modules Access */
    CLEAR_BIT(NUCMX_SIBCTRL, NUCMX_SIBCTRL_CSAE);
    /* unlock host CFG  module */
    CLEAR_BIT(NUCMX_LKSIOHA, NUCMX_LKSIOHA_LKCFG);

    /* Enable interrupts */
	interrupt_enable();

    return data_value;
}

void system_watchdog_reset(void)
{
	/* Unlock & stop watchdog registers */
	NUCMX_WDSDM = 0x87;
	NUCMX_WDSDM = 0x61;
	NUCMX_WDSDM = 0x63;

	/* Reset TWCFG */
	NUCMX_TWCFG = 0;
	/* Select T0IN clock as watchdog prescaler clock */
	SET_BIT(NUCMX_TWCFG, NUCMX_TWCFG_WDCT0I);

	/* Clear watchdog reset status initially*/
	SET_BIT(NUCMX_T0CSR, NUCMX_T0CSR_WDRST_STS);

	/* Keep prescaler ratio timer0 clock to 1:1 */
	NUCMX_TWCP = 0x00;

	/* Set internal counter and prescaler */
	NUCMX_TWDT0 = 0x00;
	NUCMX_WDCNT = 0x01;

	/* Disable interrupt */
	interrupt_disable();
	/* Reload and restart Timer 0*/
	SET_BIT(NUCMX_T0CSR, NUCMX_T0CSR_RST);
	/* Wait for timer is loaded and restart */
	while(IS_BIT_SET(NUCMX_T0CSR, NUCMX_T0CSR_RST));
	/* Enable interrupt */
	interrupt_enable();
}

/**
 * Read battery-backed ram (BBRAM) at specified index.
 *
 * @return The value of the register or 0 if invalid index.
 */
static uint32_t bbram_data_read(enum bbram_data_index index)
{
	uint32_t value = 0;
	/* Check index */
	if (index < 0 || index >= NUCMX_BBRAM_SIZE)
		return 0;

	/* BBRAM is valid */
	if(IS_BIT_SET(NUCMX_BKUP_STS, NUCMX_BKUP_STS_IBBR))
		return 0;

	/* Read BBRAM */
	value += NUCMX_BBRAM(index+3);
	value = value << 8;
	value += NUCMX_BBRAM(index+2);
	value = value << 8;
	value += NUCMX_BBRAM(index+1);
	value = value << 8;
	value += NUCMX_BBRAM(index);

	return value;
}

/**
 * Write battery-backed ram (BBRAM) at specified index.
 *
 * @return nonzero if error.
 */
static int bbram_data_write(enum bbram_data_index index, uint32_t value)
{
	/* Check index */
	if (index < 0 || index >= NUCMX_BBRAM_SIZE)
		return EC_ERROR_INVAL;

	/* BBRAM is valid */
	if(IS_BIT_SET(NUCMX_BKUP_STS, NUCMX_BKUP_STS_IBBR))
		return EC_ERROR_INVAL;

	/* Write BBRAM */
	NUCMX_BBRAM(index) 	 = value & 0xFF;
	NUCMX_BBRAM(index+1) = (value >> 8)  & 0xFF;
	NUCMX_BBRAM(index+2) = (value >> 16) & 0xFF;
	NUCMX_BBRAM(index+3) = (value >> 24) & 0xFF;

	/* Wait for write-complete */
	return EC_SUCCESS;
}

/* MTC functions */
uint32_t system_get_rtc_sec(void)
{
	/* Get MTC counter unit:seconds */
	uint32_t sec = NUCMX_TTC;
	return sec;
}

void system_set_rtc(uint32_t seconds)
{
	volatile uint16_t __i;

    /* Set MTC counter unit:seconds */
    NUCMX_TTC = seconds;

    /* Wait till clock is readable                                                                         */
    for (__i=0; __i< MTC_TTC_LOAD_DELAY; ++__i) {}
}

/* Check reset cuse */
static void check_reset_cause(void)
{
	uint32_t hib_wake_flags = bbram_data_read(BBRM_DATA_INDEX_WAKE);
	uint32_t flags = 0;

	/* Check for VCC1 reset */
	if (IS_BIT_SET(NUCMX_RSTCTL, NUCMX_RSTCTL_VCC1_RST_STS)){
		flags |= RESET_FLAG_POWER_ON;
	}

	/* Software debugger reset */
	if (IS_BIT_SET(NUCMX_RSTCTL, NUCMX_RSTCTL_DBGRST_STS)){
		flags |= RESET_FLAG_SOFT;
	}

	/* Watchdog Reset */
	if (IS_BIT_SET(NUCMX_T0CSR,NUCMX_T0CSR_WDRST_STS)) {
		flags |= RESET_FLAG_WATCHDOG;
		/* Clear watchdog reset status initially*/
		SET_BIT(NUCMX_T0CSR, NUCMX_T0CSR_WDRST_STS);
	}

	if ((hib_wake_flags & PSLDATA_WAKE_PIN))
		flags |= RESET_FLAG_WAKE_PIN;

	/* Restore then clear saved reset flags */
	flags |= bbram_data_read(BBRM_DATA_INDEX_SAVED_RESET_FLAGS);
	bbram_data_write(BBRM_DATA_INDEX_SAVED_RESET_FLAGS, 0);

	system_set_reset_flags(flags);
}

/**
 * Internal hibernate function.
 *
 * @param seconds      Number of seconds to sleep before LCT alarm
 * @param microseconds Number of microseconds to sleep before LCT alarm
 */
void __enter_hibernate(uint32_t seconds, uint32_t microseconds)
{
	/* Set instant wake up mode */
	SET_BIT(NUCMX_ENIDL_CTL, NUCMX_ENIDL_CTL_LP_WK_CTL);

	interrupt_disable();

	/* ITIM event module disable */
	CLEAR_BIT(NUCMX_ITCTS(ITIM_EVENT_NO), NUCMX_ITIM16_ITEN);
	/* ITIM time module disable */
	CLEAR_BIT(NUCMX_ITCTS(ITIM_TIME_NO), NUCMX_ITIM16_ITEN);
	/* ITIM watchdog warn module disable */
	CLEAR_BIT(NUCMX_ITCTS(ITIM_WDG_NO), NUCMX_ITIM16_ITEN);

	/*
	 * Set RTC interrupt in time to wake up before
	 * next event.
	 */
	if(seconds || microseconds){
		system_set_rtc_alarm(seconds, microseconds);
	}

	/* Unlock & stop watchdog registers */
	NUCMX_WDSDM = 0x87;
	NUCMX_WDSDM = 0x61;
	NUCMX_WDSDM = 0x63;


	while(1){
		/* Set deep idle - instant wake-up mode*/
		NUCMX_PMCSR = 0x7;
		/* Enter deep idle, wake-up by GPIOxxx or RTC */
		asm("wfi");

		/*TODO: Is POWER_BUTTON_L GPIO02 to wake-up? */
		if(IS_BIT_SET(NUCMX_WKPND(MIWU_TABLE_1 , MIWU_GROUP_1),2)){
			panic_puts("### Reset from GPIO ###\n");
			break;
		}
		/* RTC wake-up */
		else if(IS_BIT_SET(NUCMX_WTC, NUCMX_WTC_PTO)){
			panic_puts("### Reset from RTC ###\n");
			break;
		}
	}

	/* Clear WUI pending bit of MTC */
	NUCMX_WKPCL(MIWU_TABLE_0, MTC_WUI_GROUP) = MTC_WUI_MASK;

	/* Reset MTC */
	system_reset_rtc_alarm();

	panic_puts("### Start reset from hibernate ###\n");

	/* Start a watchdog reset */
	system_watchdog_reset();

}

/*****************************************************************************/
/* IC specific low-level driver */

/* Microsecond will be ignore for hardware limitation */
void system_set_rtc_alarm(uint32_t seconds, uint32_t microseconds)
{
	uint32_t curSecs, alarmSecs;

	if(seconds == 0)
		return;

    /* Get current clock */
    curSecs = NUCMX_TTC;

    /* If alarm clock is not sequential or not in range */
    alarmSecs = curSecs + seconds;
    alarmSecs = alarmSecs & MTC_ALARM_MASK;

    /* Reset alarm first */
    system_reset_rtc_alarm();

    /* Set alarm, use first 25 bits of clock value */
    NUCMX_WTC = alarmSecs;

    /* Enable interrupt mode alarm */
    SET_BIT(NUCMX_WTC, NUCMX_WTC_WIE);

    /* Enable MTC interrupt */
	task_enable_irq(NUCMX_IRQ_MTC_WKINTAD_0);

	/* Enable wake-up input sources */
	NUCMX_WKEN(MIWU_TABLE_0, MTC_WUI_GROUP) |= MTC_WUI_MASK;
}

void system_reset_rtc_alarm(void)
{
	/*
     * Clear interrupt & Disable alarm interrupt
     * Update alarm value to zero
     */
    CLEAR_BIT(NUCMX_WTC, NUCMX_WTC_WIE);
    SET_BIT(NUCMX_WTC, NUCMX_WTC_PTO);

    /* Disable MTC interrupt */
	task_disable_irq(NUCMX_IRQ_MTC_WKINTAD_0);
}

/**
 * Enable hibernate interrupt
 */
void system_enable_hib_interrupt(void)
{
	task_enable_irq(NUCMX_IRQ_MTC_WKINTAD_0);
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
	/* Flush console before hibernating */
	cflush();

#if SUPPORT_HIB
	/* Add additional hibernate operations here */
	__enter_hibernate(seconds, microseconds);
#endif
}

void system_pre_init(void)
{
	/*
	 * Add additional initialization here
	 * EC should be initialized in Booter
	 */

#if 0
	/* Power-down the modules we don't need */
	NUCMX_PWDWN_CTL(0) = 0xFD; /* Skip SDP_PD */
	NUCMX_PWDWN_CTL(1) = 0xFF;
	NUCMX_PWDWN_CTL(2) = 0xFF;
	NUCMX_PWDWN_CTL(3) = 0xF0;	/*Skip ITIM3/2/1_PD */
	NUCMX_PWDWN_CTL(4) = 0xF8;
	NUCMX_PWDWN_CTL(5) = 0x87;
#endif

	/* Check reset cause */
	check_reset_cause();
}

void system_reset(int flags)
{
	uint32_t save_flags = 0;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable();

	/* Save current reset reasons if necessary */
	if (flags & SYSTEM_RESET_PRESERVE_FLAGS)
		save_flags = system_get_reset_flags() | RESET_FLAG_PRESERVED;

	/* Add in AP off flag into saved flags. */
	if (flags & SYSTEM_RESET_LEAVE_AP_OFF)
		save_flags |= RESET_FLAG_AP_OFF;

	/* Store flags to battery backed RAM. */
	bbram_data_write(BBRM_DATA_INDEX_SAVED_RESET_FLAGS, save_flags);

	if (flags & SYSTEM_RESET_HARD) {
		/* Ask the watchdog to trigger a hard reboot */
		system_watchdog_reset();
		/* wait for the watchdog */
		while (1);
	} else{
		/* Use AIRCR of system control block to generate soft reset */
		CPU_NVIC_APINT = 0x05fa0004;
	}
	/* Spin and wait for reboot; should never return */
	while (1);
}

/**
 * Return the chip vendor/name/revision string.
 */
const char *system_get_chip_vendor(void)
{
	uint8_t fam_id = system_sib_read_reg(SIO_OFFSET, INDEX_SID);
	switch (fam_id) {
	case 0xFC:
		return "NUC";
	default:
		return "Unknown";
	}
}

const char *system_get_chip_name(void)
{
	uint8_t chip_id = system_sib_read_reg(SIO_OFFSET, INDEX_SRID);
	switch (chip_id) {
	case 0x05:
		return "NPCX5m5G";
	default:
		return "Unknown";
	}
}

const char *system_get_chip_revision(void)
{
	static char rev[1];
	uint8_t rev_num = system_sib_read_reg(SIO_OFFSET, INDEX_CHPREV);

	/* set revision from character '0' */
	rev[0] = '0' + rev_num;

	return rev;
}

int system_set_console_force_enabled(int val)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

int system_get_console_force_enabled(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

/**
 * Get/Set VbNvContext in non-volatile storage.  The block should be 16 bytes
 * long, which is the current size of VbNvContext block.
 *
 * @param block		Pointer to a buffer holding VbNvContext.
 * @return 0 on success, !0 on error.
 */
int system_get_vbnvcontext(uint8_t *block)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_vbnvcontext(const uint8_t *block)
{
	return EC_ERROR_UNIMPLEMENTED;
}

/**
 * Set a scratchpad register to the specified value.
 *
 * The scratchpad register must maintain its contents across a
 * software-requested warm reset.
 *
 * @param value		Value to store.
 * @return EC_SUCCESS, or non-zero if error.
 */
int system_set_scratchpad(uint32_t value)
{
	return bbram_data_write(BBRM_DATA_INDEX_SCRATCHPAD, value);
}

uint32_t system_get_scratchpad(void)
{
	return bbram_data_read(BBRM_DATA_INDEX_SCRATCHPAD);
}

/*****************************************************************************/
/* Console commands */

static int command_system_rtc(int argc, char **argv)
{
	uint32_t sec;
	if (argc == 3 && !strcasecmp(argv[1], "set")) {
		char *e;
		uint32_t t = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		system_set_rtc(t);
	} else if (argc > 1) {
		return EC_ERROR_INVAL;
	}

	sec = system_get_rtc_sec();
	ccprintf("RTC: 0x%08x (%d.00 s)\n", sec, sec);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc, command_system_rtc,
			"[set <seconds>]",
			"Get/set real-time clock",
			NULL);

#ifdef CONFIG_CMD_RTC_ALARM
/**
 * Test the RTC alarm by setting an interrupt on RTC match.
 */
static int command_rtc_alarm_test(int argc, char **argv)
{
	int s = 1, us = 0;
	char *e;

	ccprintf("Setting RTC alarm\n");
	system_enable_hib_interrupt();

	if (argc > 1) {
		s = strtoi(argv[1], &e, 10);
		if (*e)
			return EC_ERROR_PARAM1;

	}
	if (argc > 2) {
		us = strtoi(argv[2], &e, 10);
		if (*e)
			return EC_ERROR_PARAM2;

	}

	system_set_rtc_alarm(s, us);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rtc_alarm, command_rtc_alarm_test,
			"[seconds [microseconds]]",
			"Test alarm",
			NULL);
#endif /* CONFIG_CMD_RTC_ALARM */

/*****************************************************************************/
/* Host commands */

static int system_rtc_get_value(struct host_cmd_handler_args *args)
{
	struct ec_response_rtc *r = args->response;

	r->time = system_get_rtc_sec();
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RTC_GET_VALUE,
		     system_rtc_get_value,
		     EC_VER_MASK(0));

static int system_rtc_set_value(struct host_cmd_handler_args *args)
{
	const struct ec_params_rtc *p = args->params;

	system_set_rtc(p->time);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_RTC_SET_VALUE,
		     system_rtc_set_value,
		     EC_VER_MASK(0));

/* For LPC host register initial via SIB module */
void system_lpc_host_register_init(void){
	//Setting PMC2
	system_sib_write_reg(SIO_OFFSET,0x07,0x12);	//LDN register = 0x12(PMC2)
	system_sib_write_reg(SIO_OFFSET,0x60,0x02);	//CMD =0x200
	system_sib_write_reg(SIO_OFFSET,0x61,0x00);	
	system_sib_write_reg(SIO_OFFSET,0x62,0x02);	//Data=0x204
	system_sib_write_reg(SIO_OFFSET,0x63,0x04);
	system_sib_write_reg(SIO_OFFSET,0x30,0x01);	//enable PMC2

	//Setting SHM
	system_sib_write_reg(SIO_OFFSET,0x07,0x0F);	//LDN register = 0x0F(SHM)
//	system_sib_write_reg(SIO_OFFSET,0xF0,system_sib_read_reg(SIO_OFFSET,0xF0)&0xFB);
	system_sib_write_reg(SIO_OFFSET,0xF1,system_sib_read_reg(SIO_OFFSET,0xF1)|0x30); //WIN1&2 mapping to IO
	system_sib_write_reg(SIO_OFFSET,0xF7,0x00);	//Host Command on the IO:0x0800
	system_sib_write_reg(SIO_OFFSET,0xF6,0x00);
	system_sib_write_reg(SIO_OFFSET,0xF5,0x08);
	system_sib_write_reg(SIO_OFFSET,0xF4,0x00);
	system_sib_write_reg(SIO_OFFSET,0xFB,0x00);	//WIN1 as Host Command on the IO:0x0800
	system_sib_write_reg(SIO_OFFSET,0xFA,0x00);
	system_sib_write_reg(SIO_OFFSET,0xF9,0x09);	//WIN2 as MEMMAP on the IO:0x900
	system_sib_write_reg(SIO_OFFSET,0xF8,0x00);
	system_sib_write_reg(SIO_OFFSET,0x30,0x01);	//enable SHM
}
