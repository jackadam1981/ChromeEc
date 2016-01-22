/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MMIO Register define */
#define INT_MASK0		0x0003
  #define IRQ0_CC2_STB			0x80
  #define IRQ0_PS_ST_CHG		0x01

#define INT_MASK1		0x0004
  #define IRQ1_RX_AVAILABLE		0x80
  #define IRQ1_SRST_RX			0x40
  #define IRQ1_HRST_RX			0x20
  #define IRQ1_TX_DONE			0x04
  #define IRQ1_CC1_STB			0x01

#define INT_MASK2		0x0005
#define INT_STATUS0		0x0008
#define INT_STATUS1		0x0009
#define INT_STATUS2		0x000A
#define INT_MANUAL1		0x000F

#define INT_SRC_CLKON		0x0011
  #define ISC_IRQ_CLK_ON_EN		0x01

#define PMU_RESET_CTRL		0x0040
  #define PRC_SW_RESET			0x01

#define PMU_CLK_CTRL0		0x0041
#define PMU_LOWPOWER		0x0042
#define PMU_STATUS		0x0048
  #define PST_POR_COMPLETE		0x01

#define REV_ID			0x004A
  #define RID_REVID_A0			0xA0

#define SW_PROT_RST_STAT_CTRL	0x0100
  #define SPRSC_CAB_RST_ALLOW		0x20
  #define SPRSC_HRST_ALLOW		0x10
  #define SPRSC_SEND_HRST		0x08
  #define SPRSC_HRST_TYPE_MASK		0x06
  #define SPRSC_HRST_TYPE_CABLE		0x02
  #define SPRSC_RX_EN			0x01

#define SW_PROT_STATUS_0	0x0102
  #define SPST0_HRST_SENT		0x01

#define SW_PD_INFO_0		0x0103
  #define SPI0_RETRYCNT_MASK		0x78
  #define SPI0_RETRYCNT_INIT		0x18
  #define SPI0_SPEC_REV_MASK		0x06
  #define SPI0_SPEC_REV_2P0		0x02
  #define SPI0_IN_GOOD_CRC		0x01

#define SW_PD_INFO_1		0x0104
  #define SPI1_PR_SOP_SRC		0x01

#define SW_PD_INFO_2		0x0105
  #define SPI2_DR_SOP_SRC		0x01

#define SW_USBPD_TX_PKT_CTRL_0	0x0106
  #define SUTPC0_TX_REQ			0x80

#define SW_USBPD_TX_PKT_CTRL_1	0x0107
  #define SUTPC1_SOP_TYPE_MASK		0x70
  #define SUTPC1_INC_MSGID		0x04
  #define SUTPC1_WR_BUF_DONE		0x02
  #define SUTPC1_ABORT_TX		0x01

#define SW_USBPD_TX_PKT_STAT	0x010B
  #define SUTPS_TX_ERROR		0x02
  #define SUTPS_TX_BUF_AVAL		0x01

#define SW_USBPD_RX_PKT_CTRL_0	0x010C
  #define SURPC0_BUF1_DONE		0x02
  #define SURPC0_BUF0_DONE		0x01

#define SW_USBPD_RX_PKT_CTRL_1	0x010D
  #define SURPC1_SOP			0x01

#define SW_USBPD_RX_MSG_G_STAT	0x010F
  #define SURMGS_LEN_MASK		0x70
  #define SURMGS_INDEX_MASK		0x03
  #define SURMGS_INDEX_1		0x02

#define SW_USBPS_RX0_PKT_STAT_0	0x0110
#define USBPD_MSG_TX_BUF_PL_0	0x0144
#define USBPD_MSG_RX_BUF0_HDR_0	0x0181

#define PWR_SUPPLY_STAT_0	0x0240
  #define PSS0_VSYS_PRESENT		0x01

#define PWR_SUPPLY_STAT_1	0x0241
  #define PSS1_VBUS_PRESENT		0x04
  #define PSS1_VBUS_EQ_VSAFE5		0x02
  #define PSS1_VBUS_EQ_VSAFE0		0x01

#define FET_CTRL_0		0x0242
  #define FC0_USBFET1_MASK		0x60
  #define FC0_USBFET2_MASK		0x18
  #define FC0_USBSRC_MASK		0x06

