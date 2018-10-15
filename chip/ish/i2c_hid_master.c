/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HID over I2C implementation */

#include "i2c_hid_master.h"

#include "console.h"
#include "i2c.h"
#include "timer.h"
#include "registers.h"
#include "util.h"
#include "hid.h"

#define DEBUG_I2CHIDM			1

#define CCPRINTF(format, args...)	ccprintf(format, ## args)

/* Defines for ELAN B50 TSC */
#if defined(HIDI2C_SLAVEDEV_TSC_ELAN_B50)

#define HIDDESC_BCDVERSION		HID_BCD_VERSION

#define HIDDESC_ADR_SLAVE		0x2A	/* 0x15 for 7 bits address */
#define HIDDESC_ADR_HIDDESC		0x0001
#define HIDDESC_ADR_RPTDESC		0x0002
#define HIDDESC_ADR_INPUTREG		0x0003
#define HIDDESC_ADR_OUTPUTREG		0x0004
#define HIDDESC_ADR_CMDREG		0x0005
#define HIDDESC_ADR_DATREG		0x0006

#define HIDDESC_ID_VENDOR		0x04F3
#define HIDDESC_ID_PRODUCT		0x30C5
#define HIDDESC_ID_VERSION		0x0003

/* Command Register Report ID (Not defined in general HID header file) */
#define HID_CMDREG_RID_HEATMAP		0x09 /* For heatmap over HID */
#define HID_CMDREG_RID_IAP		0x0B /* Not in use yet */
#define HID_CMDREG_RID_TESTMODDAT	0x0C /* Test Mode Data */
#define HID_CMDREG_RID_PT		0x0D /* For production test */

/* Command Register - Basic Commands */
/* wakeup - 0x0800 */
#define CMDREG_WAKEUP			((HID_CMDREG_OP_SETPWR \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_RSVD \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_PWRWAKE \
						<< HID_CMDREG_RID_SHIFT))
/* sleep - 0x0801 */
#define CMDREG_SLEEP			((HID_CMDREG_OP_SETPWR \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_RSVD \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_PWRSLEEP \
						<< HID_CMDREG_RID_SHIFT))
/* reset - 0x0100 */
#define CMDREG_RESET			((HID_CMDREG_OP_RESET \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_RSVD \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_NONE \
						<< HID_CMDREG_RID_SHIFT))
/* get heatmap - 0x0239 */
#define CMDREG_GETRPT_HEATMAP		((HID_CMDREG_OP_GETRPT \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_FEATURE \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_HEATMAP \
						<< HID_CMDREG_RID_SHIFT))
/* set input mode - 0x0333 */
#define CMDREG_SETRPT_INPUTMODE		((HID_CMDREG_OP_SETRPT \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_FEATURE \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_INPUTMODE \
						<< HID_CMDREG_RID_SHIFT))
/* Command Register - Production Test Commands */
/* set production test - 0x033D */
#define CMDREG_SETRPT_PT		((HID_CMDREG_OP_SETRPT \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_FEATURE \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_PT \
						<< HID_CMDREG_RID_SHIFT))
/* get production test - 0x023D */
#define CMDREG_GETRPT_PT		((HID_CMDREG_OP_GETRPT \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_FEATURE \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_PT \
						<< HID_CMDREG_RID_SHIFT))
/* get testmode data - 0x023C */
#define CMDREG_GETRPT_TESTMOD_DAT	((HID_CMDREG_OP_GETRPT \
						<< HID_CMDREG_OP_SHIFT) \
					| (HID_CMDREG_RT_FEATURE \
						<< HID_CMDREG_RT_SHIFT) \
					| (HID_CMDREG_RID_TESTMODDAT \
						<< HID_CMDREG_RID_SHIFT))

