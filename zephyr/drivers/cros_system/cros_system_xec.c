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
/* Macros to access registers */
#define REG32_ADDR(addr) ((volatile uint32_t *)(addr))
#define REG16_ADDR(addr) ((volatile uint16_t *)(addr))
#define REG8_ADDR(addr)  ((volatile uint8_t  *)(addr))

#define REG32(addr) (*REG32_ADDR(addr))
#define REG16(addr) (*REG16_ADDR(addr))
#define REG8(addr)  (*REG8_ADDR(addr))

///#define MCHP_PCR_BASE		0x40080100
///#define MCHP_PCR_CLK_REQ0		REG32(MCHP_PCR_BASE + 0x50)
///#define MCHP_PCR_CLK_REQ1		REG32(MCHP_PCR_BASE + 0x54)
///#define MCHP_PCR_CLK_REQ2		REG32(MCHP_PCR_BASE + 0x58)
///#define MCHP_PCR_CLK_REQ3		REG32(MCHP_PCR_BASE + 0x5C)
///#define MCHP_PCR_CLK_REQ4		REG32(MCHP_PCR_BASE + 0x60)

/* ADC */
///#define MCHP_ADC_BASE		0x40007c00
///#define MCHP_ADC_CTRL		REG32(MCHP_ADC_BASE + 0x0)

/* Basic timers */
///#define MCHP_TMR32_0_BASE	0x40000c80
///#define MCHP_TMR_SPACING	0x20
///#define MCHP_TMR32_BASE(n)	(MCHP_TMR32_0_BASE + (n) * MCHP_TMR_SPACING)
///#define MCHP_TMR32_CTL(x)	REG32(MCHP_TMR32_BASE(x) + 0x10)

/* eSPI */
///#define MCHP_ESPI_IO_BASE	0x400f3400
///#define MCHP_ESPI_ACTIVATE		REG8(MCHP_ESPI_IO_BASE + 0x330)

/* UART */
///#define MCHP_UART0_BASE		0x400f2400
//#define MCHP_UART_SPACING	0x400
///#define MCHP_UART_CFG_OFS	0x300
///#define MCHP_UART_CONFIG_BASE(x) (MCHP_UART0_BASE + MCHP_UART_CFG_OFS + ((x) * MCHP_UART_SPACING))
///#define MCHP_UART_ACT(x)	REG8(MCHP_UART_CONFIG_BASE(x) + 0x30)

/* Macro to access 32-bit registers */
#define CPUREG(addr) (*(volatile uint32_t*)(addr))
/* Nested Vectored Interrupt Controller */
#define CPU_NVIC_EN(x)         CPUREG(0xe000e100 + 4 * (x))
#define CPU_NVIC_DIS(x)        CPUREG(0xe000e180 + 4 * (x))
#define CPU_NVIC_PEND(x)       CPUREG(0xe000e200 + 4 * (x))

#define CPU_NVIC_UNPEND(x)     CPUREG(0xe000e280 + 4 * (x))
#define CPU_NVIC_PRI(x)        CPUREG(0xe000e400 + 4 * (x))

#define MCHP_IRQ_MAX 180

/* Power/Clocks/Resets */
///#define MCHP_PCR_SYS_SLP_HEAVY		(BIT(3) | BIT(0))
///#define MCHP_PCR_SYS_SLP_ALL		(1ul << 3)
///#define MCHP_PCR_BASE				0x40080100
///#define MCHP_PCR_SYS_SLP_CTL		REG32(MCHP_PCR_BASE + 0x00)

/* EC Interrupt aggregator (ECIA) */
#define MCHP_INT_BASE		0x4000e000
/* EC Interrupt aggregator (ECIA) */
#define MCHP_INT_GIRQ_LEN	20 /* 5 32-bit registers */
#define MCHP_INT_GIRQ_FIRST	8
#define MCHP_INT_GIRQ_LAST	26
#define MCHP_INT_GIRQ_NUM	(26-8+1)
/* MCHP_INT_GIRQ_FIRST <= x <= MCHP_INT_GIRQ_LAST */
#define MCHP_INTx_BASE(x) (MCHP_INT_BASE + (((x) - 8) * MCHP_INT_GIRQ_LEN))
#define MCHP_INT_SOURCE(x)		REG32(MCHP_INTx_BASE(x) + 0x0)
#define MCHP_INT_ENABLE(x)		REG32(MCHP_INTx_BASE(x) + 0x4)
#define MCHP_INT_RESULT(x)		REG32(MCHP_INTx_BASE(x) + 0x8)
#define MCHP_INT_DISABLE(x)		REG32(MCHP_INTx_BASE(x) + 0xc)

