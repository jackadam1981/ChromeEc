/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "console.h"
#include "task.h"
#include "tcpci.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_tcpc.h"
#include "usb_pd_tcpm.h"
#include "util.h"
#include "ptn5100.h"

#define PTN5100_I2C_BASE_ADDR	0xe0

/* Short Macro for register access */
#define I2C_ADDR_TCPC(p) (PTN5100_I2C_BASE_ADDR + (p<<2))
#define RR(o, a) i2c_16read8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port), (o), (a))
#define RW(o, v) i2c_16write8(I2C_PORT_TCPC, I2C_ADDR_TCPC(port), (o), (v))

#define MAX_RETRY_CNT		0x10

/*
 * MAX_DISCHARGE_POLL * DISCHARGE_WAIT_USEC <= 200ms (approx).
 * As NXP guys requested that the max discharge time shouldn't exceed 200ms
 * otherwise Victoria chip can be damaged by high junction temperature
 */
#define MAX_DISCHARGE_POLL	0x180
#define DISCHARGE_WAIT_USEC	0x32

#define ENABLE_I2C_BURST_TFR 	0x8000

/* Start/Stop Rd/Rp toggling event */
#define DRP_TG_EVENT_START	(1<<2)
#define DRP_TG_EVENT_CONNECT	(1<<3)

/* For Rd/Rp toggling task */
/* Only support one port at this moment */
#define PTN_PORT_TO_TASK_ID(port) (TASK_ID_PTN_DRP_TG_C0)
#define PTN_TASK_ID_TO_PORT(id)   ((id) == TASK_ID_PTN_DRP_TG_C0 ? 0 : 1)

/* Port status */
#define PTN_PS_VBUS_ON          0x0008
#define PTN_PS_DRP_TG_MASK      0x0006
#define PTN_PS_DRP_TG_STOPPED   0x0004
#define PTN_PS_DRP_TOGGLING     0x0002
#define PTN_PS_CURROLE_UFP      0x0001

/* Alert */
#define PTN_IBI_ALERT_CC_ST     0x0001

/* Role control */
#define ROLE_CTRL_DRP_MASK      0xC0
#define ROLE_CTRL_RP_VAL_MASK   0x30
#define ROLE_CTRL_CC2_RD        0x08
#define ROLE_CTRL_CC2_RP        0x04
#define ROLE_CTRL_CC1_RD        0x02
#define ROLE_CTRL_CC1_RP        0x01
#define DATA_ROLE_SOURCE        0x08
#define POWER_ROLE_SOURCE       0x01

/* cc status */
#define CC_ST_TOGGLING          0x20
#define CC_ST_TOGGLE_RSLT_RD    0x10

/* Receive detect */
#define RX_DET_ENABLE_CAB_RST   0x40
#define RX_DET_ENABLE_HRST      0x20
#define RX_DET_ENABLE_ALL_SOP   0x0F
#define RX_DET_ENABLE_SOP       0x01

/* Command */
#define CMD_START_DRP_TG_RP     0x22
#define CMD_START_DRP_TG_RD     0x33

/* Message header */
#define TX_SOP_TYPE_MASK        0x07
#define TX_SOP_TYPE_SOP         0x00
#define TX_SOP_TYPE_HRST        0x05
#define TX_SOP_TYPE_CAB_RST     0x06

typedef struct _ptn5100_dev_ext {
	/* Current voltage required, only support 0=5V, 1=15V */
	uint16_t pdo_idx;
	uint16_t port_st;
	/* USB 3.1 Type C Inter-block Interface shadowing register, Rev 0.63 */
	int alert;
	int alert_mask;
	int role_control;
	int cc_status;
	int receive_detect;
	int pwr_st;
	int pwr_st_mask;
	int pwr_ctrl;
	int command;
	int msg_hdr;
} ptn5100_dev_ext;

static ptn5100_dev_ext dev_ext[CONFIG_USB_PD_PORT_COUNT];

/*
 * Read data from I2C slave with 16 bit offset address
 */
int i2c_16read8(int port, int slave_addr, int offset, uint8_t *data)
{
        int rv;
        /* We use buf[1] here so it's aligned for DMA on STM32 */
        uint8_t buf[1];
        uint16_t reg;

        reg = (offset & 0x00ff) << 8;
        reg |= (offset & 0xff00) >> 8;

        i2c_lock(port, 1);
        rv = i2c_xfer(port, slave_addr, (uint8_t *)&reg, 2, buf, 1, I2C_XFER_SINGLE);
        i2c_lock(port, 0);

        if (!rv)
                *data = buf[0];

        return rv;
}

/*
 * Write data to I2C slave with 16 bit offset address
 */
int i2c_16write8(int port, int slave_addr, int offset, uint8_t data)
{
        int rv;
        uint8_t buf[3];

        buf[0] = (offset & 0xff00) >> 8;
        buf[1] = (offset & 0x00ff);
        buf[2] = data;

        i2c_lock(port, 1);
        rv = i2c_xfer(port, slave_addr, buf, 3, 0, 0, I2C_XFER_SINGLE);
        i2c_lock(port, 0);

        return rv;
}

/*
 * Enable VCONN output on CC1/2 or disable all
 */
static int ptn_set_vconn(int port, int enable)
{
	int rv;
	uint8_t vconn_comm;

	rv  = RR(CONTROL_VCONN_COMM, &vconn_comm);

	if (rv == EC_SUCCESS) {
		/* For safty, disable any VCONN outputs first */
		vconn_comm &= ~(CVC_CC1_TO_VCONN|CVC_CC2_TO_VCONN);

		if (enable) {
			if (TCPC_REG_TCPC_CTRL_POLARITY(dev_ext[port].pwr_ctrl)) {
				/*
			 	 * CC communication is on CC2,
				 * let's enable VCONN to CC1.
				 */
				vconn_comm |= CVC_CC1_TO_VCONN;
			} else {
				vconn_comm |= CVC_CC2_TO_VCONN;
			}
		} else {
			/* Disable VCONN */
			vconn_comm &= ~(CVC_CC1_TO_VCONN|CVC_CC2_TO_VCONN);
		}

		rv |= RW(CONTROL_VCONN_COMM, vconn_comm);
	}
	return rv;
}