/* Command Register Extension Data - Basic Commands */
#define DATA_INPUTMOD_LNG		(2)
#define DATA_INPUTMOD_PTP		(0x0003)
#define DATA_INPUTMOD_MOUSE		(0x0000)
/* Command Register Extension Data - Production Test Commands */
#define DATA_PTSET_LNG			(4)
#define DATA_PTSET_FWID			(0x01010305)
#define DATA_PTSET_FWVER		(0x01020305)
#define DATA_PTSET_CHKSUM		(0x030F0305)
/* Command Register Extension Data - Production Test - Test Mode Commands */
#define DATA_PTSET_TESTMOD_EXIT		(0x00000322)
#define DATA_PTSET_TESTMOD_SELF_DV	(0x50010322)
#define DATA_PTSET_TESTMOD_SELF_BASE	(0x50020322)
#define DATA_PTSET_TESTMOD_SELF_RAW	(0x50030322)
#define DATA_PTSET_TESTMOD_MUTUAL_DV	(0x50040322)
#define DATA_PTSET_TESTMOD_MUTUAL_BASE	(0x50050322)
#define DATA_PTSET_TESTMOD_MUTUAL_RAW	(0x50060322)
#define DATA_PTSET_TESTMOD_OPENSHORT	(0x50070322)
#define DATA_PTSET_TESTMOD_NOAUTOIDLE	(0x50100322)
#define DATA_PTSET_DISABLE_AUTOIDLE	(0x50100322)
#define DATA_PTSET_ENABLE_AUTOIDLE	DATA_PTSET_TESTMOD_EXIT
/* Command Register - Production Test Data Format */
#define DATA_PTGET_FWID_LNG		(7)
#define DATA_PTGET_FWID_MSB		(6)
#define DATA_PTGET_FWID_LSB		(5)
#define DATA_PTGET_FWVER_LNG		(7)
#define DATA_PTGET_FWVER_MSB		(6)
#define DATA_PTGET_FWVER_LSB		(5)
#define DATA_PTGET_CHKSUM_LNG		(7)
#define DATA_PTGET_CHKSUM_MSB		(6)
#define DATA_PTGET_CHKSUM_LSB		(5)

/** 
 * Command Register Basic Format Limitation
 * ex. Name(size in bytes)
 * Set:
 *   I2C Adr W(1) + CmdReg(2) + CmdDat(2) + DatReg(2) + Lng(2) + RID(1)
 *   + ExtDat(N)
 * Get:
 *   I2C Adr W(1) + CmdReg(2) + CmdDat(2) + DatReg(2) + I2C Adr R(1)
 *   + ReadData.
 */
#define CMDREG_TXLNG_BASE		4  /* 2 b CMDREG and 2 b CMDDAT */
#define CMDREG_TXLNG_DATREG		2  /* 2 b DATREG */
#define CMDREG_TXLNG_DATHDR		3  /* 2 b length and 1 b RID */
#define CMDREG_TXLNG_DATMAX		7  /* N b extension data */
#define CMDREG_TXLNG_BUFMAX		(CMDREG_TXLNG_BASE \
					+ CMDREG_TXLNG_DATREG \
					+ CMDREG_TXLNG_DATHDR \
					+ CMDREG_TXLNG_DATMAX)

#if (DATA_INPUTMOD_LNG > CMDREG_TXLNG_DATMAX) \
	|| (DATA_PTSET_LNG > CMDREG_TXLNG_DATMAX)
#error "Size Error - Data size is larger than data buffer"
#endif
#endif


/* Variables */
static union hid_desc {
	struct hid_descriptor 	desc;
	uint8_t			data[HIDDESC_LNG_HIDDESC];
} hid_desc_data;

static uint32_t test_mode_list[TESTMODMAX] =
{
	DATA_PTSET_TESTMOD_SELF_DV,
	DATA_PTSET_TESTMOD_SELF_BASE,
	DATA_PTSET_TESTMOD_SELF_RAW,
	DATA_PTSET_TESTMOD_MUTUAL_DV,
	DATA_PTSET_TESTMOD_MUTUAL_BASE,
	DATA_PTSET_TESTMOD_MUTUAL_RAW,
	DATA_PTSET_TESTMOD_OPENSHORT
};

/* TODO: Define structure for report desc? */
static uint8_t report_desc[HIDDESC_LNG_MAXRPTDESC];

/* Function declarations */
static int32_t send_cmd(uint16_t cmd, uint8_t *tx, uint32_t tx_lng, uint8_t *rx
			, uint32_t rx_lng);


