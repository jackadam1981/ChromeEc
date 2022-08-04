/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __TCS3410_HWDEF_H__
#define __TCS3410_HWDEF_H__

#define SLAVE_ADDR_0 (0x39)

/* unit milliseconds */
#define MOD_CLOCK_STEP_MS (0.001388889)

enum hw_regs
{
    REG_P2RAM_OTP_8                       =  0x08,  /* UV_CALIB */
    REG_MOD_CHANNEL_CTRL                  =  0x40,
    REG_ENABLE                            =  0x80,
    REG_MEAS_MODE0                        =  0x81,
    REG_MEAS_MODE1                        =  0x82,
    REG_SAMPLE_TIME0                      =  0x83,
    REG_SAMPLE_TIME1                      =  0x84,
    REG_ALS_NR_SAMPLES0                   =  0x85,
    REG_ALS_NR_SAMPLES1                   =  0x86,
    REG_FD_NR_SAMPLES0                    =  0x87,
    REG_FD_NR_SAMPLES1                    =  0x88,
    REG_WTIME                             =  0x89,
    REG_AILT0                             =  0x8A,
    REG_AILT1                             =  0x8B,
    REG_AILT2                             =  0x8C,
    REG_AIHT0                             =  0x8D,
    REG_AIHT1                             =  0x8E,
    REG_AIHT2                             =  0x8F,
    REG_AUXID                             =  0x90, /* rev2 id */
    REG_REVID                             =  0x91,
    REG_DEVICEID                          =  0x92,
    REG_STATUS                            =  0x93,
    REG_ALS_STATUS                        =  0x94,
    REG_DATAL0                            =  0x95,
    REG_DATAH0                            =  0x96,
    REG_DATAL1                            =  0x97,
    REG_DATAH1                            =  0x98,
    REG_DATAL2                            =  0x99,
    REG_DATAH2                            =  0x9A,
    REG_ALS_STATUS2                       =  0x9B,
    REG_ALS_STATUS3                       =  0x9C,
    REG_STATUS2                           =  0x9D,
    REG_STATUS3                           =  0x9E,
    REG_STATUS4                           =  0x9F,
    REG_STATUS5                           =  0xA0,
    REG_CFG0                              =  0xA1,
    REG_CFG1                              =  0xA2,
    REG_CFG2                              =  0xA3,
    REG_CFG3                              =  0xA4,
    REG_CFG4                              =  0xA5,
    REG_CFG5                              =  0xA6,
    REG_CFG6                              =  0xA7,
    REG_CFG7                              =  0xA8,
    REG_CFG8                              =  0xA9,
    REG_CFG9                              =  0xAA,
    REG_AGC_NR_SAMPLES0                   =  0xAC,
    REG_AGC_NR_SAMPLES1                   =  0xAD,
    REG_TRIGGER_MODE                      =  0xAE,
    REG_CONTROL                           =  0xB1,
    REG_INTENAB                           =  0xBA,
    REG_SIEN                              =  0xBB,
    REG_MOD_COMP_CFG1                     =  0xCE,
    REG_MEAS_SEQR_FD_0                    =  0xCF,
    REG_MEAS_SEQR_ALS_FD_1                =  0xD0,
    REG_MEAS_SEQR_APERS_AND_VSYNC_WAIT    =  0xD1,
    REG_MEAS_SEQR_RESIDUAL_0              =  0xD2,
    REG_MEAS_SEQR_RESIDUAL_1_AND_WAIT     =  0xD3,
    REG_MEAS_SEQR_STEP0_MOD_GAINX_L       =  0xD4,
    REG_MEAS_SEQR_STEP0_MOD_GAINX_H       =  0xD5,
    REG_MEAS_SEQR_STEP1_MOD_GAINX_L       =  0xD6,
    REG_MEAS_SEQR_STEP1_MOD_GAINX_H       =  0xD7,
    REG_MEAS_SEQR_STEP2_MOD_GAINX_L       =  0xD8,
    REG_MEAS_SEQR_STEP2_MOD_GAINX_H       =  0xD9,
    REG_MEAS_SEQR_STEP3_MOD_GAINX_L       =  0xDA,
    REG_MEAS_SEQR_STEP3_MOD_GAINX_H       =  0xDB,
    REG_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_L   =  0xDC,
    REG_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_H   =  0xDD,
    REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_L   =  0xDE,
    REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_H   =  0xDF,
    REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_L   =  0xE0,
    REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_H   =  0xE1,
    REG_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_L   =  0xE2,
    REG_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_H   =  0xE3,
    REG_MOD_CALIB_CFG0                    =  0xE4,
    REG_MOD_CALIB_CFG2                    =  0xE6,
    REG_VSYNC_PERIOD_L                    =  0xF2,
    REG_VSYNC_PERIOD_H                    =  0xF3,
    REG_VSYNC_PERIOD_TARGET_L             =  0xF4,
    REG_VSYNC_PERIOD_TARGET_H             =  0xF5,
    REG_VSYNC_CONTROL                     =  0xF6,
    REG_VSYNC_CFG                         =  0xF7,
    REG_VSYNC_GPIO_INT                    =  0xF8,
    REG_MOD_FIFO_DATA_CFG0                =  0xF9,
    REG_MOD_FIFO_DATA_CFG1                =  0xFA,
    REG_MOD_FIFO_DATA_CFG2                =  0xFB,
    REG_FIFO_THR                          =  0xFC,
    REG_FIFO_LEVEL                        =  0xFD,
    REG_FIFO_STATUS0                      =  0xFE,
    REG_FIFO_DATA                         =  0xFF,
};