/*
 * Set orientation of CC and enable communication on it
 */
static int ptn_set_pwr_ctrl(int port, int pc)
{
	int rv;
	uint8_t vconn_comm;

	rv  = RR(CONTROL_VCONN_COMM, &vconn_comm);

	if (rv == EC_SUCCESS) {
		vconn_comm &= ~CVC_CC_COMM_MASK;
		if (TCPC_REG_TCPC_CTRL_POLARITY(pc)) {
			/* Enable TX on CC2 pin */
			vconn_comm |= CVC_CC2_COMM_EN;
		}
		else {
			/* Enable Tx on CC1 pin */
			vconn_comm |= CVC_CC1_COMM_EN | CVC_CC_ORIENT_CC1;
		}
		rv |= RW(CONTROL_VCONN_COMM, vconn_comm);
		dev_ext[port].pwr_ctrl = pc;
	}
	return rv;
}

static int ptn_set_msg_hdr(int port, int hdr)
{
	int rv;
	uint8_t sw_hdr_info;

	rv  = RR(SW_PD_INFO_0, &sw_hdr_info);

	 if (rv == EC_SUCCESS) {
		/* Set Spec Rev ID */
		sw_hdr_info &= ~SPI0_SPEC_REV_MASK;
		sw_hdr_info |= (hdr & SPI0_SPEC_REV_MASK);
		rv |= RW(SW_PD_INFO_0, sw_hdr_info);

		/* SOP only data role? */
		sw_hdr_info = 0;
		if ((hdr & DATA_ROLE_SOURCE) == DATA_ROLE_SOURCE) {
			/* Source */
			sw_hdr_info |= SPI2_DR_SOP_SRC;
		} else {
			/* Sink */
			sw_hdr_info &= ~SPI2_DR_SOP_SRC;
		}
		rv |= RW(SW_PD_INFO_2, sw_hdr_info);

		/* Power role */
		sw_hdr_info = 0;
		if ((hdr & POWER_ROLE_SOURCE) == POWER_ROLE_SOURCE) {
			/* Source */
			sw_hdr_info |= SPI1_PR_SOP_SRC;
		} else {
			/* Sink */
			sw_hdr_info &= ~SPI1_PR_SOP_SRC;
		}
		rv |= RW(SW_PD_INFO_1, sw_hdr_info);

		dev_ext[port].msg_hdr = hdr;
	}
	return rv;
}

static int ptn_tx_sop(int port, enum tcpm_transmit_type type, uint16_t header,
		   const uint32_t *data)
{
	int reg, i;
	uint8_t ctrl_reg, st_reg;
	int rv, cnt = PD_HEADER_CNT(header);

	/*  Address of first byte of tx payload */
	reg = USBPD_MSG_TX_BUF_PL_0;

	/*  Clear manually generate TX_DONE IRQ option */
	rv  = RR(INT_MANUAL1, &st_reg);
	st_reg &= ~IRQ1_TX_DONE;
	rv |= RW(INT_MANUAL1, st_reg);

	/*  Set SOP type, clear msg data payload write done */
	rv |= RR(SW_USBPD_TX_PKT_CTRL_1, &ctrl_reg);
	ctrl_reg &= ~(SUTPC1_SOP_TYPE_MASK|SUTPC1_WR_BUF_DONE);
	ctrl_reg |= ((type & TX_SOP_TYPE_MASK) << 4);
	rv |= RW(SW_USBPD_TX_PKT_CTRL_1, ctrl_reg);

	ctrl_reg = cnt;
	/*  Set Msg Type */
	ctrl_reg |= PD_HEADER_TYPE(header) << 3;
	rv |= RW(SW_USBPD_TX_PKT_CTRL_0, ctrl_reg);
	/*  Toggle TX req */
	ctrl_reg |= SUTPC0_TX_REQ;
	rv |= RW(SW_USBPD_TX_PKT_CTRL_0, ctrl_reg);
	ctrl_reg &= ~SUTPC0_TX_REQ;
	rv |= RW(SW_USBPD_TX_PKT_CTRL_0, ctrl_reg);

	/* If i2c read fails, return error */
	if (rv)
		return rv;

	/*  Convert to bytes */
	cnt = 4*PD_HEADER_CNT(header);

	if (cnt)
	{
		i = 0;
		/* Wait HW buffer ready */
		do {
			rv |= RR(SW_USBPD_TX_PKT_STAT, &ctrl_reg);
			if (rv)
				return rv;

			if (ctrl_reg & SUTPS_TX_ERROR) {
				/* TX ERROR detected */
				return EC_ERROR_UNKNOWN;
			}

		} while(((ctrl_reg & SUTPS_TX_BUF_AVAL) == 0) && (i++ < MAX_RETRY_CNT)); 

		if (i >= MAX_RETRY_CNT)
			return EC_ERROR_TIMEOUT;

		for (i = 0; i < cnt; i++, reg++) {
			rv |= RW(reg, ((uint8_t *)data)[i]);
		}

		rv |= RR(SW_USBPD_TX_PKT_CTRL_1, &ctrl_reg);
		ctrl_reg |= SUTPC1_WR_BUF_DONE; /* Write msg buf done */
		rv |= RW(SW_USBPD_TX_PKT_CTRL_1, ctrl_reg);
		ctrl_reg &= ~SUTPC1_WR_BUF_DONE; /* Toggle this bit to confirm */
		rv |= RW(SW_USBPD_TX_PKT_CTRL_1, ctrl_reg);
	}

	return rv;
}