/* -------- MIKEDBG: For testing -------- */
//#if defined(DEBUG_I2CHIDM) && DEBUG_I2CHIDM
enum i2c_hid_api_idx {
	GETHIDDESC		= 0,
	GETRPTDESC		= 1,
	GETINPUTRPT_INTP	= 2,
	GETINPUTRPT_NOINTP	= 3,
	RESET			= 4,
	SLEEP			= 5,
	WAKEUP			= 6,
	SETPTP			= 7,
	SETMOUSE		= 8,
	GETHEATMAP		= 9,
	GETFWUID		= 10,
	GETCHKSUM		= 11,
	TESTMODSTART		= 12,
	TESTMODGETDAT		= 13,
	TESTMODSTOP		= 14,
	MAXTESTIDX,
};

#define DBG_PRINT_BASE	16

#define RPT_HEATMAP_SIZ	(4 + 26*15 + 26 + 15)

static uint8_t test_buf[RPT_HEATMAP_SIZ * 2];

static void print_array(uint8_t *data, uint32_t lng, uint32_t print_base)
{
	uint32_t i;
	uint32_t base = (print_base ? print_base : DBG_PRINT_BASE);

	CCPRINTF("Length: %d", lng);
	for (i = 0; i < lng; i++) {
		if ((i % base) == 0)
			CCPRINTF("\n");
		CCPRINTF(" %02x", data[i]);
	}
	CCPRINTF("\n");
}

static int i2c_hid_api_test(int argc, char *argv[])
{
	uint32_t tmp = 0;
	int ret = EC_SUCCESS, idx, tidx = 0, lng = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	idx = strtoi(argv[1], NULL, 0);
	if ((idx == TESTMODSTART) || (idx == TESTMODGETDAT)) {
		if (argc != 3)
			return EC_ERROR_PARAM_COUNT;
		lng = tidx = strtoi(argv[2], NULL, 0);
	}
	switch (idx) {
	case GETHIDDESC:
		ret = i2c_hid_get_hid_desc();
		CCPRINTF("I2CHIDM - Desc ret=%d\n", ret);
		if (!ret)
			print_array(hid_desc_data.data, HIDDESC_LNG_HIDDESC, 0);
		break;
	case GETRPTDESC:
		ret = i2c_hid_get_rpt_desc();
		CCPRINTF("I2CHIDM - Rpt Desc ret=%d\n", ret);
		if (!ret)
			print_array(report_desc, HIDDESC_LNG_MAXRPTDESC, 0);
		break;
	case GETINPUTRPT_INTP:
		ret = i2c_hid_get_input(INTP_ACT, test_buf, RPT_HEATMAP_SIZ);
		CCPRINTF("I2CHIDM - Input Rpt ret=%d\n", ret);
		if (!ret)
                      print_array(test_buf, HIDDESC_LNG_MAXINPUT, 0);
		break;
	case GETINPUTRPT_NOINTP:
		ret = i2c_hid_get_input(INTP_NOACT, test_buf, RPT_HEATMAP_SIZ);
		CCPRINTF("I2CHIDM - Input Rpt ret=%d\n", ret);
		if (!ret)
			print_array(test_buf, HIDDESC_LNG_MAXINPUT, 0);
		break;
	case RESET:
		ret = i2c_hid_reset();
		CCPRINTF("I2CHIDM - Reset ret=%d\n", ret);
		break;
	case SLEEP:
		ret = i2c_hid_sleep();
		CCPRINTF("I2CHIDM - Sleep ret=%d\n", ret);
		break;
	case WAKEUP:
		ret = i2c_hid_wakeup();
		CCPRINTF("I2CHIDM - Wakeup ret=%d\n", ret);
		break;
	case SETPTP:
		ret = i2c_hid_set_ptp();
		CCPRINTF("I2CHIDM - SendPTP ret=%d\n", ret);
		break;
	case SETMOUSE:
		ret = i2c_hid_set_mouse();
		CCPRINTF("I2CHIDM - SendMouse ret=%d\n", ret);
		break;
	case GETHEATMAP:
		ret = i2c_hid_get_heatmap(test_buf, RPT_HEATMAP_SIZ);
		CCPRINTF("I2CHIDM - GetHeatmap ret=%d\n", ret);
		if (!ret) {
			CCPRINTF("lng=%d RID=%02x fidx=%d\n"
				, (test_buf[0] | (test_buf[1] << 8))
				, test_buf[2], test_buf[3]);
			print_array(test_buf + 4, RPT_HEATMAP_SIZ - 4 - 26 - 15
					, 15);
			print_array(test_buf + RPT_HEATMAP_SIZ - 26 - 15, 15
					, 30);
			print_array(test_buf + RPT_HEATMAP_SIZ - 26, 26
					, 30);
		}
		break;
	case GETFWUID:
		ret = i2c_hid_get_fwuid(&tmp);
		CCPRINTF("I2CHIDM - GetFWUID ret=%d uid=0x%08x\n", ret, tmp);
		break;
	case GETCHKSUM:
		tmp = 0;
		ret = i2c_hid_get_chksum((uint16_t *)(&tmp));
		CCPRINTF("I2CHIDM - GetFWUID ret=%d chksum=0x%04x\n", ret, tmp);
		break;
	case TESTMODSTART:
		ret = i2c_hid_testmode_start(tidx);
		CCPRINTF("I2CHIDM - SetTestMod Start ret=%d tidx=%d\n", ret, tidx);
		break;
	case TESTMODGETDAT:
		ret = i2c_hid_testmode_get(test_buf, lng);
		CCPRINTF("I2CHIDM - GetTestMod Data %d ret=%d lng=%d rid=0x%x\n"
				, lng
				, ret
				, test_buf[0] + (test_buf[1] << 8)
				, test_buf[2] );
		break;
	case TESTMODSTOP:
		ret = i2c_hid_testmode_stop();
		CCPRINTF("I2CHIDM - SetTestMod End ret=%d\n", ret);
		break;
	default:
		ret = EC_ERROR_UNIMPLEMENTED;
		break;
	}
		
	return ret;
}
DECLARE_CONSOLE_COMMAND(hidapi, i2c_hid_api_test, "<API Number>",
			"Test Specific HID I2C Master API");
