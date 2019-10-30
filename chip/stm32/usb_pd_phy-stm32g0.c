/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "crc.h"
#include "dma.h"
#include "gpio.h"
#include "hwtimer.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

/* UCPD_CFG1 register */

/**
 * Enable UCPD peripheral
 *
 * @param port  TypeC port
 */
static inline void ucpd_enable(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG1(base) |= STM32_UCPD_CFG1_UCPDEN;
}

/**
 * Disable UCPD peripheral
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG1(base) &= ~STM32_UCPD_CFG1_UCPDEN;
}

/**
 * Check if UCPD peripheral is enabled
 *
 * @param port  TypeC port
 * @return State of bit (1 or 0).
 */
static inline uint32_t ucpd_is_enabled(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return ((STM32_UCPD_CFG1(base) & STM32_UCPD_CFG1_UCPDEN) ? 1 : 0);
}

/**
 * Set the receiver ordered set detection enable
 *
 * @param  port  TypeC port
 * @param  value  This parameter can be combination of the following values:
 *
 *	RX_ORDERSET_SOP
 *	RX_ORDERSET_SOPP
 *	RX_ORDERSET_SOPPP
 *	RX_ORDERSET_HARD_RESET
 *	RX_ORDERSET_CABLE_DETECT
 *	RX_ORDERSET_SOPP_DEBUG
 *	RX_ORDERSET_SOPPP_DEBUG
 *	RX_ORDERSET_SOP_EXT1
 *	RX_ORDERSET_SOP_EXT2
 */
static inline void ucpd_set_rx_orderset(int port, enum ucpd_rx_orderset value)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CFG1(base);

	reg &= ~STM32_UCPD_CFG1_RXORDSETEN(-1);
	reg |= STM32_UCPD_CFG1_RXORDSETEN(value);

	STM32_UCPD_CFG1(base) = reg;
}

/**
 * Set the prescaler for ucpd clock
 * NOTE: should only be called when when UCPDEN is disabled, UCPDEN=0
 *
 * @param  port TypeC port
 * @param  value This parameter can be one of the following values:
 *	PSCCLK_DIV1
 *	PSCCLK_DIV2
 *	PSCCLK_DIV4
 *	PSCCLK_DIV8
 *	PSCCLK_DIV16
 */
static inline void ucpd_set_pscclk(int port, enum ucpd_pscclk value)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CFG1(base);

	reg &= ~STM32_UCPD_CFG1_PSC_USBPDCLK(-1);
	reg |= STM32_UCPD_CFG1_PSC_USBPDCLK(value);

	STM32_UCPD_CFG1(base) = reg;
}

/**
 * Set the number of cycles (minus 1) of the half bit clock
 * NOTE: should only be called when when UCPDEN is disabled, UCPDEN=0
 *
 * @param  port TypeC port
 * @param  value a value between Min_Data=0x1 and Max_Data=0x1F
 */
static inline void ucpd_set_transwin(int port, uint32_t value)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CFG1(base);

	reg &= ~STM32_UCPD_CFG1_TRANSWIN(-1);
	reg |= STM32_UCPD_CFG1_TRANSWIN(value);

	STM32_UCPD_CFG1(base) = reg;
}

/**
 * Set the clock divider value to generate an interframe gap
 * NOTE: should only be called when when UCPDEN is disabled, UCPDEN=0
 *
 * @param  port TypeC port
 * @param  valee  a value between Min_Data=0x1 and Max_Data=0x1F
 * @retval None
 */
static inline void ucpd_set_ifrgap(int port, uint32_t value)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CFG1(base);

	reg &= ~STM32_UCPD_CFG1_IFRGAP(-1);
	reg |= STM32_UCPD_CFG1_IFRGAP(value);

	STM32_UCPD_CFG1(base) = reg;
}

/**
 * Set the clock divider value to generate an interframe gap
 * NOTE: should only be called when when UCPDEN is disabled, UCPDEN=0
 *
 * @param  port TypeC port
 * @param  value a value between Min_Data=0x0 and Max_Data=0x3F
 */
static inline void ucpd_set_hbitclkdiv(int port, uint32_t value)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CFG1(base);

	reg &= ~STM32_UCPD_CFG1_HBTCLKDIV(-1);
	reg |= STM32_UCPD_CFG1_HBTCLKDIV(value);

	STM32_UCPD_CFG1(base) = reg;
}