/* enable  - 0x80 */
enum _enable_reg
{
    DEVICE_PON     = (1 << 0),
    DEVICE_AEN     = (1 << 1),
    DEVICE_FDEN    = (1 << 6),
    DEVICE_EN_ALL  = (DEVICE_PON | DEVICE_AEN | DEVICE_FDEN),
};

/* meas_mode0 0x81 */
#define MEAS_MODE0_STOP_AFTER_NTH_ITER_SHIFT                   (7)
#define MEAS_MODE0_STOP_AFTER_NTH_ITER_MASK                    (1 << MEAS_MODE0_STOP_AFTER_NTH_ITER_SHIFT)

#define MEAS_MODE0_ENABLE_AGC_ASAT_DOUBLE_STEP_DOWN_SHIFT      (6)
#define MEAS_MODE0_ENABLE_AGC_ASAT_DOUBLE_STEP_DOWN_MASK       (1 << MEAS_MODE0_ENABLE_AGC_ASAT_DOUBLE_STEP_DOWN_SHIFT)

#define MEAS_MODE0_MEAS_SEQR_SINGLE_SHOT_MODE_SHIFT            (5)
#define MEAS_MODE0_MEAS_SEQR_SINGLE_SHOT_MODE_MASK             (1 << MEAS_MODE0_MEAS_SEQR_SINGLE_SHOT_MODE_SHIFT)

#define MEAS_MODE0_FIFO_ALS_STATUS_WRITE_ENABLE_SHIFT          (4)
#define MEAS_MODE0_FIFO_ALS_STATUS_WRITE_ENABLE_MASK           (1 << MEAS_MODE0_FIFO_ALS_STATUS_WRITE_ENABLE_SHIFT)

#define MEAS_MODE0_FIFO_ALS_SCALE_SHIFT                        (0)
#define MEAS_MODE0_FIFO_ALS_SCALE_MASK                         (0x0F << MEAS_MODE0__FIFO_ALS_SCALE_SHIFT)

/* meas_mode1  0x82 */
#define MEAS_MODE1_MOD_FIFO_FD_END_MARKER_WRITE_EN_SHIFT       (7)
#define MEAS_MODE1_MOD_FIFO_FD_END_MARKER_WRITE_EN_MASK        (1 << MEAS_MODE1_MOD_FIFO_FD_END_MARKER_WRITE_EN_SHIFT)

#define MEAS_MODE1_MOD_FIFO_FD_CHKSUM_MARKER_WRITE_EN_SHIFT    (6)
#define MEAS_MODE1_MOD_FIFO_FD_CHKSUM_MARKER_WRITE_EN_MASK     (1 << MEAS_MODE1_MOD_FIFO_FD_CHKSUM_MARKER_WRITE_EN_SHIFT)

#define MEAS_MODE1_MOD_FIFO_FD_GAIN_WRITE_EN_SHIFT             (5)
#define MEAS_MODE1_MOD_FIFO_FD_GAIN_WRITE_EN_MASK              (1 << MEAS_MODE1_MOD_FIFO_FD_GAIN_WRITE_EN_SHIFT)

#define DEFAULT_ALS_MSB_POSITION                               (0x08)
#define MEAS_MODE1_ALS_MSB_POSITION_SHIFT                      (0)
#define MEAS_MODE1_ALS_MSB_POSITION_MASK                       (0x1F << MEAS_MODE1_SHIFT_ALS_MSB_POSITION)

