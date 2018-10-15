/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Routines for communicating with TSC, ELAN B50 */
#include "tsc.h"

#include "gpio.h"
#include "i2c_hid_master.h"
#include "console.h"
#include "timer.h"
#include "util.h"

#define DEBUG_TSC		1

#if defined(DEBUG_TSC) && DEBUG_TSC
#define CCPRINTF(format, args...)	ccprintf(format, ## args)
#else
#define CCPRINTF(format, args...)
#endif

#define TSC_RESET_POLLING_UDELAY		(1 * MSEC)
#define TSC_RESET_POLLING_MAXCNT		(10)

/* Snapshot Defines*/
#define TSC_SNAPSHOT_UDELAY_SELF_DV		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_SELF_BASE		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_SELF_RAW		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_MUTUAL_DV		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_MUTUAL_BASE		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_MUTUAL_RAW		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_OPENSHORT		(200 * MSEC)

#define TSC_SNAPSHOT_NFRAME_DV			2 /* dv = mutual dv + self dv */
#define TSC_SNAPSHOT_NFRAME_BASE		2 /* base = mutual base + self base */
#define TSC_SNAPSHOT_NFRAME_RAW			2 /* raw = mutual raw + self raw */
#define TSC_SNAPSHOT_NFRAME_OPENSHORT		1 /* openshort = mutual openshort */

#define SNAPSHOT_FRAME_NTYP_DV          3       /* mutual + self Y + self X */
#define SNAPSHOT_FRAME_NTYP_BASE        3       /* mutual + self Y + self X */
#define SNAPSHOT_FRAME_NTYP_RAW         3       /* mutual + self Y + self X */
#define SNAPSHOT_FRAME_NTYP_OPENSHORT   1       /* mutual open-short */

#define SNAPSHOT_FRAME_SIZE_DV_0                (FRAME_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_DV_1                (SENSE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_DV_2                (FORCE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_BASE_0              (FRAME_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_BASE_1              (SENSE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_BASE_2              (FORCE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_RAW_0               (FRAME_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_RAW_1               (SENSE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_RAW_2               (FORCE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_OPENSHORT_0         (FRAME_SIZE * 2)

#define TSC_SNAPSHOT_TESTMOD_RECHK_MAX		3
#define TSC_SNAPSHOT_TESTMOD_RECHK_UDELAY	(1 * MSEC)

/* Variables */
struct raw_touch_frame raw_frame;
union hid_report hid_rpt;

const uint32_t snapshot_ntype_lst[SNAPSHOT_FIDX_MAX] = {
	SNAPSHOT_FRAME_NTYP_DV,
	SNAPSHOT_FRAME_NTYP_BASE,
	SNAPSHOT_FRAME_NTYP_RAW,
	SNAPSHOT_FRAME_NTYP_OPENSHORT,
};

const uint8_t *snapshot_src_dv[SNAPSHOT_FRAME_NTYP_DV] = {
	(uint8_t *)(raw_frame.frame_raw),
	(uint8_t *)(raw_frame.sense_raw),
	(uint8_t *)(raw_frame.force_raw),
};

const uint8_t *snapshot_src_base[SNAPSHOT_FRAME_NTYP_BASE] = {
	(uint8_t *)(raw_frame.frame_raw),
	(uint8_t *)(raw_frame.sense_raw),
	(uint8_t *)(raw_frame.force_raw),
};

const uint8_t *snapshot_src_raw[SNAPSHOT_FRAME_NTYP_RAW] = {
	(uint8_t *)(raw_frame.frame_raw),
	(uint8_t *)(raw_frame.sense_raw),
	(uint8_t *)(raw_frame.force_raw),
};

const uint8_t *snapshot_src_openshort[SNAPSHOT_FRAME_NTYP_OPENSHORT] = {
	(uint8_t *)(raw_frame.frame_raw),
};

const uint8_t **snapshot_src_map[SNAPSHOT_FIDX_MAX] = {
	snapshot_src_dv,
	snapshot_src_base,
	snapshot_src_raw,
	snapshot_src_openshort,
};

const int snapshot_siz_dv[SNAPSHOT_FRAME_NTYP_DV] = {
	SNAPSHOT_FRAME_SIZE_DV_0,
	SNAPSHOT_FRAME_SIZE_DV_1,
	SNAPSHOT_FRAME_SIZE_DV_2,
};