/**
 * Enable Rx DMA
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_rx_dma(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG1(base) |= STM32_UCPD_CFG1_RXDMAEN;
}

/**
 * Disable Rx DMA
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_rx_dma(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG1(base) &= ~STM32_UCPD_CFG1_RXDMAEN;
}

/**
 * Enable Tx DMA
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_tx_dma(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG1(base) |= STM32_UCPD_CFG1_TXDMAEN;

}

/**
 * Disable Tx DMA
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_tx_dma(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG1(base) &= ~STM32_UCPD_CFG1_TXDMAEN;
}


/* UCPD_CFG2 register */

/**
 * Enable wakeup mode
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_wakeup(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG2(base) |= STM32_UCPD_CFG2_WUPEN;
}

/**
 * Disable wakeup mode
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_wakeup(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG2(base) &= ~STM32_UCPD_CFG2_WUPEN;
}

/**
 * Enable Force clock
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_forceclk(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG2(base) |= STM32_UCPD_CFG2_FORCECLK;
}

/**
 * Disable Force clock
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_forceclk(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG2(base) &= ~STM32_UCPD_CFG2_FORCECLK;
}

/**
 * Enable RxFilter
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_rxfilter(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG2(base) &= ~STM32_UCPD_CFG2_RXFILTDIS;
}

/**
 * Disable RxFilter
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_rxfilter(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CFG2(base) |= STM32_UCPD_CFG2_RXFILTDIS;
}

/* UCPD_CR register */

/**
 * Enable Type C detector for CC2
 * @param  port TypeC port
 */
static inline void ucpd_enable_typec_detection_cc2_enable(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_CC2TCDIS;
}

/**
 * Disable Type C detector for CC2
 * @param  port TypeC port
 */
static inline void ucpd_disable_typec_detection_cc2_enable(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_CC2TCDIS;
}

/**
 * Enable Type C detector for CC1
 * @param  port TypeC port
 */
static inline void ucpd_enable_typec_detection_cc1_enable(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_CC1TCDIS;
}

/**
 * Disable Type C detector for CC1
 * @param  port TypeC port
 */
static inline void ucpd_disable_typec_detection_cc1_enable(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_CC1TCDIS;
}

/**
 * Enable Source Vconn discharge
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_vconn_discharge(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_RDCH;
}

/**
 * Source Vconn discharge disable
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_vconn_discharge(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_RDCH;
}

/**
 * Signal Fast Role Swap request
 *
 * @param  port TypeC port
 */
static inline void ucpd_signal_frstx(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_FRSTX;
}

/**
 * Enable Fast Role swap RX detection
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_frs_detection(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_FRSRXEN;
}

/**
 * Disable Fast Role swap RX detection
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_frs_detection(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_FRSRXEN;
}


static inline void mode2(int port)
{
}

/**
 * Set cc enable
 *
 * @param  port TypeC port
 * @param  cce This parameter can be one of the following values:
 *	CCENABLE_NONE
 *	CCENABLE_CC1
 *	CCENABLE_CC2
 *	CCENABLE_CC1CC2
 */
static inline void ucpd_set_ccenable(int port, enum ucpd_ccenable cce)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CR(base);
/*
	CPRINTF("Negating: 0x%08x\n", ~STM32_UCPD_CR_CCENABLE(-1));
	CPRINTF("Or'ing: 0x%08x\n", STM32_UCPD_CR_CCENABLE(cce));
*/
	reg &= ~STM32_UCPD_CR_CCENABLE(-1);
	reg |= STM32_UCPD_CR_CCENABLE(cce);

	STM32_UCPD_CR(base) = reg;
}

/**
 * Set UCPD SNK role
 *
 * @param  port TypeC port
 */
static inline void ucpd_set_snk_role(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_ANAMODE;
}

/**
 * Set UCPD SRC role
 *
 * @param  port TypeC port
 */
static inline void ucpd_set_src_role(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_ANAMODE;
}

/**
 * Get UCPD Role
 *
 * @param  port TypeC port
 * @return one of the following values:
 *	ANAMODE_SRC
 *	ANAMODE_SNK
 */