static int ptn_tx(int port, enum tcpm_transmit_type type, uint16_t header,
		   const uint32_t *data)
{
	uint8_t ctrl_reg, st_reg;
	int rv, i;

	if ((type & TX_SOP_TYPE_MASK) <= 4) {
		/* SOP* */
		return ptn_tx_sop(port, type, header, data);
	} else {
		/* Hard/Cable Reset or BIST */
		rv |= RR(SW_PROT_RST_STAT_CTRL, &ctrl_reg);
		if ((type & TX_SOP_TYPE_MASK) == TX_SOP_TYPE_HRST) {
			/* Hard Reset */
			ctrl_reg |= SPRSC_SEND_HRST;
		}
		else if ((type & TX_SOP_TYPE_MASK) == TX_SOP_TYPE_CAB_RST) {
			/* Cable Reset */
			ctrl_reg |= SPRSC_SEND_HRST|SPRSC_HRST_TYPE_CABLE;
		}

		/* Sending start */
		rv |= RW(SW_PROT_RST_STAT_CTRL, ctrl_reg);

		i = 0;
		/* Wait until it finished sending */
		do {
			rv |= RR(SW_PROT_STATUS_0, &st_reg);
			if (rv)
				return rv;

		} while (((st_reg & SPST0_HRST_SENT) == 0x00) && (i++ < MAX_RETRY_CNT));

		if (i >= MAX_RETRY_CNT)
			return EC_ERROR_TIMEOUT;

		/* Clear Send bit */
		ctrl_reg &= ~SPRSC_SEND_HRST;
		rv |= RW(SW_PROT_RST_STAT_CTRL, ctrl_reg);

		rv |= RR(INT_MANUAL1, &st_reg);
		st_reg |= IRQ1_TX_DONE;
		rv |= RW(INT_MANUAL1, st_reg);

		rv |= RR(INT_STATUS1, &st_reg);
		st_reg |= IRQ1_TX_DONE;
		rv |= RW(INT_STATUS1, st_reg);

	}

	return rv;
}

static int ptn_rx(int port, uint32_t *payload, int *head)
{
	int rv, cnt;
	int reg_header, reg_data, reg_cnt;
	uint8_t	stat_reg, hdr_low, hdr_high, data;

	reg_header = USBPD_MSG_RX_BUF0_HDR_0;
	reg_cnt = SW_USBPS_RX0_PKT_STAT_0;

	/*
	 * Since we have enabled 2 RX buffer mode, we'd like to check where 
	 * the pending msg stored first
	 */
	rv  = RR(SW_USBPD_RX_MSG_G_STAT, &stat_reg);
	if ((stat_reg & SURMGS_INDEX_MASK) == SURMGS_INDEX_1) {
		/* Adjust offset */
		reg_header += 0x40;
		reg_cnt += 0x03;
	}
	reg_data = reg_header + 2;

	rv |= RR(reg_cnt, &stat_reg);
	cnt = 4*((stat_reg & SURMGS_LEN_MASK) >> 4);

	rv |= RR(reg_header,     &hdr_low);
	rv |= RR(reg_header + 1, &hdr_high);
	*head = hdr_high;
	*head = *head << 8;
	*head |= hdr_low;

	/* If i2c read fails, return error */
	if (rv)
		return rv;

	if (cnt > 0) {
		/* Set burst read flag */
		reg_data |= ENABLE_I2C_BURST_TFR; 
		/* MSB */
		data = (reg_data >> 8) & 0x00FF;
		reg_data = reg_data << 8;
		reg_data |= data;
		i2c_lock(I2C_PORT_TCPC, 1);
		rv = i2c_xfer(I2C_PORT_TCPC, I2C_ADDR_TCPC(port),
			      (uint8_t *)&reg_data, 2, (uint8_t *)payload,
			      cnt, I2C_XFER_SINGLE);
		i2c_lock(I2C_PORT_TCPC, 0);
	}

	/* Send GoodCRC now and Confirm next Rx */
	rv |= RR(SW_USBPD_RX_PKT_CTRL_0, &stat_reg);
	/* Confirm read, so HW can resume reception */
	stat_reg |= SURPC0_BUF1_DONE|SURPC0_BUF0_DONE;
	rv |= RW(SW_USBPD_RX_PKT_CTRL_0, stat_reg);
	/* Note:Toggle to confirm */
	stat_reg &= ~(SURPC0_BUF1_DONE|SURPC0_BUF0_DONE);
	rv |= RW(SW_USBPD_RX_PKT_CTRL_0, stat_reg);

	return rv;
}

static int ptn_set_rx_enable(int port, int enable)
{
	int rv = 0;
	uint8_t hrd_rst_rx, sop_rx;

	/* Hard reset rx control */
	rv = RR(SW_PROT_RST_STAT_CTRL, &hrd_rst_rx);

	if (enable & RX_DET_ENABLE_CAB_RST) {
		/* Cable Rst */
		hrd_rst_rx |= SPRSC_CAB_RST_ALLOW;
	} else {
		hrd_rst_rx &= ~SPRSC_CAB_RST_ALLOW;
	}

	if (enable & RX_DET_ENABLE_HRST) {
		/* Hard Rst */
		hrd_rst_rx |= SPRSC_HRST_ALLOW;
	} else {
		hrd_rst_rx &= ~SPRSC_HRST_ALLOW;
	}

	/* Any SOP RX need to be enabled */
	if (enable & RX_DET_ENABLE_ALL_SOP) {
		hrd_rst_rx |= SPRSC_RX_EN;
	} else {
		hrd_rst_rx &= ~SPRSC_RX_EN;
	}
	rv |= RW(SW_PROT_RST_STAT_CTRL, hrd_rst_rx);

	/* SOP RX control */
	sop_rx = 0x00;
	if (enable & RX_DET_ENABLE_SOP) {
		/* SOP */
		sop_rx |= SURPC1_SOP;
	} else {
		sop_rx &= ~SURPC1_SOP;
	}
	rv |= RW(SW_USBPD_RX_PKT_CTRL_1, sop_rx);

	dev_ext[port].receive_detect = enable;

	return rv;
}

static int ptn_enable_rp(int port)
{
	int rv;
	uint8_t reg;

	/* Disable any VRD detection */
	rv  = RW(CONTROL_VRD, 0x00);
	rv |= RW(CONTROL_VRA, 0x00);

	/* Enable Rp pull-up on CC1/CC2, then disable Rd pull-down */
	rv |= RW(CONTROL_RP, CRP_RP_1P5_CC1|CRP_RP_1P5_CC2);
	rv |= RW(CONTROL_RD, 0x00);

	/* Enable VRD detection again */
	reg = CVRD_CC12_EN|CVRD_VRD1P5_EN|CVRD_1P5CMP_CC1_EN|CVRD_1P5CMP_CC2_EN;
	rv |= RW(CONTROL_VRD, reg);
	/* Enable vra detection again */
	reg = CVRA_CC1_EN|CVRA_CC2_EN|CVRA_CC1_REF_1P5|CVRA_CC2_REF_1P5;
	rv |= RW(CONTROL_VRA, reg);

	return rv;
}