/* Quad Master SPI (QMSPI) */
///#define MCHP_QMSPI0_BASE	0x40070000
///#define MCHP_QMSPI0_MODE		REG32(MCHP_QMSPI0_BASE + 0x00)
///#define MCHP_QMSPI0_MODE_ACT_SRST	REG8(MCHP_QMSPI0_BASE + 0x00)
///#define MCHP_QMSPI0_MODE_SPI_MODE	REG8(MCHP_QMSPI0_BASE + 0x01)

/* Bits in MCHP_QMSPI0_MODE */
///#define MCHP_QMSPI_M_ACTIVATE		BIT(0)
///#define MCHP_QMSPI_M_SOFT_RESET		BIT(1)

///#define MCHP_EC_BASE		0x4000fc00
///#define MCHP_EC_JTAG_EN		REG32(MCHP_EC_BASE + 0x20)

/// mchp2_z
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

#define QMSPI_NODE		DT_INST(0, microchip_xec_qmspi_v2)
#define STRUCT_QMSPI_REG_BASE_ADDR \
			((struct qmspi_regs *)(DT_REG_ADDR(QMSPI_NODE)))

#define PWM_NODE		DT_INST(0, microchip_xec_pwm)
#define STRUCT_PWM_REG_BASE_ADDR \
			((struct pwm_regs *)(DT_REG_ADDR(PWM_NODE)))

#define TACH_NODE		DT_INST(0, microchip_xec_tach)
#define STRUCT_TACH_REG_BASE_ADDR \
			((struct tach_regs *)(DT_REG_ADDR(TACH_NODE)))

/// mchp2_z