/* sample_time0:  0x83 */
#define SAMPLE_TIME0_SHIFT             (0)
#define SAMPLE_TIME0_MASK              (0xFF)

/* sample_time1: 0x84 */
#define SAMPLE_TIME1_SHIFT             (8)
#define SAMPLE_TIME1_MASK              (0x07)

/* als_nr_samples0: 0x85 */
#define ALS_NR_SAMPLES0_SHIFT          (0)
#define ALS_NR_SAMPLES0_MASK           (0xFF)

/* als_nr_samples1: 0x86 */
#define ALS_NR_SAMPLES1_SHIFT          (8)
#define ALS_NR_SAMPLES1_MASK           (0x07)

/* fd_nr_samples0: 0x87 */
#define FD_NR_SAMPLES0_SHIFT           (0)
#define FD_NR_SAMPLES0_MASK            (0xFF)

/* fd_nr_samples1: 0x88 */
#define FD_NR_SAMPLES1_SHIFT           (8)
#define FD_NR_SAMPLES1_MASK            (0x07)

/* wtime: 0x89 */
#define WTIME_SHIFT                    (0)
#define WTIME_MASK                     (0xFF)

/* ailt0, ailt1, ailt2, aiht0, aiht1, aiht2:  0x8A - 0x8F */
/* ALS Interrupt thresholds are not used - data written to FIFO */


/* aux_id, rev_id, device_id: 0x90, 0x91, 0x92 */
#define AUX_ID_SHIFT                                              (0)
#define AUX_ID_MASK                                               (0x0F)   /* also known as rev2 ID */

#define REV_ID_SHIFT                                              (0)
#define REV_ID_MASK                                               (0xFF)

#define DEVICE_ID_SHIFT                                           (0)
#define DEVICE_ID_MASK                                            (0xFF)

/* STATUS - 0x93 */
/* See tcs3410_irq.h */

/* als_status 0x94 */
#define ALS_STATUS_MESS_SEQR_STEP_SHIFT            (6)
#define ALS_STATUS_MESS_SEQR_STEP_MASK        (3 << ALS_STATUS_MESS_SEQR_STEP_SHIFT)

#define ALS_STATUS_DATA0_ANA_SAT_SHIFT             (5)
#define ALS_STATUS_DATA0_ANA_SAT_MASK        (1 << ALS_STATUS_DATA0_ANA_SAT_SHIFT)

#define ALS_STATUS_DATA1_ANA_SAT_SHIFT             (4)
#define ALS_STATUS_DATA1_ANA_SAT_MASK        (1 << ALS_STATUS_DATA1_ANA_SAT_SHIFT)

#define ALS_STATUS_DATA2_ANA_SAT_SHIFT             (3)
#define ALS_STATUS_DATA2_ANA_SAT_MASK        (1 << ALS_STATUS_DATA2_ANA_SAT_SHIFT)

#define ALS_STATUS_DATA0_SCALED_SHIFT              (2)
#define ALS_STATUS_DATA0_SCALED_MASK        (1 << ALS_STATUS_DATA0_SCALED_SHIFT)

#define ALS_STATUS_DATA1_SCALED_SHIFT              (1)
#define ALS_STATUS_DATA1_SCALED_MASK        (1 << ALS_STATUS_DATA1_SCALED_SHIFT)

#define ALS_STATUS_DATA2_SCALED_SHIFT              (0)
#define ALS_STATUS_DATA2_SCALED_MASK        (1 << ALS_STATUS_DATA2_SCALED_SHIFT)

/* als_data[0,1,2]: 0x95-0x9A */
/* ALS data written to FIFO */

/* als_status2: 0x9B */
#define ALS_STATUS2_DATA0_GAIN_STATUS_SHIFT           (0)
#define ALS_STATUS2_DATA0_GAIN_STATUS_MASK            (0x0F << ALS_STATUS2_DATA0_GAIN_STATUS_SHIFT)
#define ALS_STATUS2_DATA1_GAIN_STATUS_SHIFT           (4)
#define ALS_STATUS2_DATA1_GAIN_STATUS_MASK            (0x0F << ALS_STATUS2_DATA1_GAIN_STATUS_SHIFT)

/* als_status3 0x9C */
#define ALS_STATUS3_DATA2_GAIN_STATUS_SHIFT           (0)
#define ALS_STATUS3_DATA2_GAIN_STATUS_MASK            (0x0F << ALS_STATUS3_DATA2_GAIN_STATUS_SHIFT)