static int ptn_enable_rd(int port)
{
	int rv;
	uint8_t reg;

	/* Disable any Vrd/Vra detection */
	rv  = RW(CONTROL_VRD, 0x00);
	rv |= RW(CONTROL_VRA, 0x00);

	/* Enable Rd pull-down, then disable Rp pull-up */
	rv |= RW(CONTROL_RD, CRD_RD_FINE_CC1|CRD_RD_FINE_CC2);
	rv |= RW(CONTROL_RP, 0x00);

	/* Enable vrd detection as well as current sense */
	/* but hold to set which CC we would compare to */
	reg = CVRD_CC12_EN|CVRD_VRDSTD_EN|CVRD_VRD1P5_EN;
	rv |= RW(CONTROL_VRD, reg);
	/* Enable vra detection again */
	reg = CVRA_CC1_EN|CVRA_CC2_EN|CVRA_CC1_REF_STD|CVRA_CC2_REF_STD;
	rv |= RW(CONTROL_VRA, reg);

	return rv;
}

static int ptn_set_cc (int port, int role)
{
	int rv = EC_SUCCESS;

	dev_ext[port].role_control = role;

	if ((dev_ext[port].role_control & (ROLE_CTRL_CC1_RD|ROLE_CTRL_CC1_RP)) == ROLE_CTRL_CC1_RP) {
		/* DFP */
		dev_ext[port].port_st &= ~PTN_PS_CURROLE_UFP;
		dev_ext[port].port_st &= ~PTN_PS_DRP_TG_MASK;
		return ptn_enable_rp(port);
	}
	else if ((dev_ext[port].role_control & (ROLE_CTRL_CC1_RD|ROLE_CTRL_CC1_RP)) == ROLE_CTRL_CC1_RD) {
		/* UFP */
		dev_ext[port].port_st |= PTN_PS_CURROLE_UFP;
		dev_ext[port].port_st &= ~PTN_PS_DRP_TG_MASK;
		return ptn_enable_rd(port);
	}
	else if (dev_ext[port].role_control & ROLE_CTRL_DRP_MASK) {
		/* Disable any Vrd/Vra detection */
		rv  = RW(CONTROL_VRD, 0x00);
		rv |= RW(CONTROL_VRA, 0x00);
		/* DRP: Just keep role_control register value */
		/* we'll start toggling after COMMAND register updated */
	}

	return rv;
}

static int ptn_get_cc (int port, int *cc_status)
{
	int rv;
	uint8_t cc_st, csens;

	rv  = RR(STATUS_COMP, &cc_st);

	/* If i2c read fails, return error */
	if (rv)
		return rv;

	/* Default is open */
	dev_ext[port].cc_status = 0;

	if (dev_ext[port].port_st & PTN_PS_CURROLE_UFP) {
		/* UFP */
		if ((cc_st & SC_CC1_MASK) == SC_CC1_PRESENT) {
			/* CC1 */
			dev_ext[port].cc_status |= TYPEC_CC_VOLT_RD;
			dev_ext[port].cc_status <<= 0x00;

			rv = RR(STATUS_CURSNS, &csens);
			csens &= SCS_CUR_MASK;
			if (csens)
				csens--;
			dev_ext[port].cc_status |= csens << 4;
		} 
		else if ((cc_st & SC_CC2_MASK) == SC_CC2_PRESENT) {
			/* CC2 */
			dev_ext[port].cc_status |= TYPEC_CC_VOLT_RD;
			dev_ext[port].cc_status <<= 0x02;

			rv = RR(STATUS_CURSNS, &csens);
			csens &= SCS_CUR_MASK;
			if (csens)
				csens--;
			dev_ext[port].cc_status |= csens << 4;
		}
	} else {
		/*  DFP */
		if ((cc_st & SC_CC1_MASK) == SC_CC1_PRESENT) {
			/* CC1 is Rd */
			dev_ext[port].cc_status |= TYPEC_CC_VOLT_RD;
			dev_ext[port].cc_status <<= 0x00;
		}
		else if ((cc_st & SC_CC2_MASK) == SC_CC2_PRESENT) {
			/* CC2 is Rd */
			dev_ext[port].cc_status |= TYPEC_CC_VOLT_RD;
			dev_ext[port].cc_status <<= 0x02;
		}
	}

	if (dev_ext[port].port_st & PTN_PS_DRP_TOGGLING)
		dev_ext[port].cc_status |= CC_ST_TOGGLING;

	if (dev_ext[port].port_st & PTN_PS_DRP_TG_STOPPED) {
		if (dev_ext[port].port_st & PTN_PS_CURROLE_UFP) {
			dev_ext[port].cc_status |= CC_ST_TOGGLE_RSLT_RD;
		} else {
			dev_ext[port].cc_status &= ~CC_ST_TOGGLE_RSLT_RD;
		}
	}

	*cc_status = dev_ext[port].cc_status;

	return rv;
}