//#endif
/* -------------------------------------- */

static int32_t send_cmd(uint16_t cmd, uint8_t *tx, uint32_t tx_lng, uint8_t *rx
			, uint32_t rx_lng)
{
	int32_t ret;
	uint32_t i, lng = CMDREG_TXLNG_BASE;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	if (tx_lng > CMDREG_TXLNG_DATMAX)
		return EC_ERROR_PARAM3;

	data[0] = (uint8_t)(HIDDESC_ADR_CMDREG & 0x00FF);
	data[1] = (uint8_t)((HIDDESC_ADR_CMDREG >> 8) & 0x00FF);
	data[2] = (uint8_t)(cmd & 0x00FF);
	data[3] = (uint8_t)((cmd >> 8) & 0x00FF);
	if ((cmd != CMDREG_WAKEUP) && (cmd != CMDREG_SLEEP)
		&& (cmd != CMDREG_RESET))
	{
		data[4] = (uint8_t)(HIDDESC_ADR_DATREG & 0x00FF);
		data[5] = (uint8_t)((HIDDESC_ADR_DATREG >> 8) & 0x00FF);
		lng += CMDREG_TXLNG_DATREG;
	}
	for (i = 0; i < tx_lng; i++) {
		data[lng + i] = tx[i];
	}
	lng += tx_lng;
	i2c_lock(I2C_PORT_TP, 1);
        ret = i2c_xfer(I2C_PORT_TP, HIDDESC_ADR_SLAVE, data, lng
				, rx, rx_lng, I2C_XFER_START_REPEAT);
	i2c_lock(I2C_PORT_TP, 0);
	return ret;
}

int32_t i2c_hid_get_hid_desc(void)
{
	int32_t ret;
	uint8_t data[2];
	
	data[0] = (uint8_t)(HIDDESC_ADR_HIDDESC & 0x00FF);
	data[1] = (uint8_t)((HIDDESC_ADR_HIDDESC >> 8) & 0x00FF);

        i2c_lock(I2C_PORT_TP, 1);
        ret = i2c_xfer(I2C_PORT_TP, HIDDESC_ADR_SLAVE, data, 2
				, hid_desc_data.data, HIDDESC_LNG_HIDDESC
				, I2C_XFER_START_REPEAT);
	i2c_lock(I2C_PORT_TP, 0);
	return ret;
}