static inline uint32_t ucpd_get_role(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return !!(STM32_UCPD_CR(base) & STM32_UCPD_CR_ANAMODE);
}

/**
 * Set Rp resistor
 *
 * @param  port TypeC port
 * @param  rp This parameter can be one of the following values:
 *	ANASUBMODE_RP_NONE
 *	ANASUBMODE_RP_DEFAULT
 *	ANASUBMODE_RP_1_5A
 *	ANASUBMODE_RP_3_0A
 */
static inline void ucpd_set_rp(int port, enum ucpd_anasubmode rp)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CR(base);

	reg &= ~STM32_UCPD_CR_ANASUBMODE(-1);
	reg |= STM32_UCPD_CR_ANASUBMODE(rp);

	STM32_UCPD_CR(base) = reg;
}

/**
 * Set CC pin
 *
 * @param  port TypeC port
 * @param  ccp This parameter can be one of the following values:
 *	CCPIN_CC1
 *	CCPIN_CC2
 */
static inline void ucpd_set_ccpin(int port, enum ucpd_ccpin ccp)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	if (ccp)
		STM32_UCPD_CR(base) |= STM32_UCPD_CR_PHYCCSEL;
	else
		STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_PHYCCSEL;
}

/**
 * Enable Rx
 *
 * @param  port TypeC port
 */
static inline void ucpd_enable_rx(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_PHYRXEN;
}

/**
 * Disable Rx
 *
 * @param  port TypeC port
 */
static inline void ucpd_disable_rx(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_PHYRXEN;
}

/**
 * Set Rx mode
 *
 * @param  port TypeC port
 * @param  rxm This parameter can be one of the following values:
 *	RXMODE_NORMAL
 *	RXMODE_BIST_TEST_DATA
 */
static inline void ucpd_set_rx_mode(int port, enum ucpd_rxmode rxm)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	if (rxm)
		STM32_UCPD_CR(base) |= STM32_UCPD_CR_RXMODE;
	else
		STM32_UCPD_CR(base) &= ~STM32_UCPD_CR_RXMODE;
}

/**
 * Send Hard Reset
 *
 * @param  port TypeC port
 */
static inline void ucpd_send_hard_reset(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_TXHRST;
}

/**
 * Send message
 *
 * @param  port TypeC port
 */
static inline void ucpd_send_message(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_CR(base) |= STM32_UCPD_CR_TXSEND;
}

/**
 * Set Tx mode
 *
 * @param  port TypeC port
 * @param  txm This parameter can be one of the following values:
 *	TXMODE_NORMAL
 *	TXMODE_CABLE_RESET
 *	TXMODE_BIST_CARRIER2
 */
static inline void ucpd_set_txmode(int port, enum ucpd_txmode txm)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	uint32_t reg = STM32_UCPD_CR(base);

	reg &= ~STM32_UCPD_CR_TXMODE(-1);
	reg |= STM32_UCPD_CR_TXMODE(txm);

	STM32_UCPD_CR(base) = reg;
}

/* UCPD_IMR register */

/**
 * @brief  Enable UCPD interrupt
 *
 * @param  port TypeC port
 * @param  ine This parameter can be a combination of the following values:
 *	IMR_TXISIE	- Enable Tx data receive interrupt
 *	IMR_TXMSGDISCIE	- Enable Tx message discarded interrupt
 *	IMR_TXMSGSENTIE	- Enable Tx message sent interrupt
 *	IMR_TXMSGABTIE	- Enable Tx message abort interrupt
 *	IMR_HRSTDISCIE	- Enable hard reset discard interrupt
 *	IMR_HRSTSENTIE	- Enable hard reset sent interrupt
 *	IMR_TXUNDIE	- Enable TX underrun interrupt
 *	IMR_RXNEIE	- Enable Rx non empty interrupt
 *	IMR_RXORDDETIE	- Enable Rx orderset interrupt
 *	IMR_RXHRSTDETIE	- Enable Rx hard resrt interrupt
 *	IMR_RXOVRIE	- Enable Rx overrun interrupt
 *	IMR_RXMSGENDIE	- Enable Rx message end interrupt
 *	IMR_TYPECEVT1IE - Enable type c event on CC1
 *	IMR_TYPECEVT2IE - Enable type c event on CC2
 *	IMR_FRSEVTIE	- Enable FRS interrupt
 */