/* Call back function for alert IRQ */
static int ptn_get_alert (int port, int *alert)
{
	int rv = 0;
	uint8_t irq_st0, irq_st1, irq_st2, tx_stat, cc_st, reg, flag = 0;

	*alert = 0;

	/* Get IRQ status */
	rv |= RR(INT_STATUS0, &irq_st0);
	rv |= RR(INT_STATUS1, &irq_st1);
	rv |= RR(INT_STATUS2, &irq_st2);

	/*
	 * The PD protocol layer will process all alert bits
	 * returned by this function. Therefore, these bits
	 * can now be cleared from the TCPC register.
	 */
	if (irq_st0)
		rv |= RW(INT_STATUS0, irq_st0);
	if (irq_st1)
		rv |= RW(INT_STATUS1, irq_st1);
	if (irq_st2)
		rv |= RW(INT_STATUS2, irq_st2);

	/* CC status changed */
	if ((irq_st0 & IRQ0_CC2_STB) || (irq_st1 & IRQ1_CC1_STB)) {
		/* 
		 * Once CC pin stable and we are configured to UFP, enable
		 * current sense
		 */
		if (dev_ext[port].port_st & PTN_PS_CURROLE_UFP) {
			/* UFP */
			rv |= RR(STATUS_COMP, &cc_st);
			rv |= RR(CONTROL_VRD, &reg);

			reg &= ~CVRD_CMP_EN_MASK;

			if ((cc_st & SC_CC1_MASK) == SC_CC1_PRESENT) {
				/* CC1 */
				reg |= CVRD_STDCMP_CC1_EN|CVRD_1P5CMP_CC1_EN;
			}
			if ((cc_st & SC_CC2_MASK) == SC_CC2_PRESENT) {
				/* CC2 */
				reg |= CVRD_STDCMP_CC2_EN|CVRD_1P5CMP_CC2_EN;
			}

			rv |= RW(CONTROL_VRD, reg);
		} else {
			/* DFP */
			/* Just disable current sense */
			rv |= RR(CONTROL_VRD, &reg);
			reg &= ~CVRD_CMP_EN_MASK;
			rv |= RW(CONTROL_VRD, reg);
		}

		*alert |= TCPC_REG_ALERT_CC_STATUS;

		/*
		 * For any CC Status changing, if DRP toggling is started,
		 * we'd like to notifiy that task to stop toggling
		 */
		if (dev_ext[port].port_st & PTN_PS_DRP_TOGGLING) {
			task_set_event(PTN_PORT_TO_TASK_ID(port),
				DRP_TG_EVENT_CONNECT, 0);
		}
	}

	/* Hard Reset Received */
	if (irq_st1 & IRQ1_HRST_RX) {
		*alert |= TCPC_REG_ALERT_RX_HARD_RST;

		/*
		 * Note:Recevied Hard Reset will put our PHY/PROT module into
		 * Reset status. We'd like to bring them out and init register
		 * accordingly again
		 */
		rv |= RW(PMU_RESET_CTRL, PRC_SW_RESET);
		rv |= RW(PMU_RESET_CTRL, 0x00);

		RW(SW_USBPD_TX_PKT_CTRL_1, SUTPC1_INC_MSGID);
		ptn_set_msg_hdr(port, dev_ext[port].msg_hdr);
		ptn_set_pwr_ctrl(port, dev_ext[port].pwr_ctrl);
		ptn_set_rx_enable(port, dev_ext[port].receive_detect);
	}

	/* SOP* Message Received */
	if (irq_st1 & IRQ1_RX_AVAILABLE)
		*alert |= TCPC_REG_ALERT_RX_STATUS;

	/* Soft reset message received */
	if (irq_st1 & IRQ1_SRST_RX)
		*alert |= TCPC_REG_ALERT_RX_STATUS;

	/* TX done */
	if (irq_st1 & IRQ1_TX_DONE) {
		rv |= RR(SW_USBPD_TX_PKT_STAT, &tx_stat);
		if (tx_stat & SUTPS_TX_ERROR)
			*alert |= TCPC_REG_ALERT_TX_FAILED;
		else
			*alert |= TCPC_REG_ALERT_TX_SUCCESS;
	}

	/* Update power supply status if needed */
	if (irq_st0 & IRQ0_PS_ST_CHG) {
		/* VBUS status changed */
		if (RR(PWR_SUPPLY_STAT_1, &reg) == EC_SUCCESS) {
			if (reg & PSS1_VBUS_PRESENT) {
				dev_ext[port].port_st |= PTN_PS_VBUS_ON;
			} else {
				dev_ext[port].port_st &= ~PTN_PS_VBUS_ON;
			}
		} else {
			/*
			 * Once I2C error detected, we thought PTN5100 is
			 * losting it's power, reset VBUS status.
			 */
			dev_ext[port].port_st &= ~PTN_PS_VBUS_ON;
		}

		if ((dev_ext[port].pwr_st & TCPC_REG_POWER_STATUS_VBUS_PRES) &&
		   !(dev_ext[port].port_st & PTN_PS_VBUS_ON)) {
			/* VBUS lost */
			dev_ext[port].pwr_st &= ~TCPC_REG_POWER_STATUS_VBUS_PRES;
			if (dev_ext[port].pwr_st_mask &
				TCPC_REG_POWER_STATUS_VBUS_PRES) {
				/* Set IRQ */
				flag = 1;
			}
		}
		else if (!(dev_ext[port].pwr_st & TCPC_REG_POWER_STATUS_VBUS_PRES) &&
			  (dev_ext[port].port_st & PTN_PS_VBUS_ON)) {
			/* VBUS detect */
			dev_ext[port].pwr_st |= TCPC_REG_POWER_STATUS_VBUS_PRES;
			if (dev_ext[port].pwr_st_mask &
				TCPC_REG_POWER_STATUS_VBUS_PRES) {
				/* Set IRQ */
				flag = 1;
			}
		}

		if (flag) {
			*alert |= TCPC_REG_ALERT_POWER_STATUS;
		}
	}

	return rv;
}