const int snapshot_siz_base[SNAPSHOT_FRAME_NTYP_BASE] = {
	SNAPSHOT_FRAME_SIZE_BASE_0,
	SNAPSHOT_FRAME_SIZE_BASE_1,
	SNAPSHOT_FRAME_SIZE_BASE_2,
};

const int snapshot_siz_raw[SNAPSHOT_FRAME_NTYP_RAW] = {
	SNAPSHOT_FRAME_SIZE_RAW_0,
	SNAPSHOT_FRAME_SIZE_RAW_1,
	SNAPSHOT_FRAME_SIZE_RAW_2,
};

const int snapshot_siz_openshort[SNAPSHOT_FRAME_NTYP_OPENSHORT] = {
	SNAPSHOT_FRAME_SIZE_OPENSHORT_0,
};

const int *snapshot_siz_map[SNAPSHOT_FIDX_MAX] = {
	snapshot_siz_dv,
	snapshot_siz_base,
	snapshot_siz_raw,
	snapshot_siz_openshort,
};

static const int snapshot_nframe_lst[SNAPSHOT_FIDX_MAX] = {
	TSC_SNAPSHOT_NFRAME_DV,
	TSC_SNAPSHOT_NFRAME_BASE,
	TSC_SNAPSHOT_NFRAME_RAW,
	TSC_SNAPSHOT_NFRAME_OPENSHORT,
};

struct snapshot_testmod_info {
	enum test_mode_idx	tmodidx;
	uint8_t			*buf;
	uint32_t		length;
	uint32_t		udelay;
};

static const struct snapshot_testmod_info snapshot_tmodinfo_dv[TSC_SNAPSHOT_NFRAME_DV]= {
	{ MUTUAL_DV, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf), TSC_SNAPSHOT_UDELAY_MUTUAL_DV },
	{ SELF_DV, raw_frame.self_buf, sizeof(raw_frame.self_buf), TSC_SNAPSHOT_UDELAY_SELF_DV },
};

static const struct snapshot_testmod_info snapshot_tmodinfo_base[TSC_SNAPSHOT_NFRAME_BASE] = {
	{ MUTUAL_BASE, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf), TSC_SNAPSHOT_UDELAY_MUTUAL_BASE },
	{ SELF_BASE, raw_frame.self_buf, sizeof(raw_frame.self_buf), TSC_SNAPSHOT_UDELAY_SELF_BASE },
};

static const struct snapshot_testmod_info snapshot_tmodinfo_raw[TSC_SNAPSHOT_NFRAME_RAW] = {
	{ MUTUAL_RAW, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf), TSC_SNAPSHOT_UDELAY_MUTUAL_RAW },
	{ SELF_RAW, raw_frame.self_buf, sizeof(raw_frame.self_buf), TSC_SNAPSHOT_UDELAY_SELF_RAW },
};

static const struct snapshot_testmod_info snapshot_tmodinfo_openshort[TSC_SNAPSHOT_NFRAME_OPENSHORT] = {
	{ OPENSHORT, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf), TSC_SNAPSHOT_UDELAY_OPENSHORT },
};

static const struct snapshot_testmod_info *snapshot_tmodinfo_lst[SNAPSHOT_FIDX_MAX] = {
	snapshot_tmodinfo_dv,
	snapshot_tmodinfo_base,
	snapshot_tmodinfo_raw,
	snapshot_tmodinfo_openshort,
};

#if defined(DEBUG_TSC) && DEBUG_TSC
/* Standard report buffer */
union {
	union hid_report tsc_rpt;
	struct touch_frame frame;
} test_buf;

enum HidI2cMasterApiTestIdx {
	TSCINIT		= 0,
	TSCRDRPT	= 1,
	TSCRDFRAME	= 2,
	TSCSLEEP	= 3,
	TSCWAKEUP	= 4,
	TSCSELFTEST	= 5,
	TSCSNAPSHOT	= 6,
	TSCMAXTESTIDX,
};

#define DBG_PRINT_BASE 64

static void print_array(uint8_t *data, uint32_t lng, uint32_t print_base)
{
	uint32_t i;
	uint32_t base = (print_base ? (print_base << 1) : DBG_PRINT_BASE);
	CCPRINTF("Length: %d", lng);
	for (i = 0; i < lng; i += 2) {
		if ((i % base) == 0)
			CCPRINTF("\n");
		CCPRINTF(" %05d", (int16_t)(((uint32_t)(data[i + 1]) << 8)
						+ (uint32_t)(data[i])));
	}
	CCPRINTF("\n");
}