/* status2: 0x9D */
/* see tcs3410_irq.h */

/* status3: 0x9E */
/* see tcs3410_irq.h */

/* status4: 0x9F */
/* see tcs3410_irq.h */

/* status5: 0xA0 */
/* see tcs3410_irq.h */

/* cfg0: 0xA1 */
#define CFG0_SAI_SHIFT           (6)
#define CFG0_SAI_MASK            (0x01 << CFG0_SAI_SHIFT)

#define CFG0_LOW_POWER_SHIFT     (5)
#define CFG0_LOW_POWER_MASK      (0x01 << CFG0_LOW_POWER_SHIFT)

/* cfg1: 0xA2 */

/* cfg2 - 0xA3 - See 0xF3 for High */
#define  CFG2_FIFO_THRESH0_SHIFT       (0)
#define  CFG2_FIFO_THRESH0_MASK        (0x1 << CFG2_FIFO_THRESH0_SHIFT)

/* cfg3 - 0xA4 */
#define CFG3_INT_PINMAP_SHIFT          (4)
#define CFG3_INT_PINMAP_MASK           (3 << CFG3_INT_PINMAP_SHIFT)

#define CFG3_VSYNC_GPIO_PINMAP_SHIFT   (0)
#define CFG3_VSYNC_GPIO_PINMAP_MASK    (3 << CFG3_VSYNC_GPIO_PINMAP_SHIFT)

/* cfg4 0xA5 */
#define CFG4_MOD_CALIB_NTH_ITER_STEP_EN_SHIFT       (6)
#define CFG4_MOD_CALIB_NTH_ITER_STEP_EN_MASK        (1 << CFG4_MOD_CALIB_NTH_ITER_STEP_EN_SHIFT)

#define CFG4_MEAS_SEQR_AGC_PREDICT_TGT_LVL_SHIFT    (5)
#define CFG4_MEAS_SEQR_AGC_PREDICT_TGT_LVL_MASK     (1 << CFG4_MEAS_SEQR_AGC_PREDICT_TGT_LVL_SHIFT)

enum meas_seqr_sint
{
    SINT_BY_ROUND   = 0,
    SINT_BY_STEP    = 1,
};
#define CFG4_MEAS_SEQR_SINT_PER_STEP_SHIFT          (4)
#define CFG4_MEAS_SEQR_SINT_PER_STEP_MASK           (1 << CFG4_MEAS_SEQR_SINT_PER_STEP_SHIFT)

#define CFG4_OSC_TUNE_NO_RESET_SHIFT                (3)
#define CFG4_OSC_TUNE_NO_RESET_MASK                 (1 << CFG4_OSC_TUNE_NO_RESET_SHIFT)

enum als_fifo_data_format
{
    ALS_DATA_FORMAT_16_BIT,
    ALS_DATA_FORMAT_24_BIT,
    ALS_DATA_FORMAT_RESERVED,
    ALS_DATA_FORMAT_32_BIT,
};

#define CFG4_MOD_ALS_FIFO_DATA_FORMAT_SHIFT         (0)
#define CFG4_MOD_ALS_FIFO_DATA_FORMAT_MASK          (3 << CFG4_MOD_ALS_FIFO_DATA_FORMAT_SHIFT)

/* cfg5  0xA6 */
#define CFG5_ALS_THRESH_CHANNEL_SHIFT               (4)
#define CFG5_ALS_THRESH_CHANNEL_MASK                (3 << CFG5_ALS_THRESH_CHANNEL_SHIFT)

#define CFG5_APERS_SHIFT                            (0)
#define CFG5_APERS_MASK                             (0x0F << CFG5_APERS_SHIFT)

/* cfg6 0xA7 - TBD */
#define CFG6_MOD_MEAS_COMPLETE_STARTUP_SHIFT        (5)
#define CFG6_MOD_MEAS_COMPLETE_STARTUP_MASK         (1 << CFG6_MOD_MEAS_COMPLETE_STARTUP_SHIFT)

#define CFG6_MOD_MIN_RESIDUAL_BITS_SHIFT            (2)
#define CFG6_MOD_MIN_RESIDUAL_BITS_MASK             (3 << CFG6_MOD_MIN_RESIDUAL_BITS_SHIFT)