static int ptn_alert_mask_set (int port, int mask)
{
	int rv;
	uint8_t irq_mask0, irq_mask1, irq_mask2;

	rv  = RR(INT_MASK0, &irq_mask0);
	rv |= RR(INT_MASK1, &irq_mask1);
	rv |= RR(INT_MASK2, &irq_mask2);

	if (mask & TCPC_REG_ALERT_CC_STATUS) {
		irq_mask0 &= ~IRQ0_CC2_STB; /* CC2 Pin stable */
		irq_mask1 &= ~IRQ1_CC1_STB; /* CC1 Pin stable */
	} else {
		irq_mask0 |= IRQ0_CC2_STB; /* CC2 Pin stable */
		irq_mask1 |= IRQ1_CC1_STB; /* CC1 Pin stable */
	}

	if (mask & TCPC_REG_ALERT_RX_HARD_RST) {
		irq_mask1 &= ~IRQ1_HRST_RX;
	} else {
		irq_mask1 |= IRQ1_HRST_RX;
	}

	if (mask & TCPC_REG_ALERT_RX_STATUS) {
		irq_mask1 &= ~(IRQ1_RX_AVAILABLE|IRQ1_SRST_RX);
	} else {
		irq_mask1 |= IRQ1_RX_AVAILABLE|IRQ1_SRST_RX;
	}

	if (mask & TCPC_REG_ALERT_TX_COMPLETE) {
		irq_mask1 &= ~IRQ1_TX_DONE;
	} else {
		irq_mask1 |= IRQ1_TX_DONE;
	}

	irq_mask2 = 0xFF;

	/*
	 * Enable power status change IRQ, so we can know what's happend on VBUS
	 */
#ifdef CONFIG_USB_PD_TCPM_VBUS
	if (mask & TCPC_REG_ALERT_POWER_STATUS) {
#endif
		irq_mask0 &= ~IRQ0_PS_ST_CHG;
#ifdef CONFIG_USB_PD_TCPM_VBUS
	}
#endif

	rv |= RW(INT_MASK0, irq_mask0);
	rv |= RW(INT_MASK1, irq_mask1);
	rv |= RW(INT_MASK2, irq_mask2);

	dev_ext[port].alert_mask = mask;

	return rv;
}

/*
 * OS task for Rd/Rp toggling when put into DRP mode
 */
void ptn_drp_task(void)
{
	int evt, rv;
	int port = PTN_TASK_ID_TO_PORT(task_get_current());

	while(1) {
		/* waiting for start */
		evt = task_wait_event_mask(DRP_TG_EVENT_START, 1000);

		if ((evt & DRP_TG_EVENT_START) != DRP_TG_EVENT_START)
			continue;

		dev_ext[port].port_st &= ~PTN_PS_DRP_TG_STOPPED;
		dev_ext[port].port_st |= PTN_PS_DRP_TOGGLING;

		/* Init role */
		if (dev_ext[port].command == CMD_START_DRP_TG_RD)
			dev_ext[port].port_st |= PTN_PS_CURROLE_UFP;
		else
			dev_ext[port].port_st &= ~PTN_PS_CURROLE_UFP;

		/* Loop for toggling */
		while(1) {
			if (dev_ext[port].port_st & PTN_PS_CURROLE_UFP) {
				/* Rd */
				rv = ptn_enable_rd(port);
			} else {
				/* Rp */
				rv = ptn_enable_rp(port);
			}

			/* Quit toggle Rd/Rp if any I2C error */
			if (rv)
				break;

			/* Toggle next Role */
			dev_ext[port].port_st ^= PTN_PS_CURROLE_UFP;

			/* Wait event of any connection */
			if (evt & TASK_EVENT_TIMER)
				continue;

			if (evt & DRP_TG_EVENT_CONNECT) {
				dev_ext[port].port_st &= ~PTN_PS_DRP_TOGGLING;
				dev_ext[port].port_st |= PTN_PS_DRP_TG_STOPPED;
				break;
			}
		}
	}
}

/*
 * Close/Open usbfet1 pin
 */
int ptn_close_usbfet1(int port, int close)
{
	int rv;
	uint8_t reg_v;

	rv  = RR(FET_CTRL_0, &reg_v);
	if (close) {
		reg_v |= FC0_USBFET1_MASK;
		rv |= RW(FET_CTRL_0, reg_v);
	} else {
		reg_v &= ~FC0_USBFET1_MASK;
		rv |= RW(FET_CTRL_0, reg_v);
	}

	return rv;
}

/*
 * Close/Open usbsrc pin
 */
int ptn_close_usbsrc(int port, int close)
{
	int rv;
	uint8_t reg_v;

	rv  = RR(FET_CTRL_0, &reg_v);
	if (close) {
		reg_v |= FC0_USBSRC_MASK;
		rv |= RW(FET_CTRL_0, reg_v);
	} else {
		reg_v &= ~FC0_USBSRC_MASK;
		rv |= RW(FET_CTRL_0, reg_v);
	}

	return rv;
}

/*
 * Close/Open usbfet2 pin
 */
int ptn_close_usbfet2(int port, int close)
{
	int rv;
	uint8_t reg_v;

	rv  = RR(FET_CTRL_0, &reg_v);
	if (close) {
		reg_v |= FC0_USBFET2_MASK;
		rv |= RW(FET_CTRL_0, reg_v);
	} else {
		reg_v &= ~FC0_USBFET2_MASK;
		rv |= RW(FET_CTRL_0, reg_v);
	}

	return rv;
}

/*
 * Enable or disable discharge
 */
int ptn_en_discharge(int port, int enable)
{
	if (enable) {
		/* Start VBUS discharge (discharge to ground) */
		return RW(CONTROL_SM_1, CSM1_VBUS_DISCHRG_EN);
	} else {
		return RW(CONTROL_SM_1, CSM1_VBUS_DISCHRG_DIS);
	}
}

int ptn_is_discharge_en(int port)
{
	uint8_t reg_v = 0;

	RR(CONTROL_SM_1, &reg_v);

	return ((reg_v & CSM1_VBUS_DISCHRG_EN) == CSM1_VBUS_DISCHRG_EN);
}

int ptn_wait_vsafe5(int port, int dir)
{
	int rv, i;
	uint8_t reg_v, vsafe_tgt;

	vsafe_tgt = PSS1_VBUS_EQ_VSAFE5;

	for (i = 0; i < MAX_DISCHARGE_POLL; i++) {
		usleep(DISCHARGE_WAIT_USEC);
		/* Wait until VBUS went to VSafe5v */
		rv = RR(PWR_SUPPLY_STAT_1, &reg_v);

		if (rv)
			return rv;

		if (dir) {
			/* Positive transition */
			if (reg_v & vsafe_tgt) {
				break;
			}
		} else {
			/* Negative transition */
			if (!(reg_v & vsafe_tgt)) {
				break;
			}
		}
	}

	if (i >= MAX_DISCHARGE_POLL) {
		/* Failed to discharge VBUS to vsafe0 at limited time period */
		cprintf(CC_USBPD, "Timeout waiting VBUS to reach vsafe5V.\n");
	}

	return rv;
}

/*
 * Wait until VBUS reachs vsafe 0v
 */
