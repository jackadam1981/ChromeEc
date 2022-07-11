/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#include <drivers/cros_system.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <soc.h>
#include <soc/microchip_xec/reg_def_cros.h>
#include <zephyr/sys/util.h>

#include "system.h"
#include "system_chip.h"
//// mchp
#include <zephyr/drivers/interrupt_controller/intc_mchp_xec_ecia.h>
#include "gpio/gpio_int.h"
//// mchp

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

/* Driver config */
struct cros_system_xec_config {
	/* hardware module base address */
	uintptr_t base_pcr;
	uintptr_t base_vbr;
	uintptr_t base_wdog;
};

/* Driver data */
struct cros_system_xec_data {
	int reset; /* reset cause */
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct cros_system_xec_config *)(dev)->config)
#define DRV_DATA(dev) ((struct cros_system_xec_data *)(dev)->data)

#define HAL_PCR_INST(dev) (struct pcr_regs *)(DRV_CONFIG(dev)->base_pcr)
#define HAL_VBATR_INST(dev) (struct vbatr_regs *)(DRV_CONFIG(dev)->base_vbr)
#define HAL_WDOG_INST(dev) (struct wdt_regs *)(DRV_CONFIG(dev)->base_wdog)

/* Get saved reset flag address in battery-backed ram */
#define BBRAM_SAVED_RESET_FLAG_ADDR                         \
	(DT_REG_ADDR(DT_INST(0, microchip_xec_bbram)) + \
	 DT_PROP(DT_PATH(named_bbram_regions, saved_reset_flags), offset))

/* Soc specific system local functions */
static int system_xec_watchdog_stop(void)
{
	if (IS_ENABLED(CONFIG_WATCHDOG)) {
		const struct device *wdt_dev = DEVICE_DT_GET(
				DT_NODELABEL(wdog));
		if (!device_is_ready(wdt_dev)) {
			LOG_ERR("Error: device %s is not ready", wdt_dev->name);
			return -ENODEV;
		}

		wdt_disable(wdt_dev);
	}

	return 0;
}

static const char *cros_system_xec_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "MCHP";
}

/* TODO - return specific chip name such as MEC1727 or MEC1723 */
static const char *cros_system_xec_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "MEC172X";
}

/* TODO return chip revision from HW as an ASCII string */
static const char *cros_system_xec_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "B0";
}

static int cros_system_xec_get_reset_cause(const struct device *dev)
{
	struct cros_system_xec_data *data = DRV_DATA(dev);

	return data->reset;
}

/* MCHP TODO check and verify this logic for all corner cases:
 * Someone doing ARM Vector Reset insead of SYSRESETREQ or HW reset.
 * Does NRESETIN# status get set also on power on from no power state?
 */
static int cros_system_xec_init(const struct device *dev)
{
	struct vbatr_regs *vbr = HAL_VBATR_INST(dev);
	struct cros_system_xec_data *data = DRV_DATA(dev);
	uint32_t pfsr = vbr->PFRS;

	if (IS_BIT_SET(pfsr, MCHP_VBATR_PFRS_WDT_POS)) {
		data->reset = WATCHDOG_RST;
		vbr->PFRS = BIT(MCHP_VBATR_PFRS_WDT_POS);
	} else if (IS_BIT_SET(pfsr, MCHP_VBATR_PFRS_SYSRESETREQ_POS)) {
		data->reset = DEBUG_RST;
		vbr->PFRS = BIT(MCHP_VBATR_PFRS_SYSRESETREQ_POS);
	} else if (IS_BIT_SET(pfsr, MCHP_VBATR_PFRS_RESETI_POS)) {
		data->reset = VCC1_RST_PIN;
	} else {
		data->reset = POWERUP;
	}

	return 0;
}

noreturn static int cros_system_xec_soc_reset(const struct device *dev)
{
	struct pcr_regs *const pcr = HAL_PCR_INST(dev);

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

//// mchp
	/* Stop the watchdog */
	system_xec_watchdog_stop();
//// mchp
	/* Trigger chip reset */
	pcr->SYS_RST |= MCHP_PCR_SYS_RESET_NOW;
	/* Wait for the soc reset */
	while (1)
		;
	/* should never return */
	/* return 0; */
}