#define CFG6_MOD_MAX_RESIDUAL_BITS_SHIFT            (0)
#define CFG6_MOD_MAX_RESIDUAL_BITS_MASK             (3 << CFG6_MOD_MAX_RESIDUAL_BITS_SHIFT)
#define DEFAULT_MAX_RESIDUAL_BITS                   (3)

/* cfg7 - 0xA8 reserved */

/* cfg8 - 0xA9 */
#define CFG8_MEAS_SEQR_MAX_MOD_GAIN_SHIFT           (4)
#define CFG8_MEAS_SEQR_MAX_MOD_GAIN_MASK            (0x0F << CFG8_MEAS_SEQR_MAX_MOD_GAIN_SHIFT)

#define CFG8_MEAS_SEQR_AGC_PREDICT_MOD_GAIN_REDUCT_SHIFT     (0)
#define CFG8_MEAS_SEQR_AGC_PREDICT_MOD_GAIN_REDUCT_MASK      (0x0F << CFG8_MEAS_SEQR_AGC_PREDICT_MOD_GAIN_REDUCT_SHIFT)

/* cfg9 - 0xAA */
#define CFG9_MOD_RESIDUAL_BITS_IGNORE_SHIFT         (0)
#define CFG9_MOD_RESIDUAL_BITS_IGNORE_MASK          (3 << CFG9_MOD_RESIDUAL_BITS_IGNORE_SHIFT)

/* agc_nr_samples - 0xAC[7:0] - 0xAD[2:0] */
#define AGC_NR_SAMPLES0_SHIFT          (0)
#define AGC_NR_SAMPLES0_MASK           (0xFF << AGC_NR_SAMPLES0_SHIFT)
#define AGC_NR_SAMPLES1_SHIFT          (8)
#define AGC_NR_SAMPLES1_MASK           (0x7)
/* trigger_mode:  0xAE */
#define TRIGGER_MODE_SHIFT              (0)
#define TRIGGER_MODE_MASK               (7 << TRIGGER_MODE_SHIFT)

/* control 0xB1 */
#define CONTROL_SOFT_RESET_SHIFT        (3)
#define CONTROL_SOFT_RESET_MASK         (1 << CONTROL_SOFT_RESET_SHIFT)

#define CONTROL_FIFO_CLR_SHIFT          (1)
#define CONTROL_FIFO_CLR_MASK           (1 << CONTROL_FIFO_CLR_SHIFT)

#define CONTROL_CLR_SAI_ACTIVE_SHIFT    (0)
#define CONTROL_CLR_SAI_ACTIVE_MASK     (1 << CONTROL_CLR_SAI_ACTIVE_SHIFT)

/* INTENAB - 0xBA */
/* defined in tcs3410_irq.h */

/* SIEN - 0xBB */
#define SIEN_MEASUREMENT_SEQUENCER_SHIFT       (1)
#define SIEN_MEASUREMENT_SEQUENCER_MASK        (0x01 << SIEN_MEASUREMENT_SEQUENCER_SHIFT)

/* mod_comp_cfg1 - 0xCE */
enum mod_idac_ranges
{
    IDAC_58uV,
    IDAC_38uV,
    IDAC_18uV,
    IDAC_9uV,
};

#define MOD_COMP_CFG1_IDAC_RANGE_SHIFT                 (6)
#define MOD_COMP_CFG1_IDAC_RANGE_MASK                  (3 << MOD_COMP_CFG1_IDAC_RANGE_SHIFT)

/* meas_seqr_fd_0 - 0xCF */
/* meas_seqr_als_fd_1 - 0xCF */
enum fd_sequences
{
    FD_OFF_ALL_STEPS         = (0),
    FD_ACTIVE_SEQ_STEP_0     = (1 << 0),
    FD_ACTIVE_SEQ_STEP_1     = (1 << 1),
    FD_ACTIVE_SEQ_STEP_2     = (1 << 2),
    FD_ACTIVE_SEQ_STEP_3     = (1 << 3),
    FD_ACTIVE_SEQ_ALL_STEPS  = (FD_ACTIVE_SEQ_STEP_0 | FD_ACTIVE_SEQ_STEP_1 | FD_ACTIVE_SEQ_STEP_2 | FD_ACTIVE_SEQ_STEP_3),
};
#define FD_ACTIVE_MOD0_SHIFT                  (0)
#define FD_ACTIVE_MOD1_SHIFT                  (4)
#define FD_ACTIVE_MOD2_SHIFT                  (4)

