/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for STM32F446 USB
 */

#ifndef __CHIP_STM32_DWC_USB_REGISTERS_H
#define __CHIP_STM32_DWC_USB_REGISTERS_H

#include "dwc_reg_defs.h"



/* Define USB IRQ used throughout the driver. */
#ifdef CONFIG_DWC_USB_FS
#define DWC_USB_IRQ             STM32_IRQ_OTG_FS
#undef  CONFIG_DWC_ULPI
#else
#define DWC_USB_IRQ             STM32_IRQ_OTG_HS
#define CONFIG_DWC_ULPI
#define CONFIG_DWC_DMA
#endif


/* #define      CONFIG_DWC_DMA */
/* #define      CONFIG_DWC_EXTERNAL_VBUS */
/* #define      CONFIG_DWC_VBUS_SENSE */


struct dwc_usb_ep {
	int max_packet;
	int tx_fifo;

	int out_pending;
	uint8_t *out_data;
	uint8_t *out_databuffer;
	int out_databuffer_max;

	int in_packets;
	int in_pending;
	uint8_t *in_data;
	uint8_t *in_databuffer;
	int in_databuffer_max;
};

struct dwc_usb {
	struct dwc_usb_ep *ep[USB_EP_COUNT];
	int speed;
	int phy_type;
	int dma_en;
};

extern struct dwc_usb_ep ep0_ctl;
extern struct dwc_usb usb_ctl;

/*
 * Added Alias Module Family Base Address to 0-instance Module Base Address
 * Simplify GBASE(mname) macro
 */
#define GC_MODULE_OFFSET         0x10000

#define GBASE(mname)      \
	GC_ ## mname ## _BASE_ADDR
#define GOFFSET(mname, rname)  \
	GC_ ## mname ## _ ## rname ## _OFFSET

#define GREG8(mname, rname) \
	REG8(GBASE(mname) + GOFFSET(mname, rname))
#define GREG32(mname, rname) \
	REG32(GBASE(mname) + GOFFSET(mname, rname))
#define GREG32_ADDR(mname, rname) \
	REG32_ADDR(GBASE(mname) + GOFFSET(mname, rname))
#define GWRITE(mname, rname, value) (GREG32(mname, rname) = (value))
#define GREAD(mname, rname)	    GREG32(mname, rname)

#define GFIELD_MASK(mname, rname, fname) \
	GC_ ## mname ## _ ## rname ## _ ## fname ## _MASK

#define GFIELD_LSB(mname, rname, fname)  \
	GC_ ## mname ## _ ## rname ## _ ## fname ## _LSB

#define GREAD_FIELD(mname, rname, fname) \
	((GREG32(mname, rname) & GFIELD_MASK(mname, rname, fname)) \
		>> GFIELD_LSB(mname, rname, fname))

#define GWRITE_FIELD(mname, rname, fname, fval) \
	(GREG32(mname, rname) = \
	((GREG32(mname, rname) & (~GFIELD_MASK(mname, rname, fname))) | \
	(((fval) << GFIELD_LSB(mname, rname, fname)) & \
		GFIELD_MASK(mname, rname, fname))))


#define GBASE_I(mname, i)     (GBASE(mname) + i*GC_MODULE_OFFSET)

#define GREG32_I(mname, i, rname) \
		REG32(GBASE_I(mname, i) + GOFFSET(mname, rname))

#define GREG32_ADDR_I(mname, i, rname) \
		REG32_ADDR(GBASE_I(mname, i) + GOFFSET(mname, rname))

#define GWRITE_I(mname, i, rname, value) (GREG32_I(mname, i, rname) = (value))
#define GREAD_I(mname, i, rname)	        GREG32_I(mname, i, rname)

#define GREAD_FIELD_I(mname, i, rname, fname) \
	((GREG32_I(mname, i, rname) & GFIELD_MASK(mname, rname, fname)) \
		>> GFIELD_LSB(mname, rname, fname))

#define GWRITE_FIELD_I(mname, i, rname, fname, fval) \
	(GREG32_I(mname, i, rname) = \
	((GREG32_I(mname, i, rname) & (~GFIELD_MASK(mname, rname, fname))) | \
	(((fval) << GFIELD_LSB(mname, rname, fname)) & \
		GFIELD_MASK(mname, rname, fname))))