static int tsc_api_test(int argc, char *argv[])
{
	uint32_t fwuid, i;
	int ret = EC_SUCCESS, idx, fidx, nframe;
	const struct snapshot_testmod_info *tmode;
	uint16_t chksum;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	idx = strtoi(argv[1], NULL, 0);
	if (idx == TSCSNAPSHOT) {
		if (argc != 3)
			return EC_ERROR_PARAM_COUNT;
		fidx = strtoi(argv[2], NULL, 0);
	}
	switch (idx) {
	case TSCINIT:
		ret = tsc_init();
		CCPRINTF("TSC - Init %d\n", ret);
		break;
	case TSCRDRPT:
		ret = tsc_read_report(&(test_buf.tsc_rpt), 0);
		CCPRINTF("TSC - RdRpt %d\n", ret);
		break;
	case TSCRDFRAME:
		ret = tsc_read_frame(&(test_buf.frame));
		CCPRINTF("TSC - RdFrame %d\n", ret);
		break;
	case TSCSLEEP:
		ret = tsc_sleep();
		CCPRINTF("TSC - Sleep %d\n", ret);
		break;
	case TSCWAKEUP:
		ret = tsc_wakeup();
		CCPRINTF("TSC - Waekup %d\n", ret);
		break;
	case TSCSELFTEST:
		ret = tsc_self_test(&fwuid, &chksum);
		CCPRINTF("TSC - self_test %d uid %08x chksum %04x\n", ret, fwuid, chksum);
		break;
	case TSCSNAPSHOT:
		ret = tsc_snapshot_frame(fidx);
		CCPRINTF("TSC - snapshot %d fidx %d\n", ret, fidx);
		if (!ret) {
			nframe = snapshot_nframe_lst[fidx];
			tmode = snapshot_tmodinfo_lst[fidx];
			CCPRINTF("TSC - nframe %d\n", nframe);
			for (i = 0; i < nframe; i++) {
				CCPRINTF("%d - tmidx %d lng %d delay %d - ibuf %x lng %d rid %x\n"
					, i
					, tmode->tmodidx
					, tmode->length
					, tmode->udelay
					, tmode
					, (tmode->buf[1] << 8) + tmode->buf[0]
					, tmode->buf[2]);
				if (i == 0)
					print_array(&(tmode->buf[3]), tmode->length - 3, 15);
				else {
					print_array(&(tmode->buf[3]), 30, 15);
					print_array(&(tmode->buf[33]), 52, 30);
				}
				tmode++;
			}
		}
		break;
	default:
		ret = EC_ERROR_UNIMPLEMENTED;
		break;
	}
	return ret;
}
DECLARE_CONSOLE_COMMAND(tscapi, tsc_api_test, "<API Number>",
                        "Test Specific TSC API");
#endif

/* Static Functions */
static int tsc_err_restore(void) {
	/* TODO: Add HW reset before init. */
	return tsc_init();
}

/* Functions */
int tsc_init(void) {
	int ret;

	/* HID Reset */
	ret = i2c_hid_reset();
	if (ret) {
		ret = TSC_RESET_FAIL;
		goto tsc_init_end;
	}

#if 0
	/* Polling for the first interrupt, for ELAN B50 the interval
	 * between RESET and Intp ready is about 800us. */
	ret = TSC_INTP_ON_TIMEOUT;
	cnt = TSC_RESET_POLLING_MAXCNT;
	do {
		intp = gpio_get_level(GPIO_ISH_TRACKPAD_INT_L);
		if (!intp) {
			ret = TSC_OK;
			break;
		}
		udelay(TSC_RESET_POLLING_UDELAY);
		cnt--;
	} while (cnt);
	if (ret)
		goto tsc_init_end;
#endif
	/* Change to PTP mode */
	ret = i2c_hid_set_ptp();
	if (ret) {
		ret = TSC_MODE_PTP_FAIL;
		goto tsc_init_end;
	}

// read hid descriptor and input report
	ret = i2c_hid_get_hid_desc();
	if (!ret) {
		ret = i2c_hid_get_rpt_desc();
		if (ret)
			CCPRINTF("err to get rpt desc\n");	
	} else {
		CCPRINTF("err to get hid desc\n");	
	}

	/* Read one input report for dummy data */
	ret = i2c_hid_get_input(INTP_ACT, hid_rpt.raw, HIDDESC_LNG_MAXINPUT);
	if (ret)
		ret = TSC_RD_INPUTRPT_FAIL;
tsc_init_end:
	CCPRINTF("tsc_init_end %d\n", ret);
	return ret;
}