//// mchp
/* Modules Map */
#define ADC_NODE		DT_INST(0, microchip_xec_adc_v2)
#define STRUCT_ADC_REG_BASE_ADDR \
			((struct adc_regs *)(DT_REG_ADDR(ADC_NODE)))

#define UART_NODE		DT_INST(0, microchip_xec_uart)
#define STRUCT_UART_REG_BASE_ADDR \
			((struct uart_regs *)(DT_REG_ADDR(UART_NODE)))

#define ECS_XEC_REG_BASE						\
			((struct ecs_regs *)(DT_REG_ADDR(DT_NODELABEL(ecs))))

#define TIMER_NODE		DT_INST(4, microchip_xec_timer)
#define STRUCT_TIMER4_REG_BASE_ADDR \
			((struct btmr_regs *)(DT_REG_ADDR(TIMER_NODE)))

#define ESPI_NODE		DT_INST(0, microchip_xec_espi_v2)
#define STRUCT_ESPI_REG_BASE_ADDR \
			((struct espi_iom_regs *)(DT_REG_ADDR(ESPI_NODE)))

#define KSCAN_NODE		DT_INST(0, microchip_xec_cros_kb_raw)
#define STRUCT_KBD_REG_BASE_ADDR \
			((struct kscan_regs *)(DT_REG_ADDR(KSCAN_NODE)))

#define QMSPI_NODE		DT_INST(0, microchip_xec_qmspi_v2)
#define STRUCT_QMSPI_REG_BASE_ADDR \
			((struct qmspi_regs *)(DT_REG_ADDR(QMSPI_NODE)))

#define PWM_NODE		DT_INST(0, microchip_xec_pwm)
#define STRUCT_PWM_REG_BASE_ADDR \
			((struct pwm_regs *)(DT_REG_ADDR(PWM_NODE)))

#define TACH_NODE		DT_INST(0, microchip_xec_tach)
#define STRUCT_TACH_REG_BASE_ADDR \
			((struct tach_regs *)(DT_REG_ADDR(TACH_NODE)))