/* Replace masked bits with val << lsb */
#define REG_WRITE_MLV(reg, mask, lsb, val) \
		(reg = ((reg & ~mask) | ((val << lsb) & mask)))


/* USB device controller */
#define GR_USB_REG(off)               REG32(GC_USB_BASE_ADDR + (off))
#define GR_USB_GOTGCTL                GR_USB_REG(GC_USB_GOTGCTL_OFFSET)
#define GR_USB_GOTGINT                GR_USB_REG(GC_USB_GOTGINT_OFFSET)
#define GR_USB_GAHBCFG                GR_USB_REG(GC_USB_GAHBCFG_OFFSET)
#define GR_USB_GUSBCFG                GR_USB_REG(GC_USB_GUSBCFG_OFFSET)
#define GR_USB_GRSTCTL                GR_USB_REG(GC_USB_GRSTCTL_OFFSET)
#define GR_USB_GINTSTS                GR_USB_REG(GC_USB_GINTSTS_OFFSET)
#define   GINTSTS(bit)                (1 << GC_USB_GINTSTS_ ## bit ## _LSB)
#define GR_USB_GINTMSK                GR_USB_REG(GC_USB_GINTMSK_OFFSET)
#define   GINTMSK(bit)                (1 << GC_USB_GINTMSK_ ## bit ## MSK_LSB)
#define GR_USB_GRXSTSR                GR_USB_REG(GC_USB_GRXSTSR_OFFSET)
#define GR_USB_GRXSTSP                GR_USB_REG(GC_USB_GRXSTSP_OFFSET)
#define GR_USB_GRXFSIZ                GR_USB_REG(GC_USB_GRXFSIZ_OFFSET)
#define GR_USB_GNPTXFSIZ              GR_USB_REG(GC_USB_GNPTXFSIZ_OFFSET)
/*#define GR_USB_GGPIO                  GR_USB_REG(GC_USB_GGPIO_OFFSET)*/
#define GR_USB_GCCFG                  GR_USB_REG(GC_USB_GCCFG_OFFSET)
#define  GCCFG_VBDEN            (1 << 21)
#define  GCCFG_PWRDWN           (1 << 16)
#define GR_USB_PCGCCTL                GR_USB_REG(GC_USB_PCGCCTL_OFFSET)

#define GR_USB_GSNPSID                GR_USB_REG(GC_USB_GSNPSID_OFFSET)
#define GR_USB_GHWCFG1                GR_USB_REG(GC_USB_GHWCFG1_OFFSET)
#define GR_USB_GHWCFG2                GR_USB_REG(GC_USB_GHWCFG2_OFFSET)
#define GR_USB_GHWCFG3                GR_USB_REG(GC_USB_GHWCFG3_OFFSET)
#define GR_USB_GHWCFG4                GR_USB_REG(GC_USB_GHWCFG4_OFFSET)
#define GR_USB_GDFIFOCFG              GR_USB_REG(GC_USB_GDFIFOCFG_OFFSET)
#define GR_USB_DIEPTXF(n)	\
	GR_USB_REG(GC_USB_DIEPTXF1_OFFSET - 4 + (n)*4)
#define GR_USB_DCFG                   GR_USB_REG(GC_USB_DCFG_OFFSET)
#define GR_USB_DCTL                   GR_USB_REG(GC_USB_DCTL_OFFSET)
#define GR_USB_DSTS                   GR_USB_REG(GC_USB_DSTS_OFFSET)
#define GR_USB_DIEPMSK                GR_USB_REG(GC_USB_DIEPMSK_OFFSET)
#define GR_USB_DOEPMSK                GR_USB_REG(GC_USB_DOEPMSK_OFFSET)
#define GR_USB_DAINT                  GR_USB_REG(GC_USB_DAINT_OFFSET)
#define GR_USB_DAINTMSK               GR_USB_REG(GC_USB_DAINTMSK_OFFSET)
#define  DAINT_INEP(ep)               (1 << (ep + GC_USB_DAINTMSK_INEPMSK0_LSB))
#define  DAINT_OUTEP(ep)	\
	(1 << (ep + GC_USB_DAINTMSK_OUTEPMSK0_LSB))