//TODO: Maybe we should not use the return value directly as
//	the return fwuid?
#define INVALID_TSC_FWUID	(0x5A5A5A5A)
#define INVALID_TSC_CHKSUM	(0x5A5A)

int tsc_get_fw_id(void) {
	int ret;
	uint32_t FWUID;
	ret = i2c_hid_get_fwuid(&FWUID);
	if (ret)
		ret = (int)INVALID_TSC_FWUID;
	else
		ret = (int)FWUID;
        return ret;
}

int tsc_wakeup(void) {
	int ret;
	ret = i2c_hid_wakeup();
	ret = ret ? TSC_WAKEUP_FAIL : TSC_OK;
        return ret;
}
	
int tsc_sleep(void) {
	int ret;
	ret = i2c_hid_sleep();
	ret = ret ? TSC_SLEEP_FAIL : TSC_OK;
        return ret;
}

int tsc_read_report(union hid_report *tsc_report, int intp) {
	int ret;
	ret = i2c_hid_get_input(intp, tsc_report->raw, HIDDESC_LNG_MAXINPUT);
	ret = ret ? TSC_GETINPUT_FAIL : TSC_OK;
	return ret;
}

int tsc_read_frame(struct touch_frame *frame) {
	int ret;
	ret = i2c_hid_get_heatmap(frame->raw, FRAME_PACKAGE_SIZE);
	ret = ret ? TSC_GETHEATMAP_FAIL : TSC_OK;
        return ret;
}

int tsc_send_ack(void) {
	int ret;
	ret = i2c_hid_get_input(INTP_ACT, hid_rpt.raw, HIDDESC_LNG_MAXINPUT);
	ret = ret ? TSC_ACK_FAIL : TSC_OK;
        return ret;
}

int tsc_self_test(uint32_t *fwuid, uint16_t *chksum) {
	int ret;
	if (!fwuid || !chksum)
		return TSC_PARAM_ERR;
	ret = i2c_hid_get_fwuid(fwuid);
	if (ret) {
		*fwuid = INVALID_TSC_FWUID;
		ret = TSC_GETFWUID_FAIL;
		goto tsc_self_test_end;
	}
	ret = i2c_hid_get_chksum(chksum);
	if (ret) {
		*chksum = INVALID_TSC_CHKSUM;
		ret = TSC_GETCHKSUM_FAIL;
	}
tsc_self_test_end:
	return ret;
}

int tsc_snapshot_frame(enum snapshot_frame_idx fidx) {
	int ret, nframe, prv_ret;
	const struct snapshot_testmod_info *tmode;

	if (fidx >= SNAPSHOT_FIDX_MAX)
		return TSC_PARAM_ERR;

	CCPRINTF("mutual buf %x\n", raw_frame.mutual_buf);
	CCPRINTF("self buf %x\n", raw_frame.self_buf);
	/* Get the test mode info based on current fidx */
	nframe = snapshot_nframe_lst[fidx];
	tmode = snapshot_tmodinfo_lst[fidx];

	while (nframe) {
		CCPRINTF("idx %d delay %d buf %x lng %d\n"
			, tmode->tmodidx
			, tmode->udelay
			, tmode->buf
			, tmode->length);
		/* Start testmode with specific test mode index */
		ret = i2c_hid_testmode_start(tmode->tmodidx);
		if (ret) {
			ret = TSC_TMODE_START_FAIL;
			break;
		}
		/* Each test mode index should wait for a specific delay */
		udelay(tmode->udelay);
		/* Get the test data */
		ret = i2c_hid_testmode_get(tmode->buf, tmode->length);
		if (ret) {
			ret = TSC_TMODE_GETDAT_FAIL;
			break;
		}
		CCPRINTF("0x%02x 0x%02x 0x%02x\n", tmode->buf[0], tmode->buf[1], tmode->buf[2]);
		nframe--;
		tmode++;
	};
	prv_ret = ret ? ret : TSC_OK;
	ret = i2c_hid_testmode_stop();
	if (ret) {
		/* TODO: What else can we do if restore fail? */
		tsc_err_restore();
	}
	return prv_ret ? prv_ret : ret;
}