enum als_active_sequences
{
    ALS_OFF_ALL_STEPS         = (0),
    ALS_ACTIVE_SEQ_STEP_0     = (1 << 0),
    ALS_ACTIVE_SEQ_STEP_1     = (1 << 1),
    ALS_ACTIVE_SEQ_STEP_2     = (1 << 2),
    ALS_ACTIVE_SEQ_STEP_3     = (1 << 3),
    ALS_ACTIVE_ALL_STEPS      = (ALS_ACTIVE_SEQ_STEP_0 | ALS_ACTIVE_SEQ_STEP_1 | ALS_ACTIVE_SEQ_STEP_2 | ALS_ACTIVE_SEQ_STEP_3),
};
#define SHIFT_ALS_ACTIVE_ALL_MODS_SHIFT             (0)

/* meas_seqr_apers_and_vsync_wait - 0xD1 */
/* meas_seqr_residual_0 - 0xD2           */
/* meas_seqr_residual_1_and_wait - 0xD3  */
enum als_persist_sequences
{
    ALS_PERSIST_OFF_ALL_STEPS   = (0),
    ALS_PERSIST_SEQ_STEP_0      = (1 << 0),
    ALS_PERSIST_SEQ_STEP_1      = (1 << 1),
    ALS_PERSIST_SEQ_STEP_2      = (1 << 2),
    ALS_PERSIST_SEQ_STEP_3      = (1 << 3),
    ALS_PERSIST_ALL_STEPS       = (ALS_PERSIST_SEQ_STEP_0 | ALS_PERSIST_SEQ_STEP_1 | ALS_PERSIST_SEQ_STEP_2 | ALS_PERSIST_SEQ_STEP_3),
};

enum vsync_wait_sequences
{
    VSYNC_WAIT_OFF_ALL_STEPS        = (0),
    VSYNC_WAIT_SEQ_STEP_0           = (1 << 0),
    VSYNC_WAIT_SEQ_STEP_1           = (1 << 1),
    VSYNC_WAIT_SEQ_STEP_2           = (1 << 2),
    VSYNC_WAIT_SEQ_STEP_3           = (1 << 3),
    VSYNC_WAIT_ALL_STEPS            = (VSYNC_WAIT_SEQ_STEP_0 | VSYNC_WAIT_SEQ_STEP_1 | VSYNC_WAIT_SEQ_STEP_2 | VSYNC_WAIT_SEQ_STEP_3),

};

/* meas_seqr_residual_1_and_wait - 0xD3 */
enum wait_sequences
{
    WAIT_OFF_ALL_STEPS          = (0),
    WAIT_ENABLE_SEQ_STEP_0      = (1 << 0),
    WAIT_ENABLE_SEQ_STEP_1      = (1 << 1),
    WAIT_ENABLE_SEQ_STEP_2      = (1 << 2),
    WAIT_ENABLE_SEQ_STEP_3      = (1 << 3),
    WAIT_ENABLE_ALL_STEPS       = (WAIT_ENABLE_SEQ_STEP_0 | WAIT_ENABLE_SEQ_STEP_1 | WAIT_ENABLE_SEQ_STEP_2 | WAIT_ENABLE_SEQ_STEP_3),
};

enum residuals_enable_sequences
{
    RESIDUALS_OFF_ALL_STEPS          = (0),
    RESIDUALS_ENABLE_SEQ_STEP_0      = (1 << 0),
    RESIDUALS_ENABLE_SEQ_STEP_1      = (1 << 1),
    RESIDUALS_ENABLE_SEQ_STEP_2      = (1 << 2),
    RESIDUALS_ENABLE_SEQ_STEP_3      = (1 << 3),
    RESIDUALS_ENABLE_ALL_STEPS       = (RESIDUALS_ENABLE_SEQ_STEP_0 | RESIDUALS_ENABLE_SEQ_STEP_1 | RESIDUALS_ENABLE_SEQ_STEP_2 | RESIDUALS_ENABLE_SEQ_STEP_3),
};
#define ALS_PERSIST_ALL_MODS_SHIFT       (0)
#define VSYNC_WAIT_MODS_SHIFT            (4)
#define WAIT_ENABLE_SHIFT                (4)

#define RESIDUAL_ENABLE_MOD0_SHIFT       (0)
#define RESIDUAL_ENABLE_MOD1_SHIFT       (4)
#define RESIDUAL_ENABLE_MOD2_SHIFT       (0)