#define GR_USB_DTHRCTL                GR_USB_REG(GC_USB_DTHRCTL_OFFSET)
#define  DTHRCTL_TXTHRLEN_6           (0x40 << 2)
#define  DTHRCTL_RXTHRLEN_6           (0x40 << 17)
#define  DTHRCTL_RXTHREN              (1 << 16)
#define  DTHRCTL_ISOTHREN             (1 << 1)
#define  DTHRCTL_NONISOTHREN          (1 << 0)
#define GR_USB_DIEPEMPMSK             GR_USB_REG(GC_USB_DIEPEMPMSK_OFFSET)

#define GR_USB_EPIREG(off, n)         GR_USB_REG(0x900 + (n) * 0x20 + (off))
#define GR_USB_EPOREG(off, n)         GR_USB_REG(0xb00 + (n) * 0x20 + (off))
#define GR_USB_DIEPCTL(n)             GR_USB_EPIREG(0x00, n)
#define GR_USB_DIEPINT(n)             GR_USB_EPIREG(0x08, n)
#define GR_USB_DIEPTSIZ(n)            GR_USB_EPIREG(0x10, n)
#define GR_USB_DIEPDMA(n)             GR_USB_EPIREG(0x14, n)
#define GR_USB_DTXFSTS(n)             GR_USB_EPIREG(0x18, n)
#define GR_USB_DIEPDMAB(n)            GR_USB_EPIREG(0x1c, n)
#define GR_USB_DOEPCTL(n)             GR_USB_EPOREG(0x00, n)
#define GR_USB_DOEPINT(n)             GR_USB_EPOREG(0x08, n)
#define GR_USB_DOEPTSIZ(n)            GR_USB_EPOREG(0x10, n)
#define GR_USB_DOEPDMA(n)             GR_USB_EPOREG(0x14, n)
#define GR_USB_DOEPDMAB(n)            GR_USB_EPOREG(0x1c, n)

#define GOTGCTL_BVALOEN               (1 << GC_USB_GOTGCTL_BVALIDOVEN_LSB)
#define GOTGCTL_BVALOVAL              (1 << 7)

/* Bit 5 */
#define GAHBCFG_DMA_EN                (1 << GC_USB_GAHBCFG_DMAEN_LSB)
/* Bit 1 */
#define GAHBCFG_GLB_INTR_EN           (1 << GC_USB_GAHBCFG_GLBLINTRMSK_LSB)
/* HS Burst Len */
#define GAHBCFG_HBSTLEN_INCR4         (3 << GC_USB_GAHBCFG_HBSTLEN_LSB)
/* Bit 7 */
#define GAHBCFG_NP_TXF_EMP_LVL        (1 <<  GC_USB_GAHBCFG_NPTXFEMPLVL_LSB)
#define GAHBCFG_TXFELVL               GAHBCFG_NP_TXF_EMP_LVL
#define GAHBCFG_PTXFELVL              (1 << 8)

#define GUSBCFG_TOUTCAL(n)            (((n) << GC_USB_GUSBCFG_TOUTCAL_LSB) \
				       & GC_USB_GUSBCFG_TOUTCAL_MASK)
#define GUSBCFG_USBTRDTIM(n)          (((n) << GC_USB_GUSBCFG_USBTRDTIM_LSB) \
				       & GC_USB_GUSBCFG_USBTRDTIM_MASK)
/* Force device mode */
#define GUSBCFG_FDMOD                 (1 << GC_USB_GUSBCFG_FDMOD_LSB)
#define GUSBCFG_PHYSEL                (1 << 6)
#define GUSBCFG_SRPCAP                (1 << 8)
#define GUSBCFG_HNPCAP                (1 << 9)
#define GUSBCFG_ULPIFSLS              (1 << 17)
#define GUSBCFG_ULPIAR                (1 << 18)
#define GUSBCFG_ULPICSM               (1 << 19)
#define GUSBCFG_ULPIEVBUSD            (1 << 20)
#define GUSBCFG_ULPIEVBUSI            (1 << 21)
#define GUSBCFG_TSDPS                 (1 << 22)
#define GUSBCFG_PCCI                  (1 << 23)
#define GUSBCFG_PTCI                  (1 << 24)
#define GUSBCFG_ULPIIPD               (1 << 25)
#define GUSBCFG_TSDPS                 (1 << 22)