#define CONTROL_SM_1		0x0280
  #define CSM1_VBUS_DISCHRG_EN		0x30
  #define CSM1_VBUS_DISCHRG_DIS		0x20

#define CONTROL_VCONN_COMM	0x0283
  #define CVC_CC1_TO_VCONN		0x10
  #define CVC_CC2_TO_VCONN		0x08
  #define CVC_CC_COMM_MASK		0x07
  #define CVC_CC_ORIENT_CC1		0x04
  #define CVC_CC1_COMM_EN		0x02
  #define CVC_CC2_COMM_EN		0x01

#define CONTROL_RD		0x0284
  #define CRD_RD_FINE_CC1		0x08
  #define CRD_RD_FINE_CC2		0x02

#define CONTROL_RP		0x0285
  #define CRP_RP_3P0_CC1		0x20
  #define CRP_RP_1P5_CC1		0x10
  #define CRP_RP_STD_CC1		0x08
  #define CRP_RP_3P0_CC2		0x04
  #define CRP_RP_1P5_CC2		0x02
  #define CRP_RP_STD_CC2		0x01

#define CONTROL_VRD		0x0286
  #define CVRD_CC12_EN			0xC0
  #define CVRD_CC1_EN			0x80
  #define CVRD_CC2_EN			0x40
  #define CVRD_VRD_EN_MASK		0x30
  #define CVRD_VRDSTD_EN		0x20
  #define CVRD_VRD1P5_EN		0x10
  #define CVRD_CMP_EN_MASK		0x0F
  #define CVRD_STDCMP_CC1_EN		0x08
  #define CVRD_STDCMP_CC2_EN		0x04
  #define CVRD_1P5CMP_CC1_EN		0x02
  #define CVRD_1P5CMP_CC2_EN		0x01

#define CONTROL_VRA		0x0287
  #define CVRA_CC1_EN			0x80
  #define CVRA_CC2_EN			0x40
  #define CVRA_CC1_REF_1P5		0x10
  #define CVRA_CC1_REF_STD		0x08
  #define CVRA_CC2_REF_1P5		0x02
  #define CVRA_CC2_REF_STD		0x01

#define STATUS_CURSNS		0x0289
  #define SCS_CUR_MASK			0x03

#define STATUS_COMP		0x028A
  #define SC_CC1_MASK			0xF0
  #define SC_CC1_PRESENT		0x50
  #define SC_CC2_MASK			0x0F
  #define SC_CC2_PRESENT		0x05

#define CONFIG			0x028C
  #define CFG_SW_RP_OK			0x80
  #define CFG_SW_SET_SEL		0x10
  #define CFG_CC1_INVT			0x01

#define SW_TVBUSDISCHARGE_10_8	0x029D
#define SW_TVBUSDISCHARGE_7_0	0x029E
#define SW_TCCDETECTSKEW	0x029F
#define SW_CC1_DBNC_CNT_15_8	0x02A1
#define SW_CC2_DBNC_CNT_15_8	0x02A2
#define SW_CC_CS_DBNC_CNT_15_8	0x02A8

#define PWR_CTRL_0		0x0300
  #define PC0_EN_VSAFE0_COMP		0x02

#define PWR_CTRL_1		0x0301
  #define PC1_RX_REF_A0			0x0A
  #define PC1_RX_REF_B0			0x03

#define AFE_CTRL_0		0x0302
  #define AC0_TRIM_DONE			0x08
  #define AC0_RX_EN			0x01

#define AFE_CTRL_1		0x0303
  #define AC1_INIT			0x1A

#define AFE_CTRL_2		0x0304
  #define AC2_ICTRL			0x20
  #define AC2_THR_1			0x08

#define SPARE_REG_0		0x0380
  #define SR0_PWR_EN_SYS_IREF		0x02

/**
 * Read an 8-bit register from the slave at 8-bit slave address <slaveaddr>, at
 * the specified 16-bit <offset> in the slave's address space.
 */
int i2c_16read8(int port, int slave_addr, int offset, uint8_t *data);

/**
 * Write an 8-bit register to the slave at 8-bit slave address <slaveaddr>, at
 * the specified 16-bit <offset> in the slave's address space.
 */
int i2c_16write8(int port, int slave_addr, int offset, uint8_t data);