static inline void ucpd_enable_interrupt(int port, enum ucpd_imr ie)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_IMR(base) |= ie;
}

/**
 * @brief  Disable UCPD interrupt
 *
 * @param  port TypeC port
 * @param  ine This parameter can be a combination of the following values:
 *	IMR_TXISIE	- Disable Tx data receive interrupt
 *	IMR_TXMSGDISCIE	- Disable Tx message discarded interrupt
 *	IMR_TXMSGSENTIE	- Disable Tx message sent interrupt
 *	IMR_TXMSGABTIE	- Disable Tx message abort interrupt
 *	IMR_HRSTDISCIE	- Disable hard reset discard interrupt
 *	IMR_HRSTSENTIE	- Disable hard reset sent interrupt
 *	IMR_TXUNDIE	- Disable TX underrun interrupt
 *	IMR_RXNEIE	- Disable Rx non empty interrupt
 *	IMR_RXORDDETIE	- Disable Rx orderset interrupt
 *	IMR_RXHRSTDETIE	- Disable Rx hard resrt interrupt
 *	IMR_RXOVRIE	- Disable Rx overrun interrupt
 *	IMR_RXMSGENDIE	- Disable Rx message end interrupt
 *	IMR_TYPECEVT1IE - Disable type c event on CC1
 *	IMR_TYPECEVT2IE - Disable type c event on CC2
 *	IMR_FRSEVTIE	- Disable FRS interrupt
 */
static inline void ucpd_disable_interrupt(int port, enum ucpd_imr ie)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_IMR(base) &= ~ie;
}

/**
 * @brief  Clear UCPD interrupt
 *
 * @param  port TypeC port
 * @param  ine This parameter can be a combination of the following values:
 *	IMR_TXMSGDISCCF	- Clear Tx message discarded interrupt
 *	IMR_TXMSGSENTCF	- Clear Tx message sent interrupt
 *	IMR_TXMSGABTCF	- Clear Tx message abort interrupt
 *	IMR_HRSTDISCCF	- Clear hard reset discard interrupt
 *	IMR_HRSTSENTCF	- Clear hard reset sent interrupt
 *	IMR_TXUNDCF	- Clear TX underrun interrupt
 *	IMR_RXORDDETCF	- Clear Rx orderset interrupt
 *	IMR_RXHRSTDETCF	- Clear Rx hard resrt interrupt
 *	IMR_RXOVRCF	- Clear Rx overrun interrupt
 *	IMR_RXMSGENDCF	- Clear Rx message end interrupt
 *	IMR_TYPECEVT1CF - Clear type c event on CC1
 *	IMR_TYPECEVT2CF - Clear type c event on CC2
 *	IMR_FRSEVTCF	- Clear FRS interrupt
 */
static inline void ucpd_clear_interrupt(int port, enum ucpd_icr cf)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_ICR(base) |= cf;
}

/**
 * @brief  Check UCPD interrupt
 *
 * @param  port TypeC port
 * @param  sr This parameter can be a combination of the following values:
 *	SR_TXIS		- Check if Tx data receive interrupt
 *	SR_TXMSGDISC	- Check if Tx message discarded interrupt
 *	SR_TXMSGSENT	- Check if Tx message sent interrupt
 *	SR_TXMSGABT	- Check if Tx message abort interrupt
 *	SR_HRSTDISC	- Check if hard reset discard interrupt
 *	SR_HRSTSENT	- Check if hard reset sent interrupt
 *	SR_TXUND	- Check if TX underrun interrupt
 *	SR_RXNE		- Check if Rx non empty interrupt
 *	SR_RXORDDET	- Check if Rx orderset interrupt
 *	SR_RXHRSTDET	- Check if Rx hard resrt interrupt
 *	SR_RXOVR	- Check if Rx overrun interrupt
 *	SR_RXMSGEND	- Check if Rx message end interrupt
 *	SR_RXERR	- Check if RX message no completed OK
 *	SR_TYPECEVT1	- Check if type c event on CC1
 *	SR_TYPECEVT2	- Check if type c event on CC2
 *	SR_FRSEVT	- Check if FRS interrupt
 * @param return 1 if the interrupt is set, else 0
 */