#define GRSTCTL_CSFTRST               (1 << GC_USB_GRSTCTL_CSFTRST_LSB)
#define GRSTCTL_AHBIDLE               (1 << GC_USB_GRSTCTL_AHBIDLE_LSB)
#define GRSTCTL_TXFFLSH               (1 << GC_USB_GRSTCTL_TXFFLSH_LSB)
#define GRSTCTL_RXFFLSH               (1 << GC_USB_GRSTCTL_RXFFLSH_LSB)
#define GRSTCTL_TXFNUM(n)	\
	(((n) << GC_USB_GRSTCTL_TXFNUM_LSB) & GC_USB_GRSTCTL_TXFNUM_MASK)

#define DCFG_DEVSPD_HSULPI            (0 << GC_USB_DCFG_DEVSPD_LSB)
#define DCFG_DEVSPD_FSULPI            (1 << GC_USB_DCFG_DEVSPD_LSB)
#define DCFG_DEVSPD_FS48              (3 << GC_USB_DCFG_DEVSPD_LSB)
#define DCFG_DEVADDR(a)	\
	(((a) << GC_USB_DCFG_DEVADDR_LSB) & GC_USB_DCFG_DEVADDR_MASK)
#define DCFG_NZLSOHSK                 (1 << GC_USB_DCFG_NZSTSOUTHSHK_LSB)

#define DCTL_SFTDISCON                (1 << GC_USB_DCTL_SFTDISCON_LSB)
#define DCTL_CGOUTNAK                 (1 << GC_USB_DCTL_CGOUTNAK_LSB)
#define DCTL_CGNPINNAK                (1 << GC_USB_DCTL_CGNPINNAK_LSB)
#define DCTL_PWRONPRGDONE             (1 << GC_USB_DCTL_PWRONPRGDONE_LSB)

/* Device Endpoint Common IN Interrupt Mask bits */
#define DIEPMSK_AHBERRMSK             (1 << GC_USB_DIEPMSK_AHBERRMSK_LSB)
#define DIEPMSK_BNAININTRMSK          (1 << GC_USB_DIEPMSK_BNAININTRMSK_LSB)
#define DIEPMSK_EPDISBLDMSK           (1 << GC_USB_DIEPMSK_EPDISBLDMSK_LSB)
#define DIEPMSK_INEPNAKEFFMSK         (1 << GC_USB_DIEPMSK_INEPNAKEFFMSK_LSB)
#define DIEPMSK_INTKNEPMISMSK         (1 << GC_USB_DIEPMSK_INTKNEPMISMSK_LSB)
#define DIEPMSK_INTKNTXFEMPMSK        (1 << GC_USB_DIEPMSK_INTKNTXFEMPMSK_LSB)
#define DIEPMSK_NAKMSK                (1 << GC_USB_DIEPMSK_NAKMSK_LSB)
#define DIEPMSK_TIMEOUTMSK            (1 << GC_USB_DIEPMSK_TIMEOUTMSK_LSB)
#define DIEPMSK_TXFIFOUNDRNMSK        (1 << GC_USB_DIEPMSK_TXFIFOUNDRNMSK_LSB)
#define DIEPMSK_XFERCOMPLMSK          (1 << GC_USB_DIEPMSK_XFERCOMPLMSK_LSB)

/* Device Endpoint Common OUT Interrupt Mask bits */
#define DOEPMSK_AHBERRMSK             (1 << GC_USB_DOEPMSK_AHBERRMSK_LSB)
#define DOEPMSK_BBLEERRMSK            (1 << GC_USB_DOEPMSK_BBLEERRMSK_LSB)
#define DOEPMSK_BNAOUTINTRMSK         (1 << GC_USB_DOEPMSK_BNAOUTINTRMSK_LSB)
#define DOEPMSK_EPDISBLDMSK           (1 << GC_USB_DOEPMSK_EPDISBLDMSK_LSB)
#define DOEPMSK_NAKMSK                (1 << GC_USB_DOEPMSK_NAKMSK_LSB)
#define DOEPMSK_NYETMSK               (1 << GC_USB_DOEPMSK_NYETMSK_LSB)
#define DOEPMSK_OUTPKTERRMSK          (1 << GC_USB_DOEPMSK_OUTPKTERRMSK_LSB)
#define DOEPMSK_OUTTKNEPDISMSK        (1 << GC_USB_DOEPMSK_OUTTKNEPDISMSK_LSB)
#define DOEPMSK_SETUPMSK              (1 << GC_USB_DOEPMSK_SETUPMSK_LSB)
#define DOEPMSK_STSPHSERCVDMSK        (1 << GC_USB_DOEPMSK_STSPHSERCVDMSK_LSB)
#define DOEPMSK_XFERCOMPLMSK          (1 << GC_USB_DOEPMSK_XFERCOMPLMSK_LSB)