int ptn_wait_vsafe0(int port)
{
	int rv, i;
	uint8_t reg_v, vsafe_tgt;

	vsafe_tgt = PSS1_VBUS_EQ_VSAFE0;

	for (i = 0; i < MAX_DISCHARGE_POLL; i++) {
		usleep(DISCHARGE_WAIT_USEC);
		/* Wait until VBUS went to VSafe */
		rv = RR(PWR_SUPPLY_STAT_1, &reg_v);
		if (reg_v & vsafe_tgt) {
			break;
		}
	}

	if (i >= MAX_DISCHARGE_POLL) {
		/* Failed to discharge VBUS to vsafe0 at limited time period */
		cprintf(CC_USBPD, "Timeout waiting VBUS discharge to vsafe0V.\n");
	}

	return rv;
}

/*
 * Interface to poll vbus status
 */
int ptn_is_vbus_on(int port)
{
	return dev_ext[port].port_st & PTN_PS_VBUS_ON;
}

/* Chip init */
static void ptn_init (int port)
{
	uint8_t rev_id, reg;

	/* Init driver control blocks */
	memset((unsigned char *)&dev_ext[port], 0, sizeof(ptn5100_dev_ext));

	/* Make sure all VBUS source is off */
	ptn_close_usbfet2(port, 0);
	ptn_close_usbsrc(port, 0);

	/* 
	 * Always close usbfet1 pin, so in case of Thames-Lite board, PTN5100
	 * can get power from VBUS
	 */
	ptn_close_usbfet1(port, 1);

	RW(AFE_CTRL_0, AC0_TRIM_DONE);
	usleep(50);
	RW(AFE_CTRL_0, AC0_TRIM_DONE|AC0_RX_EN);

	/* Read HW Revision ID */
	RR(REV_ID, &rev_id);
	if (rev_id == RID_REVID_A0) {
		RW(PWR_CTRL_1, PC1_RX_REF_A0);
		RW(AFE_CTRL_1, AC1_INIT);
		RW(AFE_CTRL_2, AC2_ICTRL|0x03);
	} else {
		RW(PWR_CTRL_1, PC1_RX_REF_B0);
		RW(AFE_CTRL_1, AC1_INIT);
		RW(AFE_CTRL_2, AC2_ICTRL|AC2_THR_1|0x05);
	}

	/* Enable clock for all submodules */
	RW(PMU_CLK_CTRL0, 0x3F);
	RW(PMU_LOWPOWER, 0x00);

	RW(SW_TCCDETECTSKEW, 0x2A);
	RW(SW_USBPD_TX_PKT_CTRL_1, SUTPC1_INC_MSGID);
	RW(INT_SRC_CLKON, ISC_IRQ_CLK_ON_EN);

	RW(SW_CC1_DBNC_CNT_15_8, 0x02);
	RW(SW_CC2_DBNC_CNT_15_8, 0x02);
	/* Disable SM, disable vbus discharge circuit as well */
	RW(CONTROL_SM_1, CSM1_VBUS_DISCHRG_DIS);
	RW(CONFIG, CFG_SW_RP_OK|CFG_SW_SET_SEL|CFG_CC1_INVT);

	/* Doesn't need deboucing timer for cursns */
	RW(SW_CC_CS_DBNC_CNT_15_8, 0x00);

	/* Default port role setting for msg header */
	RW(SW_PD_INFO_0, SPI0_RETRYCNT_INIT|SPI0_SPEC_REV_2P0|SPI0_IN_GOOD_CRC);

	/* Enable discharge reference */
	RW(SPARE_REG_0, SR0_PWR_EN_SYS_IREF);
	/* Enable vsafe0 comparator */
	RW(PWR_CTRL_0, PC0_EN_VSAFE0_COMP);

	/* Update VBUS status */
	RR(PWR_SUPPLY_STAT_1, &reg);
	if (reg & PSS1_VBUS_PRESENT) {
		dev_ext[port].port_st |= PTN_PS_VBUS_ON;
		dev_ext[port].pwr_st |= TCPC_REG_POWER_STATUS_VBUS_PRES;
	} else {
		dev_ext[port].port_st &= ~PTN_PS_VBUS_ON;
		dev_ext[port].pwr_st &= ~TCPC_REG_POWER_STATUS_VBUS_PRES;
	}
}


/*
 * All Google TCPCI API functions below
 */
static int tcpc_vbus[CONFIG_USB_PD_PORT_COUNT];

static int init_alert_mask(int port)
{
	uint16_t mask;
	int rv;

	/*
	 * Create mask of alert events that will cause the TCPC to
	 * signal the TCPM via the Alert# gpio line.
	 */
	mask = TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED |
		TCPC_REG_ALERT_TX_DISCARDED | TCPC_REG_ALERT_RX_STATUS |
		TCPC_REG_ALERT_RX_HARD_RST | TCPC_REG_ALERT_CC_STATUS
#ifdef CONFIG_USB_PD_TCPM_VBUS
		| TCPC_REG_ALERT_POWER_STATUS
#endif
	;

	/* Set the alert mask in TCPC */
	rv = ptn_alert_mask_set(port, mask);
	return rv;
}