static inline uint32_t ucpd_check_interrupt(int port, enum ucpd_sr sr)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return !!(STM32_UCPD_SR(base) & sr);
}

/**
 * Get vstate value for CC2
 *
 * @param  port TypeC port
 * @return vstate value for CC2
 */
static inline uint32_t ucpd_get_typec_vstate_cc2(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return UCPD_SR_TYPEC_VSTATE_CC2(STM32_UCPD_SR(base));
}

/**
 * Get vstate value for CC1
 *
 * @param  port TypeC port
 * @return vstate value for CC1
 */
static inline uint32_t ucpd_get_typec_vstate_cc1(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return UCPD_SR_TYPEC_VSTATE_CC1(STM32_UCPD_SR(base));
}

/**
 * Write the orderset for Tx message
 *
 * @param  port TypeC port
 * @param  tos one of the following value:
 *	TX_ORDERED_SET_SOP
 *	TX_ORDERED_SET_SOP1
 *	TX_ORDERED_SET_SOP2
 *	TX_ORDERED_SET_HARD_RESET
 *	TX_ORDERED_SET_CABLE_RESET
 *	TX_ORDERED_SET_SOP1_DEBUG
 *	TX_ORDERED_SET_SOP2_DEBUG
 */
static inline void ucpd_set_tx_orderset(int port, enum ucpd_tx_ordset tos)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_TX_ORDSET(base) = tos;
}

/**
 * Write the Tx paysize
 *
 * @param  port TypeC port
 * @param  tps payload size
 */
static inline void ucpd_write_tx_paysize(int port, uint32_t tps)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_TX_PAYSZ(base) = STM32_UCPD_TXPAYSZ(tps);
}

/**
 * Write data
 *
 * @param  port TypeC port
 * @param  Data Value between Min_Data=0x00 and Max_Data=0xFF
 */
static inline void ucpd_write_txdr(int port, uint8_t data)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_TXDR(base) = data;
}

/**
 * Read RX the orderset
 *
 * @param  port TypeC port
 * @return RxOrderSet one of the following value
 *	RXORDSET_SOP
 *	RXORDSET_SOP1
 *	RXORDSET_SOP2
 *	RXORDSET_SOP1_DEBUG
 *	RXORDSET_SOP2_DEBUG
 *	RXORDSET_CABLE_RESET
 *	RXORDSET_SOPEXT1
 *	RXORDSET_SOPEXT2
 */
static inline enum ucpd_rx_ordset ucpd_read_rx_orderset(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return UCPD_RX_ORDSET_RXORDSET(STM32_UCPD_RX_ORDSET(base));
}

/**
 * Read the Rx paysize
 *
 * @param  port TypeC port
 * @return RXPaysize.
 */
static inline uint32_t ucpd_read_rx_paysize(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return UCPD_RX_PAYSZ(STM32_UCPD_RX_PAYSZ(base));
}

/**
 * Read data
 *
 * @param  port TypeC port
 * @return RxData Value between Min_Data=0x00 and Max_Data=0xFF
 */
static inline uint32_t ucpd_read_rxdr(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	return UCPD_RXDR_RXDATA(STM32_UCPD_RXDR(base));
}

/**
 * Set Rx OrderSet Ext1
 *
 * @param  port TypeC port
 * @param  sope Value between Min_Data=0x00000 and Max_Data=0xFFFFF
 */
static inline void ucpd_set_orderset_ext1(int port, uint32_t sope)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_RX_ORDEXT1(base) = STM32_UCPD_RXSOPX1(sope);
}

/**
 * Set Rx OrderSet Ext2
 *
 * @param  port TypeC port
 * @param  SOPExt Value between Min_Data=0x00000 and Max_Data=0xFFFFF
 */
static inline void ucpd_set_orderset_ext2(int port, uint32_t sope)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;

	STM32_UCPD_RX_ORDEXT2(base) = STM32_UCPD_RXSOPX2(sope);
}

// Dubious

//static struct pd_physical {
//	int rx_started;
//} pd_phy[CONFIG_USB_PD_PORT_COUNT];