/* Device Endpoint-n IN Interrupt Register bits */
#define DIEPINT_AHBERR             (1 << GC_USB_DIEPINT0_AHBERR_LSB)
#define DIEPINT_BBLEERR            (1 << GC_USB_DIEPINT0_BBLEERR_LSB)
#define DIEPINT_BNAINTR            (1 << GC_USB_DIEPINT0_BNAINTR_LSB)
#define DIEPINT_EPDISBLD           (1 << GC_USB_DIEPINT0_EPDISBLD_LSB)
#define DIEPINT_INEPNAKEFF         (1 << GC_USB_DIEPINT0_INEPNAKEFF_LSB)
#define DIEPINT_INTKNEPMIS         (1 << GC_USB_DIEPINT0_INTKNEPMIS_LSB)
#define DIEPINT_INTKNTXFEMP        (1 << GC_USB_DIEPINT0_INTKNTXFEMP_LSB)
#define DIEPINT_NAKINTRPT          (1 << GC_USB_DIEPINT0_NAKINTRPT_LSB)
#define DIEPINT_NYETINTRPT         (1 << GC_USB_DIEPINT0_NYETINTRPT_LSB)
#define DIEPINT_PKTDRPSTS          (1 << GC_USB_DIEPINT0_PKTDRPSTS_LSB)
#define DIEPINT_TIMEOUT            (1 << GC_USB_DIEPINT0_TIMEOUT_LSB)
#define DIEPINT_TXFEMP             (1 << GC_USB_DIEPINT0_TXFEMP_LSB)
#define DIEPINT_TXFIFOUNDRN        (1 << GC_USB_DIEPINT0_TXFIFOUNDRN_LSB)
#define DIEPINT_XFERCOMPL          (1 << GC_USB_DIEPINT0_XFERCOMPL_LSB)

/* Device Endpoint-n OUT Interrupt Register bits */
#define DOEPINT_AHBERR             (1 << GC_USB_DOEPINT0_AHBERR_LSB)
#define DOEPINT_BACK2BACKSETUP     (1 << GC_USB_DOEPINT0_BACK2BACKSETUP_LSB)
#define DOEPINT_BBLEERR            (1 << GC_USB_DOEPINT0_BBLEERR_LSB)
#define DOEPINT_BNAINTR            (1 << GC_USB_DOEPINT0_BNAINTR_LSB)
#define DOEPINT_EPDISBLD           (1 << GC_USB_DOEPINT0_EPDISBLD_LSB)
#define DOEPINT_NAKINTRPT          (1 << GC_USB_DOEPINT0_NAKINTRPT_LSB)
#define DOEPINT_NYETINTRPT         (1 << GC_USB_DOEPINT0_NYETINTRPT_LSB)
#define DOEPINT_OUTPKTERR          (1 << GC_USB_DOEPINT0_OUTPKTERR_LSB)
#define DOEPINT_OUTTKNEPDIS        (1 << GC_USB_DOEPINT0_OUTTKNEPDIS_LSB)
#define DOEPINT_PKTDRPSTS          (1 << GC_USB_DOEPINT0_PKTDRPSTS_LSB)
#define DOEPINT_SETUP              (1 << GC_USB_DOEPINT0_SETUP_LSB)
#define DOEPINT_STSPHSERCVD        (1 << GC_USB_DOEPINT0_STSPHSERCVD_LSB)
#define DOEPINT_STUPPKTRCVD        (1 << GC_USB_DOEPINT0_STUPPKTRCVD_LSB)
#define DOEPINT_XFERCOMPL          (1 << GC_USB_DOEPINT0_XFERCOMPL_LSB)