int tcpm_set_power_status_mask(int port, uint8_t mask)
{
	dev_ext[port].pwr_st_mask = mask;

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_TCPM_VBUS
static int init_power_status_mask(int port)
{
	uint8_t mask;
	int rv;

	mask = TCPC_REG_POWER_STATUS_VBUS_PRES;
	rv = tcpm_set_power_status_mask(port, mask);

	return rv;
}
#endif

int tcpm_get_cc(int port, int *cc1, int *cc2)
{
	int status;
	int rv;

	rv = ptn_get_cc(port, &status);

	/* If i2c read fails, return error */
	if (rv)
		return rv;

	*cc1 = TCPC_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_REG_CC_STATUS_CC2(status);

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */
	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= TCPC_REG_CC_STATUS_TERM(status) << 2;
	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= TCPC_REG_CC_STATUS_TERM(status) << 2;

	return rv;
}

int tcpm_get_power_status(int port, int *status)
{
	return *status = dev_ext[port].pwr_st;
}

int tcpm_set_cc(int port, int pull)
{
	int rv;

	/*
	 * Set manual control of Rp/Rd, and set both CC lines to the same
	 * pull.
	 */
	rv = ptn_set_cc(port, TCPC_REG_ROLE_CTRL_SET(0, 0, pull, pull));

	return rv;
}

int tcpm_set_polarity(int port, int polarity)
{
	int rv;
	uint8_t reg_val;

	/* Write new polarity, leave vconn enable flag untouched */
	rv = ptn_set_pwr_ctrl(port,
		TCPC_REG_TCPC_CTRL_SET(polarity));

	/* Prepare for VCONN output */
	if (TCPC_REG_TCPC_CTRL_POLARITY(polarity)) {
		/* Disable VRd/VRa detection on CC1 pin */
		rv |= RR(CONTROL_VRD, &reg_val);
		reg_val &= ~(CVRD_CC1_EN|CVRD_STDCMP_CC1_EN|CVRD_1P5CMP_CC1_EN);
		rv |= RW(CONTROL_VRD, reg_val);

		rv |= RR(CONTROL_VRA, &reg_val);
		reg_val &= ~CVRA_CC1_EN;
		rv |= RW(CONTROL_VRA, reg_val);

		/* Remove Rd on CC1 pin */
		rv |= RR(CONTROL_RD, &reg_val);
		reg_val &= ~CRD_RD_FINE_CC1;
		rv |= RW(CONTROL_RD, reg_val);

		/* Remove Rp on CC1 pin */
		rv |= RR(CONTROL_RP, &reg_val);
		reg_val &= ~(CRP_RP_3P0_CC1|CRP_RP_1P5_CC1|CRP_RP_STD_CC1);
		rv |= RW(CONTROL_RP, reg_val);
	} else {
		/* Disable VRd/VRa detection on CC2 pin */
		rv |= RR(CONTROL_VRD, &reg_val);
		reg_val &= ~(CVRD_CC2_EN|CVRD_STDCMP_CC2_EN|CVRD_1P5CMP_CC2_EN);
		rv |= RW(CONTROL_VRD, reg_val);

		rv |= RR(CONTROL_VRA, &reg_val);
		reg_val &= ~CVRA_CC2_EN;
		rv |= RW(CONTROL_VRA, reg_val);

		/* Remove Rd on CC2 pin */
		rv |= RR(CONTROL_RD, &reg_val);
		reg_val &= ~CRD_RD_FINE_CC2;
		rv |= RW(CONTROL_RD, reg_val);

		/* Remove Rp on CC2 pin */
		rv |= RR(CONTROL_RP, &reg_val);
		reg_val &= ~(CRP_RP_3P0_CC2|CRP_RP_1P5_CC2|CRP_RP_STD_CC2);
		rv |= RW(CONTROL_RP, reg_val);
	}


	return rv;
}

int tcpm_set_vconn(int port, int enable)
{
	return ptn_set_vconn(port, enable);
}

int tcpm_set_msg_header(int port, int power_role, int data_role)
{
	return ptn_set_msg_hdr(port, 
			TCPC_REG_MSG_HDR_INFO_SET(data_role, power_role));
}

int tcpm_set_rx_enable(int port, int enable)
{
	/* If enable, then set RX detect for SOP and HRST */
	return ptn_set_rx_enable(port, 
			enable ? TCPC_REG_RX_DETECT_SOP_HRST_MASK : 0);
}

#ifdef CONFIG_USB_PD_TCPM_VBUS
int tcpm_get_vbus_level(int port)
{
	return tcpc_vbus[port];
}
#endif

int tcpm_get_message(int port, uint32_t *payload, int *head)
{
	return ptn_rx(port, payload, head);
}

int tcpm_transmit(int port, enum tcpm_transmit_type type, uint16_t header,
		   const uint32_t *data)
{
	return ptn_tx(port, type, header, data);
}

void tcpc_alert(int port)
{
	int status;
	int power_status;

	/* Read the Alert register from the TCPC */
	ptn_get_alert(port, &status);

	if (status & TCPC_REG_ALERT_TX_COMPLETE) {
		/* transmit complete */
		pd_transmit_complete(port, status & TCPC_REG_ALERT_TX_SUCCESS ?
					   TCPC_TX_COMPLETE_SUCCESS :
					   TCPC_TX_COMPLETE_FAILED);
	}
	if (status & TCPC_REG_ALERT_CC_STATUS) {
		/* CC status changed, wake task */
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
	}
	if (status & TCPC_REG_ALERT_POWER_STATUS) {
		/* Read Power Status register */
		tcpm_get_power_status(port, &power_status);
		/* Update VBUS status */
		tcpc_vbus[port] = power_status &
			TCPC_REG_POWER_STATUS_VBUS_PRES ? 1 : 0;
#if defined(CONFIG_USB_PD_TCPM_VBUS) && defined(CONFIG_USB_CHARGER)
		/* Update charge manager with new VBUS state */
		usb_charger_vbus_change(port, tcpc_vbus[port]);
#endif /* CONFIG_USB_PD_TCPM_VBUS && CONFIG_USB_CHARGER */
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
	if (status & TCPC_REG_ALERT_RX_STATUS) {
		/* message received */
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_RX, 0);
	}
	if (status & TCPC_REG_ALERT_RX_HARD_RST) {
		/* hard reset received */
		pd_execute_hard_reset(port);
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
}

int tcpm_init(int port)
{
	int rv, alert = 0;


	while (1) {
		rv = RR(PMU_STATUS, (uint8_t *)&alert);

		/*
		 * If i2c succeeds and VID is non-zero, then initialization
		 * is complete
		 */
		if (rv == EC_SUCCESS && (alert & PST_POR_COMPLETE)) {
			ptn_init(port);

#ifdef CONFIG_USB_PD_TCPM_VBUS
			/* Initialize power_status_mask */
			init_power_status_mask(port);
			/* Update VBUS status */
			tcpc_vbus[port] = dev_ext[port].pwr_st &
				TCPC_REG_POWER_STATUS_VBUS_PRES ? 1 : 0;
#endif

			/* clear all alert bits */
			return init_alert_mask(port);
		}
		msleep(10);
	}
}