/* Reconfigure GPIOs in hibernate */
static void cros_board_hibernate_late(void)
{
	/* GPIOs reconfiguration */
#if 0
	/* Disable JTAG GPIO */
	*(volatile unsigned long*) 0x40081194 = 0x8040;
	*(volatile unsigned long*) 0x40081198 = 0x8040;
	*(volatile unsigned long*) 0x4008119C = 0x8040;
	*(volatile unsigned long*) 0x400811A0 = 0x8040;
#endif

	/* Disable eSPI */
	/* espi gpio as input, otherwise, block can not enter deep sleep */
	*(volatile unsigned long*) 0x400810D4 = 0x8040;
	*(volatile unsigned long*) 0x400810D8 = 0x8040;
	*(volatile unsigned long*) 0x400810e0 = 0x8040;
	*(volatile unsigned long*) 0x400810e4 = 0x8040;
	*(volatile unsigned long*) 0x400810e8 = 0x8040;
	*(volatile unsigned long*) 0x400810ec = 0x8040;
	*(volatile unsigned long*) 0x400810c4 = 0x8040;
	/* eSPI alert */
	*(volatile unsigned long*) 0x400810cc = 0x8040;

	//#051, 0.5mA, SMC_WAKE_SCI_N_MECC = 1
	*(volatile unsigned long*) 0x400810a4 = 0x8040;
	//#016, 0.5ma, PM_PWRBTN_N = 1
	*(volatile unsigned long*) 0x40081038 = 0x8040;

	// GPIO000 - not used
	*(volatile unsigned long*) 0x40081000 = 0x8040;
	// GPIO161 GPIO162
	*(volatile unsigned long*) 0x400811c4 = 0x8040;
	*(volatile unsigned long*) 0x400811c8 = 0x8040;

	// cfg interrupt GPIOs as input disable
	//#GPIO 0057 @ 0x400810bc:	0x000004f0
	*(volatile unsigned long*) 0x400810bc = 0x8040;
	//#GPIO 0221 @ 0x40081244:	0x000004f0
	*(volatile unsigned long*) 0x40081244 = 0x8040;
	//#GPIO 0243 @ 0x4008128c:	0x000004f0
	*(volatile unsigned long*) 0x4008128c = 0x8040;
	//#GPIO 0201 @ 0x40081204:	0x010004f0
	*(volatile unsigned long*) 0x40081204 = 0x8040;
	//#GPIO 0227 @ 0x4008125c:	0x000004f0
	*(volatile unsigned long*) 0x4008125c = 0x8040;
	//#GPIO 0036 @ 0x40081078:	0x010004f1
	*(volatile unsigned long*) 0x40081078 = 0x8040;
	//#GPIO 0254 @ 0x400812b0:	0x010004f1
	*(volatile unsigned long*) 0x400812b0 = 0x8040;

	//#GPIO 0226 @ 0x40081258:	0x010004f1
	//#GPIO 0115 @ 0x40081134:	0x010004f0
	//#GPIO 0043 @ 0x4008108c:	0x010004f0
	//#GPIO 0156 @ 0x400811b8:	0x010004f0

	//#GPIO 0222 @ 0x40081248:	0x000004f0
	*(volatile unsigned long*) 0x40081248 = 0x8040;
	//#GPIO 0014 @ 0x40081030:	0x000004f0
 	*(volatile unsigned long*) 0x40081030 = 0x8040;
	//#GPIO 0175 @ 0x400811f4:	0x010004f0
 	*(volatile unsigned long*) 0x400811f4 = 0x8040;
	//#GPIO 0105 @ 0x40081114:	0x01001440
 	*(volatile unsigned long*) 0x40081114 = 0x8040;
	//#GPIO 0143 @ 0x4008118c:	0x010004f0
 	*(volatile unsigned long*) 0x4008118c = 0x8040;
	//#GPIO 0240 @ 0x40081280:	0x010004f0
	*(volatile unsigned long*) 0x40081280 = 0x8040;
	//#GPIO 0241 @ 0x40081284:	0x010004f0
	*(volatile unsigned long*) 0x40081284 = 0x8040;
	//#GPIO 0101 @ 0x40081104:	0x000004f0
 	*(volatile unsigned long*) 0x40081104 = 0x8040;

	/* Tune another 3 GPIOs */
	//#GPIO 022 @ 0x40081048
 	*(volatile unsigned long*) 0x40081048 = 0x8040;
	//#GPIO 046 @ 0x40081098
 	*(volatile unsigned long*) 0x40081098 = 0x8040;
	//#GPIO 0170 @ 0x400811E0
 	*(volatile unsigned long*) 0x400811E0 = 0x8040;

	// turn off both leds: GPIO153 GPIO157
	*(volatile unsigned long*) 0x400811ac = 0x10240;
	*(volatile unsigned long*) 0x400811bc = 0x10240;

	// 01 - 8.4
	// 02 - 6.5 --> 5.5
	*(volatile unsigned long*) 0x40081050 = 0x8040;
  	*(volatile unsigned long*) 0x4008106C = 0x8040;
	// 03 - 5.4
 	*(volatile unsigned long*) 0x40081160 = 0x8040;
  	*(volatile unsigned long*) 0x40081164 = 0x8040;
	// 04 - vtr1 0.59 -> 0.29
 	*(volatile unsigned long*) 0x400810DC = 0x8040;
}