#define DXEPCTL_EPTYPE_CTRL           (0 << GC_USB_DIEPCTL0_EPTYPE_LSB)
#define DXEPCTL_EPTYPE_ISO            (1 << GC_USB_DIEPCTL0_EPTYPE_LSB)
#define DXEPCTL_EPTYPE_BULK           (2 << GC_USB_DIEPCTL0_EPTYPE_LSB)
#define DXEPCTL_EPTYPE_INT            (3 << GC_USB_DIEPCTL0_EPTYPE_LSB)
#define DXEPCTL_EPTYPE_MASK           GC_USB_DIEPCTL0_EPTYPE_MASK
#define DXEPCTL_TXFNUM(n)             ((n) << GC_USB_DIEPCTL1_TXFNUM_LSB)
#define DXEPCTL_STALL                 (1 << GC_USB_DIEPCTL0_STALL_LSB)
#define DXEPCTL_CNAK                  (1 << GC_USB_DIEPCTL0_CNAK_LSB)
#define DXEPCTL_DPID                  (1 << GC_USB_DIEPCTL0_DPID_LSB)
#define DXEPCTL_SNAK                  (1 << GC_USB_DIEPCTL0_SNAK_LSB)
#define DXEPCTL_NAKSTS                (1 << GC_USB_DIEPCTL0_NAKSTS_LSB)
#define DXEPCTL_EPENA                 (1 << GC_USB_DIEPCTL0_EPENA_LSB)
#define DXEPCTL_EPDIS                 (1 << GC_USB_DIEPCTL0_EPDIS_LSB)
#define DXEPCTL_USBACTEP              (1 << GC_USB_DIEPCTL0_USBACTEP_LSB)
#define DXEPCTL_MPS64                 (0 << GC_USB_DIEPCTL0_MPS_LSB)
#define DXEPCTL_MPS(cnt)              ((cnt) << GC_USB_DIEPCTL1_MPS_LSB)

#define DXEPTSIZ_SUPCNT(n)            ((n) << GC_USB_DOEPTSIZ0_SUPCNT_LSB)
#define DXEPTSIZ_PKTCNT(n)            ((n) << GC_USB_DIEPTSIZ0_PKTCNT_LSB)
#define DXEPTSIZ_XFERSIZE(n)          ((n) << GC_USB_DIEPTSIZ0_XFERSIZE_LSB)

#define DOEPDMA_BS_HOST_RDY           (0 << 30)
#define DOEPDMA_BS_DMA_BSY            (1 << 30)
#define DOEPDMA_BS_DMA_DONE           (2 << 30)
#define DOEPDMA_BS_HOST_BSY           (3 << 30)
#define DOEPDMA_BS_MASK               (3 << 30)
#define DOEPDMA_RXSTS_MASK            (3 << 28)
#define DOEPDMA_LAST                  (1 << 27)
#define DOEPDMA_SP                    (1 << 26)
#define DOEPDMA_IOC                   (1 << 25)
#define DOEPDMA_SR                    (1 << 24)
#define DOEPDMA_MTRF                  (1 << 23)
#define DOEPDMA_NAK                   (1 << 16)
#define DOEPDMA_RXBYTES(n)            (((n) & 0xFFFF) << 0)
#define DOEPDMA_RXBYTES_MASK          (0xFFFF << 0)

#define DIEPDMA_BS_HOST_RDY           (0 << 30)
#define DIEPDMA_BS_DMA_BSY            (1 << 30)
#define DIEPDMA_BS_DMA_DONE           (2 << 30)
#define DIEPDMA_BS_HOST_BSY           (3 << 30)
#define DIEPDMA_BS_MASK               (3 << 30)
#define DIEPDMA_TXSTS_MASK            (3 << 28)
#define DIEPDMA_LAST                  (1 << 27)
#define DIEPDMA_SP                    (1 << 26)
#define DIEPDMA_IOC                   (1 << 25)
#define DIEPDMA_TXBYTES(n)            (((n) & 0xFFFF) << 0)
#define DIEPDMA_TXBYTES_MASK          (0xFFFF << 0)

#endif /* ! __CHIP_STM32_DWC_USB_REGISTERS_H */