/////////////////////////////////////////////////////////////////////////
/*  */
static int cros_system_xec_hibernate(const struct device *dev,
				     uint32_t seconds, uint32_t microseconds)
{
///mchp2_z
	struct pcr_regs *const pcr = HAL_PCR_INST(dev);
	struct adc_regs *adc0 = STRUCT_ADC_REG_BASE_ADDR;
	struct uart_regs *uart0 = STRUCT_UART_REG_BASE_ADDR;
//	struct ecs_regs *ecs = ECS_XEC_REG_BASE;
	struct btmr_regs *btmr4 = STRUCT_TIMER4_REG_BASE_ADDR;
	struct espi_iom_regs *espi0 = STRUCT_ESPI_REG_BASE_ADDR;
	struct qmspi_regs *qmspi0 = STRUCT_QMSPI_REG_BASE_ADDR;
	struct pwm_regs *pwm0 = STRUCT_PWM_REG_BASE_ADDR;
	struct tach_regs *tach0 = STRUCT_TACH_REG_BASE_ADDR;
	
///mchp2_z
	int i;

#if 1
	///printk("hib: MCHP_PCR_CLK_REQ0 = %08X \n", MCHP_PCR_CLK_REQ0);
	///printk("hib: MCHP_PCR_CLK_REQ1 = %08X \n", MCHP_PCR_CLK_REQ1);
	///printk("hib: MCHP_PCR_CLK_REQ2 = %08X \n", MCHP_PCR_CLK_REQ2);
	///printk("hib: MCHP_PCR_CLK_REQ3 = %08X \n", MCHP_PCR_CLK_REQ3);
	///printk("hib: MCHP_PCR_CLK_REQ4 = %08X \n", MCHP_PCR_CLK_REQ4);
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

	/* Enter hibernate mode */
	/* 1: disable all individaul block interrupt and source */
	for (i = MCHP_INT_GIRQ_FIRST; i <= MCHP_INT_GIRQ_LAST; ++i) {
		MCHP_INT_DISABLE(i) = 0xffffffff;
		MCHP_INT_SOURCE(i)  = 0xffffffff;
	}

	/* 2: clear all NVIC interrupt pending */
	for (i = 0; i < MCHP_IRQ_MAX; ++i) {
		irq_disable(i);
		mchp_xec_ecia_nvic_clr_pend(i);
	}

	/* 3: disable JATG and RTM */
	/* TODO */
	/* Disable JTAG */
#if 0
	///MCHP_EC_JTAG_EN &= ~1;
ecs->DEBUG_CTRL = 0;
ecs->ETM_CTRL = 0;
	/* jtag GPIO */
	*(volatile unsigned long*) 0x40081194 = 0x8040;
	*(volatile unsigned long*) 0x40081198 = 0x8040;
	*(volatile unsigned long*) 0x4008119C = 0x8040;
	*(volatile unsigned long*) 0x400811A0 = 0x8040;
#endif

	/* 4: disable blocks */
#ifdef CONFIG_ADC_XEC_V2	
	/* 4.1: disable ADC */
	///MCHP_ADC_CTRL &= ~1;
adc0->CONTROL &= ~(MCHP_ADC_CTRL_ACTV);
#endif
	/* 4.2: disable eSPI */
	/* espi gpio as input, otherwise, block can not enter deep sleep */
	*(volatile unsigned long*) 0x400810D4 = 0x8040;
	*(volatile unsigned long*) 0x400810D8 = 0x8040;
	*(volatile unsigned long*) 0x400810e0 = 0x8040;
	*(volatile unsigned long*) 0x400810e4 = 0x8040;
	*(volatile unsigned long*) 0x400810e8 = 0x8040;
	*(volatile unsigned long*) 0x400810ec = 0x8040;
	*(volatile unsigned long*) 0x400810c4 = 0x8040;
	/* alert */
	*(volatile unsigned long*) 0x400810cc = 0x8040;
	///MCHP_ESPI_ACTIVATE &= ~0x01;
espi0->ACTV &= ~0x01;
//printk("hib: ttt espi0->ACTV = %08X \n", espi0->ACTV);
//printk("hib: ttt add 0x400F3730 espi0->ACTV = %08X \n", (unsigned int)&(espi0->ACTV));

	/* 4.2: disable SMB / I2C */
	/* disable I2C blocks */
	///*(volatile unsigned long*) 0x40004028 |= BIT(9);
	///*(volatile unsigned long*) 0x40004428 |= BIT(9);
	///*(volatile unsigned long*) 0x40004828 |= BIT(9);
	///*(volatile unsigned long*) 0x40004C28 |= BIT(9);
	///*(volatile unsigned long*) 0x40005028 |= BIT(9);
	///*(volatile unsigned long*) 0x40004028 &= ~BIT(9);
	///*(volatile unsigned long*) 0x40004428 &= ~BIT(9);
	///*(volatile unsigned long*) 0x40004828 &= ~BIT(9);
	///*(volatile unsigned long*) 0x40004C28 &= ~BIT(9);
	///*(volatile unsigned long*) 0x40005028 &= ~BIT(9);

#ifdef CONFIG_I2C
	for (i = 0; i < MCHP_I2C_SMB_INSTANCES; i++) {
		uint32_t addr = MCHP_I2C_SMB_BASE_ADDR(i) +
				MCHP_I2C_SMB_CFG_OFS;
		uint32_t regval = sys_read32(addr);
		sys_write32(regval & ~(MCHP_I2C_SMB_CFG_ENAB), addr);
	}
#endif

// copy from legacy ec
	/* disable DMA */
	///*(volatile unsigned long *)0x40002400 = 0;
//	dma_disable_all();
	/* dis qmspi */
	///MCHP_QMSPI0_MODE_ACT_SRST = MCHP_QMSPI_M_SOFT_RESET;
//qmspi0->MODE = MCHP_QMSPI_M_SRST;
qmspi0->MODE &= ~MCHP_QMSPI_M_ACTIVATE;
	//unused = MCHP_QMSPI0_MODE_ACT_SRST;
	///MCHP_QMSPI0_MODE_ACT_SRST = 0;
//qmspi0->MODE = 0;
//	MCHP_PCR_SLP_EN_DEV(MCHP_PCR_QMSPI);
	/* dis etm */
	///*(volatile unsigned long *)0x4000FC1C = 0;

/* Zephyr - disable local DMA */
/* *(volatile unsigned long *)0x40070000 = 0;
*(volatile unsigned long *)0x40070004 = 0;
*(volatile unsigned long *)0x40070010 = 0xffffffff; */

	/* 4.4: disable PWM / TACH */
	///mchp2_z
#if defined(CONFIG_PWM_XEC)
	/* disable PWM0 */
	///*(volatile unsigned long*) 0x40005808 &= ~BIT(0);
pwm0->CONFIG &= ~MCHP_PWM_CFG_ENABLE;
#endif
#if defined(CONFIG_TACH_XEC)
	/* disable TACH0 */
	///*(volatile unsigned long*) 0x40006000 &= ~BIT(1);
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


	/* GPIOs */
	//#051, 0.5mA, SMC_WAKE_SCI_N_MECC = 1
    //*(unsigned long *)0x400810a4 = $gpioval 
	*(volatile unsigned long*) 0x400810a4 = 0x8040;
	//#016, 0.5ma, PM_PWRBTN_N = 1
    //set *(unsigned long *)0x40081038 = $gpioval    
	*(volatile unsigned long*) 0x40081038 = 0x8040;

	// GPIO000 - not used???
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

#if 1
// Zephyr test

// 01 - 8.4

// 02 - 6.5 --> 5.5
	*(volatile unsigned long*) 0x40081050 = 0x8040;
  	*(volatile unsigned long*) 0x4008106C = 0x8040;

// 03 - 5.4
 	*(volatile unsigned long*) 0x40081160 = 0x8040;
  	*(volatile unsigned long*) 0x40081164 = 0x8040;

// 04 - vtr1 0.59 -> 0.29
 	*(volatile unsigned long*) 0x400810DC = 0x8040;

// 05 - mchp2_z
	/* disable PWM0 amd TACH0 GPIOs */
	// Todo;
#endif

// end of copy from legacy ec

	/* 5: disable timers - 32bit timer 0 */
	///MCHP_TMR32_CTL(0) &= ~1;
btmr4->CTRL &= ~MCHP_BTMR_CTRL_ENABLE;
	/* 6: setup GPIOs for hibernate */
	/* 7: enable wakeup pins */
	/* enable power button irq - gpio GPIO115 (GIRQ9.13bit) */
	MCHP_INT_ENABLE(9) = BIT(13);
	irq_enable(1);
	/* LID irq - gpio GPIO226 (GIRQ12.22bit) */
	MCHP_INT_ENABLE(12) = BIT(22);
	irq_enable(4);
	/* AC ok irq - gpio GPIO156 (GIRQ8.14bit) */
	MCHP_INT_ENABLE(8) = BIT(14);
	irq_enable(0);

	/* 8: init htimer and enable interupt if times are not 0 */
	if (seconds || microseconds) {
		printk("cros_system_xec_hibernate: init and enable htimer \n");
		//htimer_init();
		//system_set_htimer_alarm(seconds, microseconds);
		//interrupt_enable();
	}

	// test purpose
	///printk("hib: 1 MCHP_PCR_CLK_REQ0 = %08X \n", MCHP_PCR_CLK_REQ0);
	///printk("hib: 1 MCHP_PCR_CLK_REQ1 = %08X \n", MCHP_PCR_CLK_REQ1);
	///printk("hib: 1 MCHP_PCR_CLK_REQ2 = %08X \n", MCHP_PCR_CLK_REQ2);
	///printk("hib: 1 MCHP_PCR_CLK_REQ3 = %08X \n", MCHP_PCR_CLK_REQ3);
	///printk("hib: 1 MCHP_PCR_CLK_REQ4 = %08X \n", MCHP_PCR_CLK_REQ4);
	printk("hib: 1 MCHP_PCR_CLK_REQ0 = %08X \n", pcr->CLK_REQ[0]);
	printk("hib: 1 MCHP_PCR_CLK_REQ1 = %08X \n", pcr->CLK_REQ[1]);
	printk("hib: 1 MCHP_PCR_CLK_REQ2 = %08X \n", pcr->CLK_REQ[2]);
	printk("hib: 1 MCHP_PCR_CLK_REQ3 = %08X \n", pcr->CLK_REQ[3]);
	printk("hib: 1 MCHP_PCR_CLK_REQ4 = %08X \n", pcr->CLK_REQ[4]);
	printk("hib: 1 enter sleep #### \n");

#ifdef CONFIG_UART_XEC	
	/* 9: disable uart0 and JTAG */
	/* Flush console before hibernating */
	cflush();
	/* Disable UART */
	///MCHP_UART_ACT(0) &= ~0x1;
uart0->ACTV &= ~(MCHP_UART_LD_ACTIVATE);
#endif
	for (i = MCHP_INT_GIRQ_FIRST; i <= MCHP_INT_GIRQ_LAST; ++i) {
		MCHP_INT_SOURCE(i)  = 0xffffffff;
	}

	/* 9.3. wait clk idle */
	/* check all clock required status */
    ///while(MCHP_PCR_CLK_REQ0 != 0);
    ///while(MCHP_PCR_CLK_REQ1 != 0x100);    /* bit8=PROCESSOR */
    ///while(MCHP_PCR_CLK_REQ2 != 0);
    ///while(MCHP_PCR_CLK_REQ3 != 0);
    ///while(MCHP_PCR_CLK_REQ4 != 0);	
    while(pcr->CLK_REQ[0] != 0);
    while(pcr->CLK_REQ[1] != 0x100);    /* bit8=PROCESSOR */
    while(pcr->CLK_REQ[2] != 0);
    while(pcr->CLK_REQ[3] != 0);
    while(pcr->CLK_REQ[4] != 0);	

	/* 10: enter deep sleep */
	/*
	 * Set sleep state
	 * arm sleep state to trigger on next WFI
	 */
	///MCHP_PCR_SYS_SLP_CTL |= MCHP_PCR_SYS_SLP_HEAVY;
	///MCHP_PCR_SYS_SLP_CTL |= MCHP_PCR_SYS_SLP_ALL;
pcr->SYS_SLP_CTRL |= MCHP_PCR_SYS_SLP_HEAVY;	

	/* 11: wfi */
	/* GPIO171 as Tst GPIO */
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

	/* arm sleep state to trigger on next WFI */
	__asm__ volatile("dsb");
	__asm__ volatile("wfi");
	__asm__ volatile("isb");
	__asm__ volatile("nop");

#if 1	/* debugging purpose */
	/* Enable UART after wakeup */
	///MCHP_UART_ACT(0) |= 0x1;
uart0->ACTV |= MCHP_UART_LD_ACTIVATE;
	printk("hib: waken up by source!!!!\n");

	/* trace out block source */
	for (i = MCHP_INT_GIRQ_FIRST; i <= MCHP_INT_GIRQ_LAST; ++i) {
		printk("hib: 2 GIRQi iii= %08X \n", i);
		printk("hib: 2 GIRQi           SRC= %08X \n", MCHP_INT_SOURCE(i));
	}
	/* trace out NVIC source */
	printk("hib: nvic pending 0 = %08X \n", CPU_NVIC_PEND(0));
	printk("hib: nvic enable  0 = %08X \n", CPU_NVIC_EN(0));
	printk("hib: nvic pending 1 = %08X \n", CPU_NVIC_PEND(1));
	printk("hib: nvic enable  1 = %08X \n", CPU_NVIC_EN(1));
	printk("hib: nvic pending 2 = %08X \n", CPU_NVIC_PEND(2));
	printk("hib: nvic enable  2 = %08X \n", CPU_NVIC_EN(2));
	printk("hib: nvic pending 3 = %08X \n", CPU_NVIC_PEND(3));
	printk("hib: nvic enable  3 = %08X \n", CPU_NVIC_EN(3));
	printk("hib: nvic pending 4 = %08X \n", CPU_NVIC_PEND(4));
	printk("hib: nvic enable  4 = %08X \n", CPU_NVIC_EN(4));
#endif

	/* GPIO171 as Tst GPIO */
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x10240;
	*(volatile unsigned long*) 0x400811E4 = 0x00240;
	*(volatile unsigned long*) 0x400811E4 = 0x8040;
	/* 12: reboot - _system_reset(0, 1); */
	cros_system_xec_soc_reset(dev);

	/* MCHP TODO */
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