/* Configure wakeup GPIOs in hibernate (from hibernate-wake-pins). */
static void system_xec_set_wakeup_gpios_before_hibernate(void)
{
#if DT_NODE_EXISTS(SYSTEM_DT_NODE_HIBERNATE_CONFIG)

/*
 * Get the interrupt DTS node for this wakeup pin
 */
#define WAKEUP_INT(id, prop, idx)  DT_PHANDLE_BY_IDX(id, prop, idx)

/*
 * Get the named-gpio node for this wakeup pin by reading the
 * irq-gpio property from the interrupt node.
 */
#define WAKEUP_NGPIO(id, prop, idx) \
	DT_PHANDLE(WAKEUP_INT(id, prop, idx), irq_pin)

/*
 * Reset and re-enable interrupts on this wake pin.
 */
#define WAKEUP_SETUP(id, prop, idx)		\
do {									       \
	gpio_pin_configure_dt(GPIO_DT_FROM_NODE(WAKEUP_NGPIO(id, prop, idx)),  \
			      GPIO_INPUT);				       \
	gpio_enable_dt_interrupt(					       \
		GPIO_INT_FROM_NODE(WAKEUP_INT(id, prop, idx)));	       \
	} while (0);

/*
 * For all the wake-pins, re-init the GPIO and re-enable the interrupt.
 */
	DT_FOREACH_PROP_ELEM(SYSTEM_DT_NODE_HIBERNATE_CONFIG,
			     wakeup_irqs,
			     WAKEUP_SETUP);

#undef WAKEUP_INT
#undef WAKEUP_NGPIO
#undef WAKEUP_SETUP

#endif
}