/* meas_seqr_stepx_gainx_l 0xD4 - 0xDB */
enum mod_gains
{
    M_HALF_X_GAIN            = 0x00,
    M_1_X_GAIN               = 0x01,
    M_2_X_GAIN               = 0x02,
    M_4_X_GAIN               = 0x03,
    M_8_X_GAIN               = 0x04,
    M_16_X_GAIN              = 0x05,
    M_32_X_GAIN              = 0x06,
    M_64_X_GAIN              = 0x07,
    M_128_X_GAIN             = 0x08,
    M_256_X_GAIN             = 0x09,
    M_512_X_GAIN             = 0x0A,
    M_1024_X_GAIN            = 0x0B,
    M_2048_X_GAIN            = 0x0C,
    M_4096_X_GAIN            = 0x0D,
};
#define GAIN_MOD0_SHIFT                  (0)
#define GAIN_MOD1_SHIFT                  (4)
#define GAIN_MOD2_SHIFT                  (0)


/* meas_seqr_stepx_mod_phdx_smux_l/h - 0xDC - 0xE3 */
/* Used to set the PHD/modulator mapping in each step */
#define   PHD_NO_CONNECT_MOD0   (0x00)
#define   PHD_CONNECT_MOD0      (0x01)
#define   PHD_CONNECT_MOD1      (0x02)
#define   PHD_CONNECT_MOD2      (0x03)

/* smux_l */
#define   PHD0_SHIFT        (0)
#define   PHD1_SHIFT        (2)
#define   PHD2_SHIFT        (4)
#define   PHD3_SHIFT        (6)

/* smux_h */
#define   PHD4_SHIFT         (0)    /* same bit position as PHD0 */
#define   PHD5_SHIFT         (2)    /* same bit position as PHD1 */
#define   MOD_PHD_MASK       (0x0F) /* mask for the photdiode bits */

/* 0xDF:[7:4] measrement_seqr_agc_asat_pattern */
/* 0xE1:[7:4] measrement_seqr_agc_predict_pattern */
/* meas_seqr_step1_mod_phdx_smux_h - 0xDF */
#define MEASUREMENT_SEQUENCER_AGC_ASAT_PATTERN_SHIFT (4)
#define MEASUREMENT_SEQUENCER_AGC_ASAT_PATTERN_MASK  (0x0F << MEASUREMENT_SEQUENCER_AGC_ASAT_PATTERN_SHIFT)

/* meas_seqr_step2_mod_phdx_smux_h - 0xE1 */
#define MEASUREMENT_SEQUENCER_AGC_PREDICT_PATTERN_SHIFT (4)
#define MEASUREMENT_SEQUENCER_AGC_PREDICT_PATTERN_MASK  (0x0F << MEASUREMENT_SEQUENCER_AGC_PREDICT_PATTERN_SHIFT)

/* mod_calib_cfg0 - 0xE4 */

/* mod_calib_cfg2  - 0xE6 */
#define MOD_CALIB_CFG2_NTH_ITER_RC_ENABLE_SHIFT        (7)
#define MOD_CALIB_CFG2_NTH_ITER_RC_ENABLE_MASK         (1 << MOD_CALIB_CFG2_NTH_ITER_RC_ENABLE_SHIFT)

#define MOD_CALIB_CFG2_NTH_ITER_AZ_ENABLE_SHIFT        (6)
#define MOD_CALIB_CFG2_NTH_ITER_AZ_ENABLE_MASK         (1 << MOD_CALIB_CFG2_NTH_ITER_AZ_ENABLE_SHIFT)

#define MOD_CALIB_CFG2_NTH_ITER_AGC_ENABLE_SHIFT       (5)
#define MOD_CALIB_CFG2_NTH_ITER_AGC_ENABLE_MASK        (1 << MOD_CALIB_CFG2_NTH_ITER_AGC_ENABLE_SHIFT)

#define MOD_CALIB_CFG2_RES_ENABLE_AUTO_CALIB_SHIFT     (4)
#define MOD_CALIB_CFG2_RES_ENABLE_AUTO_CALIB_MASK      (1 << MOD_CALIB_CFG2_RES_ENABLE_AUTO_CALIB_SHIFT)

/* VSYNC_PERIOD[7:0],[15:8] - 0xF2, 0xF3       */
/* VSYNC_PERIOD_TARGET[7:0],[6:0] - 0xF4, 0xF5 */
/* 0xF5:[7] ->  fast timing                    */
/* VSYNC_CONTROL - 0xF6 */

/* VSYNC_CFG - 0xF7 */