int32_t i2c_hid_copy_hid_desc(uint8_t *buf)
{
	memcpy(buf, hid_desc_data.data, HIDDESC_LNG_HIDDESC);
	return HIDDESC_LNG_HIDDESC;
}

int32_t i2c_hid_get_rpt_desc(void)
{
        int32_t ret;
        uint8_t data[2];

        data[0] = (uint8_t)(HIDDESC_ADR_RPTDESC & 0x00FF);
        data[1] = (uint8_t)((HIDDESC_ADR_RPTDESC >> 8) & 0x00FF);
        i2c_lock(I2C_PORT_TP, 1);
        ret = i2c_xfer(I2C_PORT_TP, HIDDESC_ADR_SLAVE, data, 2
				, report_desc, HIDDESC_LNG_MAXRPTDESC
				, I2C_XFER_START_REPEAT);
	i2c_lock(I2C_PORT_TP, 0);
	return ret;
}

int32_t i2c_hid_copy_rpt_desc(uint8_t *buf)
{
	memcpy(buf, report_desc, HIDDESC_LNG_MAXRPTDESC);
	return HIDDESC_LNG_MAXRPTDESC;
}

int32_t i2c_hid_get_input(uint32_t interrupt, uint8_t *data
				, uint32_t max_lng)
{
	int32_t ret, lng;
	uint8_t tmp[2];
	
	if (max_lng < HIDDESC_LNG_MAXINPUT)
		return EC_ERROR_PARAM3;

	/* No need to specify reg adr when interrput is asserted. */
	if (interrupt) {
		tmp[0] = tmp[1] = 0;
		lng = 0;
	} else {
		tmp[0] = (uint8_t)(HIDDESC_ADR_INPUTREG & 0x00FF);
		tmp[1] = (uint8_t)((HIDDESC_ADR_INPUTREG >> 8) & 0x00FF);
		lng = 2;
	}
	i2c_lock(I2C_PORT_TP, 1);
	ret = i2c_xfer(I2C_PORT_TP, HIDDESC_ADR_SLAVE, tmp, lng
				, data, HIDDESC_LNG_MAXINPUT
				, I2C_XFER_START_REPEAT);
	i2c_lock(I2C_PORT_TP, 0);
	return ret;
}

int32_t i2c_hid_reset(void)
{
	int32_t ret;
	ret = send_cmd(CMDREG_RESET, NULL, 0, NULL, 0);
	return ret;
}

int32_t i2c_hid_sleep(void)
{
	int32_t ret;
	ret = send_cmd(CMDREG_SLEEP, NULL, 0, NULL, 0);
	return ret;
}

int32_t i2c_hid_wakeup(void)
{
	int32_t ret;
	ret = send_cmd(CMDREG_WAKEUP, NULL, 0, NULL, 0);
	return ret;
}

int32_t i2c_hid_set_ptp(void)
{
	int32_t ret;
	uint16_t i, lng = 3; /* length 2 bytes + RID 1 bytes*/
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	for (i = 0; i < DATA_INPUTMOD_LNG; i++)
		data[3 + i] = (uint8_t)((DATA_INPUTMOD_PTP >> (i * 8)) & 0xFF);
	lng += DATA_INPUTMOD_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_INPUTMODE;

	ret = send_cmd(CMDREG_SETRPT_INPUTMODE, data, lng, NULL, 0);
	return ret;
}

int32_t i2c_hid_set_mouse(void) {
	int32_t ret;
	uint16_t i, lng = 3; /* length 2 bytes + RID 1 bytes*/
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	for (i = 0; i < DATA_INPUTMOD_LNG; i++)
		data[3 + i] = (uint8_t)((DATA_INPUTMOD_MOUSE >> (i * 8)) & 0xFF);
	lng += DATA_INPUTMOD_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_INPUTMODE;

	ret = send_cmd(CMDREG_SETRPT_INPUTMODE, data, lng, NULL, 0);
	return ret;
}


int32_t i2c_hid_get_heatmap(uint8_t *data, uint32_t lng) {
	int32_t ret;
	ret = send_cmd(CMDREG_GETRPT_HEATMAP, NULL, 0, data, lng);
	return ret;
}