/* Put the EC in hibernate (lowest EC power state). */
static int cros_system_xec_hibernate(const struct device *dev,
				     uint32_t seconds, uint32_t microseconds)
{
	struct pcr_regs *const pcr = HAL_PCR_INST(dev);
	struct adc_regs *adc0 = STRUCT_ADC_REG_BASE_ADDR;
	struct uart_regs *uart0 = STRUCT_UART_REG_BASE_ADDR;
	//struct ecs_regs *ecs = ECS_XEC_REG_BASE;
	struct btmr_regs *btmr4 = STRUCT_TIMER4_REG_BASE_ADDR;
	struct espi_iom_regs *espi0 = STRUCT_ESPI_REG_BASE_ADDR;
	struct kscan_regs *kbd = STRUCT_KBD_REG_BASE_ADDR;
	struct qmspi_regs *qmspi0 = STRUCT_QMSPI_REG_BASE_ADDR;
	struct pwm_regs *pwm0 = STRUCT_PWM_REG_BASE_ADDR;
	struct tach_regs *tach0 = STRUCT_TACH_REG_BASE_ADDR;
	struct ecia_regs *ecia = (struct ecia_regs *)(ECIA_BASE_ADDR);
	int i;

#if 1
	printk("hib: MCHP_PCR_CLK_REQ0 = %08X \n", pcr->CLK_REQ[0]);
	printk("hib: MCHP_PCR_CLK_REQ1 = %08X \n", pcr->CLK_REQ[1]);
	printk("hib: MCHP_PCR_CLK_REQ2 = %08X \n", pcr->CLK_REQ[2]);
	printk("hib: MCHP_PCR_CLK_REQ3 = %08X \n", pcr->CLK_REQ[3]);
	printk("hib: MCHP_PCR_CLK_REQ4 = %08X \n", pcr->CLK_REQ[4]);
#endif

	/* Disable interrupt first */
	interrupt_disable_all();
	printk("cros_system_xec_hibernate: dis irq s=%d ms=%d\n", seconds, microseconds);
	/* Stop the watchdog */
	system_xec_watchdog_stop();

	/* Disable all individaul block interrupt and source */
	for (i = 0; i < MCHP_GIRQ_IDX_MAX; ++i) {
		ecia->GIRQ[i].EN_CLR = 0xffffffff;
		ecia->GIRQ[i].SRC = 0xffffffff;
	}

	/* Disable and clear all NVIC interrupt pending */
	for (i = 0; i < MCHP_MAX_NVIC_EXT_INPUTS; ++i) {
		mchp_xec_ecia_nvic_clr_pend(i);
	}

	/* Disable blocks */
#ifdef CONFIG_ADC_XEC_V2	
	/* Disable ADC */
	adc0->CONTROL &= ~(MCHP_ADC_CTRL_ACTV);
#endif
	/* Disable eSPI */
	espi0->ACTV &= ~0x01;
#ifdef CONFIG_CROS_KB_RAW_XEC
	/* Disable Keyboard Scanner */
   	kbd->KSO_SEL &= ~(MCHP_KSCAN_KSO_EN);
#endif
#ifdef CONFIG_I2C
	/* Disable SMB / I2C */
	for (i = 0; i < MCHP_I2C_SMB_INSTANCES; i++) {
		uint32_t addr = MCHP_I2C_SMB_BASE_ADDR(i) +
				MCHP_I2C_SMB_CFG_OFS;
		uint32_t regval = sys_read32(addr);
		sys_write32(regval & ~(MCHP_I2C_SMB_CFG_ENAB), addr);
	}
#endif
	/* Disable QMSPI */
	qmspi0->MODE &= ~MCHP_QMSPI_M_ACTIVATE;
#if defined(CONFIG_PWM_XEC)
	/* Disable PWM0 */
	pwm0->CONFIG &= ~MCHP_PWM_CFG_ENABLE;
#endif
#if defined(CONFIG_TACH_XEC)
	/* Disable TACH0 */
	tach0->CONTROL &= ~MCHP_TACH_CTRL_EN;
#endif
#if defined(CONFIG_TACH_XEC) || defined(CONFIG_PWM_XEC)
	/* This low-speed clock derived from the 48MHz clock domain is used as
	 * a time base for PWMs and TACHs
	 * Set SLOW_CLOCK_DIVIDE = CLKOFF to save additional power
	 */
	pcr->SLOW_CLK_CTRL &= (~MCHP_PCR_SLOW_CLK_CTRL_100KHZ &
				MCHP_PCR_SLOW_CLK_CTRL_MASK);
#endif
	/* Disable timers - 32bit timer 0 */
	btmr4->CTRL &= ~MCHP_BTMR_CTRL_ENABLE;
	/* Reconfigure GPIOs in hibernate */
	cros_board_hibernate_late();

	/* Setup wakeup GPIOs for hibernate */
	system_xec_set_wakeup_gpios_before_hibernate();
	/* Init htimer and enable interupt if times are not 0 */
	if (seconds || microseconds) {
		printk("cros_system_xec_hibernate: init and enable htimer \n");
		//htimer_init();
		//system_set_htimer_alarm(seconds, microseconds);
		//interrupt_enable();
	}

#if 1	/* debugging purpose */
	/* debug wake-up resource */
	for (i = 0; i < MCHP_GIRQ_IDX_MAX; ++i) {	
		printk("hib: 0 GIRQi iii= %08X \n", i);
		printk("hib: 0 GIRQi EN_SET= %08X \n", ecia->GIRQ[i].EN_SET);
	}
	printk("hib: 0 nvic pending 0 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_PEND_BASE)));
	printk("hib: 0 nvic enable  0 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_EN_BASE)));

	/* debug clock required regs */
	printk("hib: 1 MCHP_PCR_CLK_REQ0 = %08X \n", pcr->CLK_REQ[0]);
	printk("hib: 1 MCHP_PCR_CLK_REQ1 = %08X \n", pcr->CLK_REQ[1]);
	printk("hib: 1 MCHP_PCR_CLK_REQ2 = %08X \n", pcr->CLK_REQ[2]);
	printk("hib: 1 MCHP_PCR_CLK_REQ3 = %08X \n", pcr->CLK_REQ[3]);
	printk("hib: 1 MCHP_PCR_CLK_REQ4 = %08X \n", pcr->CLK_REQ[4]);
	printk("hib: 1 enter sleep #### \n");
#endif

#ifdef CONFIG_UART_XEC	
	/* Disable UART0 */
	/* Flush console before hibernating */
	cflush();
	uart0->ACTV &= ~(MCHP_UART_LD_ACTIVATE);
#endif

#if 0
	/* Disable JATG and RTM */
	ecs->DEBUG_CTRL = 0;
	ecs->ETM_CTRL = 0;
#endif

	/* Check all clock required status */
    while(pcr->CLK_REQ[0] != 0);
	/* REQ[1].bit8 = PROCESSOR */
    while(pcr->CLK_REQ[1] != 0x100);
    while(pcr->CLK_REQ[2] != 0);
    while(pcr->CLK_REQ[3] != 0);
    while(pcr->CLK_REQ[4] != 0)
		;

	/*
	 * Set sleep state
	 * arm sleep state to trigger on next WFI
	 */
	pcr->SYS_SLP_CTRL |= MCHP_PCR_SYS_SLP_HEAVY;	

	/* GPIO171 as Test GPIO */
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x8040;

#if 0
while(1)
	;
#endif
	/*
	 * Set PRIMASK = 1 so on wake the CPU will not vector to any ISR.
	 * Set BASEPRI = 0 to allow any priority to wake.
	 */
	__set_BASEPRI(0);
	/* triggers sleep hardware */
	__WFI();
	__NOP();
	__NOP();

	/* Wake up by resource */
#if 1	/* debugging purpose */
	/* Enable UART0 after wakeup */
	uart0->ACTV |= MCHP_UART_LD_ACTIVATE;
	printk("hib: woken up by source!!!!\n");

	/* Trace out block source */
	for (i = 0; i < MCHP_GIRQ_IDX_MAX; ++i) {	
		printk("hib: 2 GIRQi iii= %08X \n", i);
		printk("hib: 2 GIRQi SRC= %08X \n", ecia->GIRQ[i].SRC);
	}
	/* Trace out NVIC source */
	printk("DT: nvic pending 0 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_PEND_BASE)));
	printk("DT: nvic enable  0 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_EN_BASE)));
	printk("DT: nvic pending 1 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_PEND_BASE + 0x04)));
	printk("DT: nvic enable  1 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_EN_BASE + 0x04)));
	printk("DT: nvic pending 2 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_PEND_BASE + 0x08)));
	printk("DT: nvic enable  2 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_EN_BASE + 0x08)));
	printk("DT: nvic pending 3 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_PEND_BASE + 0x0C)));
	printk("DT: nvic enable  3 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_EN_BASE + 0x0C)));
	printk("DT: nvic pending 4 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_PEND_BASE + 0x10)));
	printk("DT: nvic enable  4 = %08X \n", (*(volatile uint32_t*)(MCHP_NVIC_SET_EN_BASE + 0x10)));

	/* GPIO171 as Test GPIO */
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x8040;
#endif
	/* Reset EC chip */
	cros_system_xec_soc_reset(dev);

	/* Should not reach here... */
	printk("cros_system_xec_hibernate: ### fail to enter hibernate mode \n");
	return 0;
}
//// mchp

static struct cros_system_xec_data cros_system_xec_dev_data;

static const struct cros_system_xec_config cros_system_dev_cfg = {
	.base_pcr = DT_REG_ADDR_BY_NAME(DT_INST(0, microchip_xec_pcr), pcrr),
	.base_vbr = DT_REG_ADDR_BY_NAME(DT_INST(0, microchip_xec_pcr), vbatr),
	.base_wdog = DT_REG_ADDR(DT_INST(0, microchip_xec_watchdog)),
};

static const struct cros_system_driver_api cros_system_driver_xec_api = {
	.get_reset_cause = cros_system_xec_get_reset_cause,
	.soc_reset = cros_system_xec_soc_reset,
	.hibernate = cros_system_xec_hibernate,
	.chip_vendor = cros_system_xec_get_chip_vendor,
	.chip_name = cros_system_xec_get_chip_name,
	.chip_revision = cros_system_xec_get_chip_revision,
};

DEVICE_DEFINE(cros_system_xec_0, "CROS_SYSTEM", cros_system_xec_init, NULL,
	      &cros_system_xec_dev_data, &cros_system_dev_cfg, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_XEC_INIT_PRIORITY,
	      &cros_system_driver_xec_api);