/* VSYNC_GPIO_INT - 0xF8 */

/* not used */

/* mod_fifo_data_cfg0 - 0xF9 */
#define MOD_ALS_FIFO_DATA0_WRITE_EN_SHIFT        (7)
#define MOD_FD_FIFO_DATA0_COMPRESS_EN_SHIFT      (5)
#define MOD_FD_FIFO_DATA0_DIFF_EN_SHIFT          (4)
#define MOD_FD_FIFO_DATA0_WIDTH_EN_SHIFT         (0)

/* mod_fifo_data_cfg1 - 0xFA */
#define MOD_ALS_FIFO_DATA1_WRITE_EN_SHIFT     MOD_ALS_FIFO_DATA0_WRITE_EN_SHIFT
#define MOD_FD_FIFO_DATA1_COMPRESS_EN_SHIFT   MOD_FD_FIFO_DATA0_COMPRESS_EN_SHIFT
#define MOD_FD_FIFO_DATA1_DIFF_EN_SHIFT       MOD_FD_FIFO_DATA0_DIFF_EN_SHIFT
#define MOD_FD_FIFO_DATA1_WIDTH_EN_SHIFT      MOD_FD_FIFO_DATA0_WIDTH_EN_SHIFT

/* mod_fifo_data_cfg2 - 0xFB*/
#define MOD_ALS_FIFO_DATA2_WRITE_EN_SHIFT     MOD_ALS_FIFO_DATA0_WRITE_EN_SHIFT
#define MOD_FD_FIFO_DATA2_COMPRESS_EN_SHIFT   MOD_FD_FIFO_DATA0_COMPRESS_EN_SHIFT
#define MOD_FD_FIFO_DATA2_DIFF_EN_SHIFT       MOD_FD_FIFO_DATA0_DIFF_EN_SHIFT
#define MOD_FD_FIFO_DATA2_WIDTH_EN_SHIFT      MOD_FD_FIFO_DATA0_WIDTH_EN_SHIFT

/* --------------------------------------------------*/

/* fifo_threshold - 0xFC      */
/* fifo_thr: - 0xFC [8:1]     */
/* fifo_thr: bit 0 is in CFG2 */
#define  FIFO_THRESH1_SHIFT     (1)
#define  FIFO_THRESH1_MASK      (0xFF)

/* fifo_level - 0xFD[9:2], 0xFE[1:0] */
#define FIFO_LEVEL1_SHIFT                    (2)  /* shift upper 8 bits of a 10 bit number */

/* fifo_status0 - 0xFE */
#define FIFO_STATUS0_OVERFLOW_SHIFT         (7)
#define FIFO_STATUS0_OVERFLOW_MASK          (1 << FIFO_STATUS0_OVERFLOW_SHIFT)

#define FIFO_STATUS0_UNDERFLOW_SHIFT        (6)
#define FIFO_STATUS0_UNDERFLOW_MASK         (1 << FIFO_STATUS0_UNDERFLOW_SHIFT)

#define FIFO_STATUS0_LVL0_SHIFT            (0)
#define FIFO_STATUS0_LVL0_MASK             (3 << FIFO_STATUS0_LVL0_SHIFT)

/* fifo_data - 0xFF */
/* used to read fifo normally */

/* ------------------------------------------------------------ */

typedef enum
{
    AGC_DISABLED            = 0,
    AGC_SAT_ENABLED         = 1,
    AGC_PREDICT_ENABLED     = 2,
    AGC_SAT_PREDICT_ENABLED = 3,
} agc_mode;

enum pwr_state
{
    POWER_ON      ,
    POWER_OFF     ,
    POWER_STANDBY ,
};

enum modx_gains
{
    MODX_HALF_X_GAIN            = 0x00,
    MODX_1_X_GAIN               = 0x01,
    MODX_2_X_GAIN               = 0x02,
    MODX_4_X_GAIN               = 0x03,
    MODX_8_X_GAIN               = 0x04,
    MODX_16_X_GAIN              = 0x05,
    MODX_32_X_GAIN              = 0x06,
    MODX_64_X_GAIN              = 0x07,
    MODX_128_X_GAIN             = 0x08,
    MODX_256_X_GAIN             = 0x09,
    MODX_512_X_GAIN             = 0x0A,
    MODX_1024_X_GAIN            = 0x0B,
    MODX_2048_X_GAIN            = 0x0C,
    MODX_4096_X_GAIN            = 0x0D,
};

#endif /* __TCS3410_HWDEF_H__ */