int32_t i2c_hid_get_fwuid(uint32_t *fwuid)
{
	int32_t ret;
	uint16_t i, lng = CMDREG_TXLNG_DATHDR;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	if (!fwuid)
		return EC_ERROR_PARAM1;

	/* Set/Get FW ID */
	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((DATA_PTSET_FWID >> (i * 8)) & 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	if (ret)
		goto get_fwuid_end;
	ret = send_cmd(CMDREG_GETRPT_PT, NULL, 0, data, DATA_PTGET_FWID_LNG);
	if (ret)
		goto get_fwuid_end;
	*fwuid = (((uint32_t)(data[DATA_PTGET_FWID_MSB]) << 8)
			+ (uint32_t)(data[DATA_PTGET_FWID_LSB])) << 16;
	/* Set/Get FW VER */
	lng = CMDREG_TXLNG_DATHDR;
	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((DATA_PTSET_FWVER >> (i * 8)) & 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	if (ret)
		goto get_fwuid_end;
	ret = send_cmd(CMDREG_GETRPT_PT, NULL, 0, data, DATA_PTGET_FWVER_LNG);
	if (ret)
		goto get_fwuid_end;
	*fwuid += ((uint32_t)(data[DATA_PTGET_FWVER_MSB]) << 8)
			+ (uint32_t)(data[DATA_PTGET_FWVER_LSB]);
get_fwuid_end:
	return ret;
}

int32_t i2c_hid_testmode_start(enum test_mode_idx testidx)
{
	int32_t ret;
	uint32_t testmodext; /* testmode command extension*/
	uint16_t i, lng = CMDREG_TXLNG_DATHDR;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	if (testidx >= TESTMODMAX)
		return EC_ERROR_PARAM1;
	testmodext = test_mode_list[testidx];
	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((testmodext >> (i * 8)) & 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	return ret;
}

int32_t i2c_hid_testmode_get(uint8_t *data, uint32_t lng)
{
	int32_t ret;
	if (!data)
		return EC_ERROR_PARAM1;
	ret = send_cmd(CMDREG_GETRPT_TESTMOD_DAT, NULL, 0, data, lng);
	return ret;
}

int32_t i2c_hid_testmode_stop(void)
{
	int32_t ret;
	uint16_t i, lng = CMDREG_TXLNG_DATHDR;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((DATA_PTSET_TESTMOD_EXIT >> (i * 8))
					& 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	return ret;
}

int32_t i2c_hid_disable_autoidle(void)
{
	int32_t ret;
	uint16_t i, lng = CMDREG_TXLNG_DATHDR;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((DATA_PTSET_DISABLE_AUTOIDLE >> (i * 8))
					& 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	return ret;
}

int32_t i2c_hid_enable_autoidle(void)
{
	int32_t ret;
	uint16_t i, lng = CMDREG_TXLNG_DATHDR;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((DATA_PTSET_ENABLE_AUTOIDLE >> (i * 8))
					& 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	return ret;
}


int32_t i2c_hid_get_chksum(uint16_t *chksum)
{
	int32_t ret;
	uint16_t i, lng = CMDREG_TXLNG_DATHDR;
	uint8_t data[CMDREG_TXLNG_BUFMAX];

	if (!chksum)
		return EC_ERROR_PARAM1;

	for (i = 0; i < DATA_PTSET_LNG; i++)
		data[CMDREG_TXLNG_DATHDR + i]
			= (uint8_t)((DATA_PTSET_CHKSUM >> (i * 8))
					& 0xFF);
	lng += DATA_PTSET_LNG;
	data[0] = (uint8_t)(lng & 0x00FF);
	data[1] = (uint8_t)((lng >> 8) & 0x00FF);
	data[2] = HID_CMDREG_RID_PT;
	/* Set Feature - ChkSum */
	ret = send_cmd(CMDREG_SETRPT_PT, data, lng, NULL, 0);
	if (ret)
		goto get_chksum_end;
	/* Get Feature - ChkSum */
	ret = send_cmd(CMDREG_GETRPT_PT, NULL, 0, data, DATA_PTGET_CHKSUM_LNG);
	if (ret)
		goto get_chksum_end;
	*chksum = ((uint16_t)data[DATA_PTGET_CHKSUM_MSB] << 8)
			+ (uint16_t)data[DATA_PTGET_CHKSUM_LSB];
get_chksum_end:
	return ret;
}