int pd_rx_started(int port)
{
	//CPRINTF("pd_rx_started(%d) = %d\n", port, pd_phy[port].rx_started);
	//return pd_phy[port].rx_started;
	return 1;
	//CPRINTF("state %d %d\n",
	//	ucpd_get_typec_vstate_cc1(port),
	//	ucpd_get_typec_vstate_cc2(port));
}

int pd_analyze_rx(int port, uint32_t *payload)
{
	CPRINTF("pd_analyze_rx()\n");
	return 0;
}

void pd_rx_complete(int port)
{
	CPRINTF("pd_rx_complete()\n");
}

void pd_rx_enable_monitoring(int port)
{
	CPRINTF("pd_rx_enable_monitoring(%d)\n", port);
}

void pd_rx_disable_monitoring(int port)
{
	CPRINTF("pd_rx_disable_monitoring(%d)\n", port);
}

int send_hard_reset(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	int val = STM32_UCPD_CR(base);
	val |= 0x0008;
	
	CPRINTF("send_hard_reset(%d)\n", port);
	STM32_UCPD_CR(base) = val;
	return 0;
}


int send_validate_message(int port, uint16_t header,
			  const uint32_t *data)
{
	CPRINTF("send_validate_message()\n");
	return 0;
}

void send_goodcrc(int port, int id)
{
	CPRINTF("send_goodcrc()\n");
}

void bist_mode_2_tx(int port)
{
	uint32_t base = port ? STM32_UCPD2_BASE : STM32_UCPD1_BASE;
	int val = STM32_UCPD_CR(base);
	val &= ~0x0003;
	val |= 0x0006;
	
	CPRINTF("bist_mode_2_tx(%d)\n", port);
	STM32_UCPD_CR(base) = val;
}

// End of dubious

void pd_select_polarity(int port, int polarity)
{
	CPRINTF("pd_select_polarity(%d, %d)\n", port, polarity);
}

int pd_read_cc_status(int port, int cc)
{
	switch (cc == 0
		? ucpd_get_typec_vstate_cc1(port)
		: ucpd_get_typec_vstate_cc2(port)) {
	case 0:
		return TYPEC_CC_VOLT_OPEN;
	case 1:
		return TYPEC_CC_VOLT_RP_DEF;
	case 2:
		return TYPEC_CC_VOLT_RP_1_5;
	case 3:
		return TYPEC_CC_VOLT_RP_3_0;
	}
	return 0;
}

void pd_set_clock(int port, int freq)
{
}

void pd_hw_init(int port, int role)
{
	CPRINTF("pd_hw_init(%d, %d)\n", port, role);
	STM32_RCC_APBENR1 |= port ? STM32_RCC_UCPD2EN : STM32_RCC_UCPD1EN;
	msleep(1);

	ucpd_disable(port);

	/* PSCCLK = (HSI16 / 1) = 16 MHz */
	ucpd_set_pscclk(port, PSCCLK_DIV1);

	/* HBITCLK = (PSCLK / 26) = 615384 Hz approx. */
	ucpd_set_hbitclkdiv(port, 0x19);

	/* TRANSWIN = (1 / (HBITCLK / 8)) = 13 uS */
	ucpd_set_transwin(port, 0x7);

	/* IFRGAP CLK = (HBITCLK / 17) = 36199 Hz approx. */
	ucpd_set_ifrgap(port, 0x10);

	ucpd_set_rx_orderset(port, RX_ORDERSET_SOP | RX_ORDERSET_HARD_RESET);
	ucpd_set_tx_orderset(port, TX_ORDERSET_SOP | TX_ORDERSET_HARD_RESET);

	ucpd_enable(port);

	ucpd_enable_typec_detection_cc1_enable(port);
	ucpd_enable_typec_detection_cc2_enable(port);
	
	ucpd_set_ccenable(port, CCENABLE_CC1CC2);
	ucpd_set_snk_role(port);

	/* disable dead battery */
	STM32_RCC_APBENR2 |= STM32_RCC_SYSCFGEN;
	/* strobe */
	STM32_SYSCFG_CFGR1 |= (port) ?
		STM32_SYSCFG_CFGR1_UCPD2_STROBE :
		STM32_SYSCFG_CFGR1_UCPD1_STROBE;
	CPRINTF("pd_hw_init(%d, %d) done\n", port, role);
}
