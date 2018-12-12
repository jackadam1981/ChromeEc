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

#define DEBUG_TSC		0

#define TSC_RET_OK		0
#define TSC_RET_FAIL		1

#if defined(DEBUG_TSC) && DEBUG_TSC
#define CCPRINTF(format, args...)	ccprintf(format, ## args)
#else
#define CCPRINTF(format, args...)
#endif

#define TSC_RESET_POLLING_UDELAY		(1 * MSEC)
#define TSC_RESET_POLLING_MAXCNT		(10)
#define TSC_RESET_UDELAY			(10 * MSEC)

/* Snapshot Defines*/
#define TSC_SNAPSHOT_UDELAY_SELF_DV		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_SELF_BASE		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_SELF_RAW		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_MUTUAL_DV		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_MUTUAL_BASE		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_MUTUAL_RAW		(20 * MSEC)
#define TSC_SNAPSHOT_UDELAY_OPENSHORT		(200 * MSEC)

#define TSC_SNAPSHOT_NFRAME_DV			2
#define TSC_SNAPSHOT_NFRAME_BASE		2
#define TSC_SNAPSHOT_NFRAME_RAW			2
#define TSC_SNAPSHOT_NFRAME_OPENSHORT		1

#define SNAPSHOT_FRAME_NTYP_DV		3
#define SNAPSHOT_FRAME_NTYP_BASE	3
#define SNAPSHOT_FRAME_NTYP_RAW		3
#define SNAPSHOT_FRAME_NTYP_OPENSHORT	1

#define SNAPSHOT_FRAME_SIZE_DV_0		(FRAME_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_DV_1		(SENSE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_DV_2		(FORCE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_BASE_0		(FRAME_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_BASE_1		(SENSE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_BASE_2		(FORCE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_RAW_0		(FRAME_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_RAW_1		(SENSE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_RAW_2		(FORCE_SIZE * 2)
#define SNAPSHOT_FRAME_SIZE_OPENSHORT_0		(FRAME_SIZE * 2)

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

static const struct snapshot_testmod_info
snapshot_tmodinfo_dv[TSC_SNAPSHOT_NFRAME_DV] = {
	{ MUTUAL_DV, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf),
		TSC_SNAPSHOT_UDELAY_MUTUAL_DV },
	{ SELF_DV, raw_frame.self_buf, sizeof(raw_frame.self_buf),
		TSC_SNAPSHOT_UDELAY_SELF_DV },
};

static const struct snapshot_testmod_info
snapshot_tmodinfo_base[TSC_SNAPSHOT_NFRAME_BASE] = {
	{ MUTUAL_BASE, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf),
		TSC_SNAPSHOT_UDELAY_MUTUAL_BASE },
	{ SELF_BASE, raw_frame.self_buf, sizeof(raw_frame.self_buf),
		TSC_SNAPSHOT_UDELAY_SELF_BASE },
};

static const struct snapshot_testmod_info
snapshot_tmodinfo_raw[TSC_SNAPSHOT_NFRAME_RAW] = {
	{ MUTUAL_RAW, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf),
		TSC_SNAPSHOT_UDELAY_MUTUAL_RAW },
	{ SELF_RAW, raw_frame.self_buf, sizeof(raw_frame.self_buf),
		TSC_SNAPSHOT_UDELAY_SELF_RAW },
};

static const struct snapshot_testmod_info
snapshot_tmodinfo_openshort[TSC_SNAPSHOT_NFRAME_OPENSHORT] = {
	{ OPENSHORT, raw_frame.mutual_buf, sizeof(raw_frame.mutual_buf),
		TSC_SNAPSHOT_UDELAY_OPENSHORT },
};

static const struct snapshot_testmod_info
*snapshot_tmodinfo_lst[SNAPSHOT_FIDX_MAX] = {
	snapshot_tmodinfo_dv,
	snapshot_tmodinfo_base,
	snapshot_tmodinfo_raw,
	snapshot_tmodinfo_openshort,
};

/* Static Functions */
static int tsc_err_restore(void)
{
	/* TODO: Add HW reset before init. */
	return tsc_init();
}

/* Functions */
int tsc_init(void)
{
	int ret;

	ret = i2c_hid_reset();
	if (ret)
		goto tsc_init_end;
	/* wait for reset complete */
	udelay(TSC_RESET_UDELAY);

	ret = i2c_hid_set_ptp();
	if (ret)
		goto tsc_init_end;

	ret = i2c_hid_get_input(INTP_ACT, hid_rpt.raw, HIDDESC_LNG_MAXINPUT);
tsc_init_end:
	CCPRINTF("tsc_init_end %d\n", ret);
	return ret;
}

int tsc_get_fw_id(uint32_t *fwuid)
{
	return i2c_hid_get_fwuid(fwuid);
}

int tsc_get_chksum(uint16_t *chksum)
{
	return i2c_hid_get_chksum(chksum);
}

int tsc_wakeup(void)
{
	return i2c_hid_wakeup();
}

int tsc_sleep(void)
{
	return i2c_hid_sleep();
}

int tsc_read_report(union hid_report *tsc_report, int intp)
{
	return i2c_hid_get_input(intp, tsc_report->raw, HIDDESC_LNG_MAXINPUT);
}

int tsc_read_frame(struct touch_frame *frame)
{
	return i2c_hid_get_heatmap(frame->raw, FRAME_PACKAGE_SIZE);
}

int tsc_send_ack(void)
{
	return i2c_hid_get_input(INTP_ACT, hid_rpt.raw, HIDDESC_LNG_MAXINPUT);
}

int tsc_snapshot_frame(enum snapshot_frame_idx fidx)
{
	int ret, nframe, prv_ret;
	const struct snapshot_testmod_info *tmode;

	if (fidx >= SNAPSHOT_FIDX_MAX)
		return TSC_RET_FAIL;

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
		if (ret)
			break;
		/* Each test mode index should wait for a specific delay */
		udelay(tmode->udelay);
		/* Get the test data */
		ret = i2c_hid_testmode_get(tmode->buf, tmode->length);
		if (ret)
			break;
		CCPRINTF("0x%02x 0x%02x 0x%02x\n", tmode->buf[0], tmode->buf[1],
							tmode->buf[2]);
		nframe--;
		tmode++;
	};
	prv_ret = ret ? ret : TSC_RET_OK;
	ret = i2c_hid_testmode_stop();
	if (ret)
		tsc_err_restore();
	return prv_ret ? prv_ret : ret;
}

