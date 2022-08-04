/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_delay.h"
#include "ams_errno.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"
#include "tcs3410_irq.h"
#include "tcs3410_utils.h"
#include "tcs3410_fifo.h"
#include "tcs3410_als.h"
#include "ams_i2c.h"
#include "ams_device.h"
#include "ams_platform.h"

static char const * const VERSION = "1.1";
bool irq_log_enable = true;

/*
 * These are the registers that can be modified -
 * Some registers you do not want to write or overwrite
 */
static const uint8_t restorable_regs[] =
{
    REG_MEAS_MODE0         ,
    REG_MEAS_MODE1         ,
    REG_SAMPLE_TIME0       ,
    REG_SAMPLE_TIME1       ,
    REG_ALS_NR_SAMPLES0    ,
    REG_ALS_NR_SAMPLES1    ,
    REG_FD_NR_SAMPLES0     ,
    REG_FD_NR_SAMPLES1     ,
    REG_WTIME              ,
    REG_CFG0               ,            /* SAI */
    REG_CFG1               ,
    REG_CFG2               ,
    REG_CFG3               ,            /* Wait for Flicker, write ALS to FIFO */
    REG_CFG4               ,            /* step vs round interrupt */
    REG_CFG5               ,
    REG_CFG6               ,
    REG_CFG7               ,
    REG_CFG8               ,
    REG_CFG9               ,
    REG_AGC_NR_SAMPLES0    ,
    REG_AGC_NR_SAMPLES1    ,
    REG_TRIGGER_MODE       ,
    REG_CONTROL            ,
    REG_INTENAB            ,
    REG_SIEN               ,
    REG_MOD_COMP_CFG1      ,
    REG_MEAS_SEQR_FD_0     ,
    REG_MEAS_SEQR_ALS_FD_1 ,
    REG_MEAS_SEQR_APERS_AND_VSYNC_WAIT   ,
    REG_MEAS_SEQR_RESIDUAL_0             ,
    REG_MEAS_SEQR_RESIDUAL_1_AND_WAIT    ,
    REG_MEAS_SEQR_STEP0_MOD_GAINX_L      ,
    REG_MEAS_SEQR_STEP0_MOD_GAINX_H      ,
    REG_MEAS_SEQR_STEP1_MOD_GAINX_L      ,
    REG_MEAS_SEQR_STEP1_MOD_GAINX_H      ,
    REG_MEAS_SEQR_STEP2_MOD_GAINX_L      ,
    REG_MEAS_SEQR_STEP2_MOD_GAINX_H      ,
    REG_MEAS_SEQR_STEP3_MOD_GAINX_L      ,
    REG_MEAS_SEQR_STEP3_MOD_GAINX_H      ,
    REG_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_L  ,
    REG_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_H  ,
    REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_L  ,
    REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_H  ,
    REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_L  ,
    REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_H  ,   /* has agc pattern */
    REG_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_L  ,
    REG_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_H  ,
    REG_MOD_CALIB_CFG0     ,
    REG_MOD_CALIB_CFG2     ,
    REG_MOD_FIFO_DATA_CFG0 ,
    REG_MOD_FIFO_DATA_CFG1 ,
    REG_MOD_FIFO_DATA_CFG2 ,
    REG_FIFO_THR           ,
};

/*
 * APC is enabled for this MCU reference driver
 */
struct _device_ids
{
    uint8_t device;
    uint8_t rev;
    uint8_t aux;
};

/* The next two structures must match - multiple parts, single driver */
static struct _device_ids const dev_ids[] =
{
 { .device = 0x5C,.rev = 0x11,.aux = 0x01 },
};

static char const * const device_names[] =
{
    "tcs3410",
};

/* Holds the index into the dev_ids and device_names structures above */
static uint8_t selected_device;

/* state variables for the current i2c session */
static volatile ams_current_state_t current_state =
{
    .validated = false,
    .pon       = false,
    {    /* General */
        [AMS_FEATURE_ALS]     = AMS_FEATURE_DISABLE,
        [AMS_FEATURE_FLICKER] = AMS_FEATURE_DISABLE,
    },
    /* .sai */
    {
        AMS_SAI_ENABLE,               /* SAI is enabled during init */
        false,                        /* not active */
    },
    {    /* ALS */
        {0.0, 0.0, 0.0}, /* raw counts */
        {0.0, 0.0, 0.0}, /* norm. counts lux */
        {0, 0, 0},       /* gain regs */
        {0, 0, 0},       /* gain modulators */
        {0, 0, 0},       /* status bytes */
        .lux   = 0.0,
    },
    {   /* flicker */
        .fd_nr_samples = 0,
        .sample_freq   = 0.0,
        .gain_reg      = 0,
        .gain_mod      = 0,
        .end_marker    = false,
        .len           = 0,
        .freq          = 0.0,
    },
    {    /* fifo */
        .total_len  = 0,
        .level      = 0,
        .threshold  = 0,
        .overflow   = 0,
        .underflow  = 0,
        .fifo_state = AMS_FIFO_IDLE,
    },
};

/* Default values that are used to program the device */
/* ... user can override these with a config command.   */
static const ams_registers_t default_config =
{
    .sample_time     = DEFAULT_SAMPLE_TIME_REG,
    .mod_trigger     = DEFAULT_MOD_TRIGGER_REG, /* 2.844 ms */
    .wait_time       = DEFAULT_WAIT_TIME_REG,   /* 400 ms */
    .agc_mode        = AGC_PREDICT_ENABLED,
    .agc_nr_samples  = DEFAULT_AGC_NR_SAMPLES_REG,
    .als_nr_samples  = DEFAULT_ALS_NR_SAMPLES_REG,
    .fd_nr_samples   = DEFAULT_FD_NR_SAMPLES_REG,
    .fifo_threshold  = DEFAULT_FIFO_THRESH_REG,
};

/* holds the user changeable values */
static ams_registers_t current_config;

/* shadow view of the device registers */
static uint8_t shadow_regs[MAX_REGS]; /* shadows the HW */

uint8_t * const sh = &shadow_regs[0];

/* Indexed by the register value of gain - needs to be global */
const double gain_reg_2_gain[] =
{
    0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0, 256.0, 512.0, 1024.0,
    2048.0, 4096.0
};

/*
 *    Local Functions
 */
static bool validate_device_id(void)
{
    uint8_t buffer[3] = {0};
    struct _device_ids id =
    {
        .device = 0,
        .rev    = 0,
        .aux    = 0,
    };
    bool ret_val = false;
    uint8_t max_num_devices;

    AMS_LOG_PRINTF(LOG_INFO, "Sensor[0x%02X]: validating id.", SLAVE_ADDR_0);

       /* Validate actual versus expected: */
    /* REV_ID, DEVICE_ID, REV_ID2       */
    ams_i2c_block_read(SLAVE_ADDR_0, REG_AUXID, buffer, 3);
    id.rev     = (buffer[1] & REV_ID_MASK) >> REV_ID_SHIFT;
    id.device  = (buffer[2] & DEVICE_ID_MASK) >> DEVICE_ID_SHIFT;
    id.aux     = (buffer[0] & AUX_ID_MASK) >> AUX_ID_SHIFT;  /* rev_id2 (ver_id) */

    for (max_num_devices = 0; max_num_devices < ARRAY_SIZE(dev_ids); max_num_devices++)
    {
        AMS_LOG_PRINTF(LOG_INFO, "Sensor  --> ID=0x%02X REV=0x%02X AUX=0x%02X",
                    id.device, id.rev, id.aux);
        if ((id.device == dev_ids[max_num_devices].device) &&
            (id.rev    == dev_ids[max_num_devices].rev)    &&
            (id.aux    == dev_ids[max_num_devices].aux))
        {
            /* We have a match */
            AMS_LOG_PRINTF(LOG_INFO, "Sensor match : %s --> ID=0x%02X REV=0x%02X AUX=0x%02X",
                                      device_names[max_num_devices], id.device, id.rev, id.aux);
            ret_val = true;
            buffer[0] = 0xFF;
            /* clear all interrupts */
            ams_i2c_write(SLAVE_ADDR_0, sh, REG_STATUS, buffer[0]);
            selected_device = max_num_devices;
            current_state.validated = true;
            break;
        }
    }

    if (max_num_devices >= ARRAY_SIZE(dev_ids))
    {
        AMS_LOG_PRINTF(LOG_ERROR, "Sensor - Not supported chip id --> ID=0x%02X REV=0x%02X AUX=0x%02X",
                id.device, id.rev, id.aux);
    }
    return(ret_val);
}

/*
 *   Neelix Color Photo-Diode Config:
 *   PHD0: WB
 *   PHD1: Clear
 *   PHD2: WB
 *   PHD3: Green
 *   PHD4: Blue
 *   PHD5: Red
 *
 *   |--------------------------------------------------------|
 *   | Sequencer |     MOD0     |     MOD1     |     MOD2     |
 *   |   Step    |              |              |              |
 *   |--------------------------------------------------------|
 *   |           |              |              |              |
 *   |  STEP 0   |   Clear (1)  |   Green (3)  |   Red (5)    |
 *   |           |              |              |              |
 *   |--------------------------------------------------------|
 *   |           |              |              |              |
 *   |  STEP 1   |   Clear (1)  |   Blue (4)   |   WB (0,2)   |
 *   |           |              |              |              |
 *   ---------------------------------------------------------|
 *   |           |              |              |              |
 *   |  STEP 2   |   Clear (1)  |   Blue (4)   |   Red (5)    |
 *   |           |              |              |              |
 *   |--------------------------------------------------------|
 *   |           |    Clear,    |              |              |
 *   |  STEP 3   |  WB, Green   |   Blue (4)   |   Red (5)    |
 *   |           |  (0,1,2,3)   |              |              |
 *   |--------------------------------------------------------|
 *
 */
static void sensor_update_phd_smux(void)
{
    uint8_t val = 0;

    AMS_LOG_PRINTF(LOG_INFO, "Sensor - Updating SMUX connections");
    /* -------------- */
    /* --- Step 0 --- */
    /* -------------- */
    val = ((PHD_CONNECT_MOD0 << PHD1_SHIFT) |
           (PHD_CONNECT_MOD1 << PHD3_SHIFT)
          );
    sensor_write(REG_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_L, sh, val);

    val = (PHD_CONNECT_MOD2 << PHD5_SHIFT);
    sensor_write(REG_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_H, sh, val);

    /* -------------- */
    /* --- Step 1 --- */
    /* -------------- */
    val = ((PHD_CONNECT_MOD0 << PHD1_SHIFT) |
           (PHD_CONNECT_MOD2 << PHD0_SHIFT) |
           (PHD_CONNECT_MOD2 << PHD2_SHIFT)
          );
    sensor_write(REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_L, sh, val);

    val = (PHD_CONNECT_MOD1 << PHD4_SHIFT);
    sensor_write(REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_H, sh, val);

    /* -------------- */
    /* --- Step 2 --- */
    /* -------------- */
    val = (PHD_CONNECT_MOD0 << PHD1_SHIFT);
    sensor_write(REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_L, sh, val);

    val = ((PHD_CONNECT_MOD1 << PHD4_SHIFT) |
           (PHD_CONNECT_MOD2 << PHD5_SHIFT)
          );
    sensor_write(REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_H, sh, val);

    /* -------------- */
    /* --- Step 3 --- */
    /* -------------- */
    val = ((PHD_CONNECT_MOD0 << PHD0_SHIFT) |
           (PHD_CONNECT_MOD0 << PHD1_SHIFT) |
           (PHD_CONNECT_MOD0 << PHD2_SHIFT) |
           (PHD_CONNECT_MOD0 << PHD3_SHIFT)
          );
    sensor_write(REG_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_L, sh, val);

    val = ((PHD_CONNECT_MOD1 << PHD4_SHIFT) |
           (PHD_CONNECT_MOD2 << PHD5_SHIFT)
          );
    sensor_write(REG_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_H, sh, val);

    return;
}

static void sensor_init_shadow_regs(void)
{
    uint16_t idx = 0;

    for (idx = BASE_REGISTER; idx < MAX_REGS; idx += NUM_REGISTERS_READ)
    {
        sensor_read(idx, &shadow_regs[idx], NUM_REGISTERS_READ);
        nrf_delay_ms(200);
    }

    return;
}

static void sensor_flush_regs(void)
{
    uint16_t idx;
    uint16_t size = ARRAY_SIZE(restorable_regs);
    uint8_t reg;

    for (idx = 0; idx < size; idx++)
    {
        reg = restorable_regs[idx];
        ams_i2c_write_direct(SLAVE_ADDR_0, reg, shadow_regs[reg]);
    }
    return;
}

/*
 * Only using step 0 and 1, but the agc mode is set for all steps - no harm
 * Turn on Predict AGC
 * Turn off Saturation AGC
 */
static void sensor_set_agc_mode(agc_mode mode)
{
    uint8_t i2c_data = 0;
    if ((mode == AGC_SAT_ENABLED) || (mode == AGC_SAT_PREDICT_ENABLED))
    {
        /* Assume ASAT is turned on for all steps -> 0xF */
        i2c_data= (0x0F << MEASUREMENT_SEQUENCER_AGC_ASAT_PATTERN_SHIFT);
    }

    sensor_modify(REG_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_H, sh, MEASUREMENT_SEQUENCER_AGC_ASAT_PATTERN_MASK, i2c_data);

    i2c_data = 0x00;
    if ((mode == AGC_PREDICT_ENABLED) || (mode == AGC_SAT_PREDICT_ENABLED))
    {
        /* Assume Predict AGC is turned on for all steps -> 0xF */
        i2c_data= (0x0F << MEASUREMENT_SEQUENCER_AGC_PREDICT_PATTERN_SHIFT);
    }

    sensor_modify(REG_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_H, sh, MEASUREMENT_SEQUENCER_AGC_PREDICT_PATTERN_MASK, i2c_data);

    return;
}

void sensor_soft_reset(void)
{
    /* Make sure PON is enabled and all features are off */
    /* ???? Need to make sure OSC_EN is off before issiung soft reset */
    /* But PON enables OSC ??? */
    sensor_write(REG_ENABLE, sh, DEVICE_PON);
    sensor_modify(REG_CONTROL, sh, CONTROL_SOFT_RESET_MASK, CONTROL_SOFT_RESET_MASK);
    nrf_delay_ms(200);

    return;
}

/*
 * NOTES:
 *
 * 1. The alternate registers for Sample Time, ALS Samples and FD Samples are
 *    set to the same as the primary registers.  The Selection is set to 0 (Use
 *    the primary registers for all)
 *
 */
static void sensor_init_config(ams_registers_t *config)
{
    shadow_regs[REG_SAMPLE_TIME0] = (current_config.sample_time & SAMPLE_TIME0_MASK)  << SAMPLE_TIME0_SHIFT;
    shadow_regs[REG_SAMPLE_TIME1] = (current_config.sample_time >> SAMPLE_TIME1_SHIFT) & SAMPLE_TIME1_MASK;

    shadow_regs[REG_MEAS_MODE0]   = ((MEAS_MODE0_MEAS_SEQR_SINGLE_SHOT_MODE_MASK)   |
                                     (MEAS_MODE0_FIFO_ALS_STATUS_WRITE_ENABLE_MASK) |
                                     (0x04) /* ALS SCALE - # of MSBs that must be 0 */
                                    );
    shadow_regs[REG_MEAS_MODE1]   = ((MEAS_MODE1_MOD_FIFO_FD_END_MARKER_WRITE_EN_MASK) |
                                     (MEAS_MODE1_MOD_FIFO_FD_GAIN_WRITE_EN_MASK)       |
                                     (DEFAULT_ALS_MSB_POSITION << MEAS_MODE1_ALS_MSB_POSITION_SHIFT));

    shadow_regs[REG_ALS_NR_SAMPLES0]     = (current_config.als_nr_samples & ALS_NR_SAMPLES0_MASK) << ALS_NR_SAMPLES0_SHIFT;
    shadow_regs[REG_ALS_NR_SAMPLES1]     = (current_config.als_nr_samples >> ALS_NR_SAMPLES1_SHIFT) & ALS_NR_SAMPLES1_MASK;

    shadow_regs[REG_FD_NR_SAMPLES0]      = (current_config.fd_nr_samples & FD_NR_SAMPLES0_MASK) << FD_NR_SAMPLES0_SHIFT;
    shadow_regs[REG_FD_NR_SAMPLES1]      = (current_config.fd_nr_samples >> FD_NR_SAMPLES1_SHIFT) & FD_NR_SAMPLES1_MASK;

    shadow_regs[REG_WTIME]               = (current_config.wait_time & WTIME_MASK) << WTIME_SHIFT;

    /* Not using thresholds or AINT  - ALS data written to FIFO */
    shadow_regs[REG_AILT0]   = 0x00;
    shadow_regs[REG_AILT1]   = 0x00;
    shadow_regs[REG_AILT2]   = 0x00;
    shadow_regs[REG_AIHT0]   = 0xFF;
    shadow_regs[REG_AIHT1]   = 0xFF;
    shadow_regs[REG_AIHT2]   = 0xFF;

    /* REG_STATUS thru REG_STATUS6 - no initial config */

    shadow_regs[REG_CFG0] = CFG0_SAI_MASK | 0x08; /* do not overwrite lower 5 bits */
    shadow_regs[REG_CFG1] = 0; // CFG1_MASK_DO_ALS_FINAL_PROCESSING: Only needed if fd and als in same step
    shadow_regs[REG_CFG2] = current_config.fifo_threshold & CFG2_FIFO_THRESH0_MASK;
    shadow_regs[REG_CFG3] = 0;  // no pinmap or vsync config
    shadow_regs[REG_CFG4] = ((SINT_BY_ROUND << CFG4_MEAS_SEQR_SINT_PER_STEP_SHIFT) |   /* bit 6 must be 0 for AGC to work properly */
                             (ALS_DATA_FORMAT_24_BIT << CFG4_MOD_ALS_FIFO_DATA_FORMAT_SHIFT)
                            );
    shadow_regs[REG_CFG5] = 0; // no als_threshold channel and no persistance
    shadow_regs[REG_CFG6] = (DEFAULT_MAX_RESIDUAL_BITS << CFG6_MOD_MAX_RESIDUAL_BITS_SHIFT);
    shadow_regs[REG_CFG7] = 1; // reserved set to 0x1;
    shadow_regs[REG_CFG8] = (MODX_512_X_GAIN << CFG8_MEAS_SEQR_MAX_MOD_GAIN_SHIFT) |
                            (MODX_8_X_GAIN << CFG8_MEAS_SEQR_AGC_PREDICT_MOD_GAIN_REDUCT_SHIFT);
    shadow_regs[REG_CFG9] = 0; // residual bits ignore

    shadow_regs[REG_AGC_NR_SAMPLES0] = (current_config.agc_nr_samples &  AGC_NR_SAMPLES0_MASK) << AGC_NR_SAMPLES0_SHIFT;
    shadow_regs[REG_AGC_NR_SAMPLES1] = (current_config.agc_nr_samples >> AGC_NR_SAMPLES1_SHIFT) & AGC_NR_SAMPLES1_MASK;

    shadow_regs[REG_TRIGGER_MODE]   = (uint8_t)(current_config.mod_trigger & TRIGGER_MODE_MASK);

    shadow_regs[REG_CONTROL]        = (CONTROL_CLR_SAI_ACTIVE_MASK | CONTROL_FIFO_CLR_MASK); // clear fifo and SAI

    shadow_regs[REG_INTENAB]        = (INTENAB_MIEN_MASK | INTENAB_SIEN_MASK | INTENAB_FIEN_MASK);
    shadow_regs[REG_SIEN]           = SIEN_MEASUREMENT_SEQUENCER_MASK;

    shadow_regs[REG_MOD_COMP_CFG1]   = (IDAC_9uV << MOD_COMP_CFG1_IDAC_RANGE_SHIFT);

     /* Flicker only active on step 3, modulator 0 */
    shadow_regs[REG_MEAS_SEQR_FD_0] =     ((FD_OFF_ALL_STEPS << FD_ACTIVE_MOD1_SHIFT) | (FD_ACTIVE_SEQ_STEP_3 << FD_ACTIVE_MOD0_SHIFT));
    shadow_regs[REG_MEAS_SEQR_ALS_FD_1] = (FD_OFF_ALL_STEPS << FD_ACTIVE_MOD2_SHIFT) |
                                          ((ALS_ACTIVE_SEQ_STEP_0 | ALS_ACTIVE_SEQ_STEP_1 | ALS_ACTIVE_SEQ_STEP_2) << SHIFT_ALS_ACTIVE_ALL_MODS_SHIFT);

    /* */
    shadow_regs[REG_MEAS_SEQR_APERS_AND_VSYNC_WAIT]    = ((VSYNC_WAIT_OFF_ALL_STEPS << VSYNC_WAIT_MODS_SHIFT) |
                                                          (ALS_PERSIST_OFF_ALL_STEPS << ALS_PERSIST_ALL_MODS_SHIFT));

    shadow_regs[REG_MEAS_SEQR_RESIDUAL_0]              = (((RESIDUALS_ENABLE_SEQ_STEP_0 | RESIDUALS_ENABLE_SEQ_STEP_1) << RESIDUAL_ENABLE_MOD1_SHIFT) |
                                                          ((RESIDUALS_ENABLE_SEQ_STEP_0 | RESIDUALS_ENABLE_SEQ_STEP_1) << RESIDUAL_ENABLE_MOD0_SHIFT));
    shadow_regs[REG_MEAS_SEQR_RESIDUAL_1_AND_WAIT]     = (((WAIT_ENABLE_SEQ_STEP_0 | WAIT_ENABLE_SEQ_STEP_1) << WAIT_ENABLE_SHIFT) |
                                                          ((RESIDUALS_ENABLE_SEQ_STEP_0 | RESIDUALS_ENABLE_SEQ_STEP_1) << RESIDUAL_ENABLE_MOD2_SHIFT));

    /* These regsiters will be overwritten by the AGC function */
    shadow_regs[REG_MEAS_SEQR_STEP0_MOD_GAINX_L]     = ((M_128_X_GAIN << GAIN_MOD1_SHIFT) | (M_128_X_GAIN << GAIN_MOD0_SHIFT));
    shadow_regs[REG_MEAS_SEQR_STEP0_MOD_GAINX_H]     = (M_128_X_GAIN << GAIN_MOD2_SHIFT);
    shadow_regs[REG_MEAS_SEQR_STEP1_MOD_GAINX_L]     = ((M_128_X_GAIN << GAIN_MOD1_SHIFT) | (M_128_X_GAIN << GAIN_MOD0_SHIFT));
    shadow_regs[REG_MEAS_SEQR_STEP1_MOD_GAINX_H]     = (M_128_X_GAIN << GAIN_MOD2_SHIFT);
    shadow_regs[REG_MEAS_SEQR_STEP2_MOD_GAINX_L]     = ((M_128_X_GAIN << GAIN_MOD1_SHIFT) | (M_128_X_GAIN << GAIN_MOD0_SHIFT));
    shadow_regs[REG_MEAS_SEQR_STEP2_MOD_GAINX_H]     = (M_128_X_GAIN << GAIN_MOD2_SHIFT);
    shadow_regs[REG_MEAS_SEQR_STEP3_MOD_GAINX_L]     = ((M_128_X_GAIN << GAIN_MOD1_SHIFT) | (M_128_X_GAIN << GAIN_MOD0_SHIFT));
    shadow_regs[REG_MEAS_SEQR_STEP3_MOD_GAINX_H]     = (M_128_X_GAIN << GAIN_MOD2_SHIFT);

    /* carried over from betz mcu driver and linux tcs3410 driver values */
    sensor_update_phd_smux();

    shadow_regs[REG_MOD_CALIB_CFG0] = 0x01; /* nth iteration to 1 */

    shadow_regs[REG_MOD_CALIB_CFG2] = ((1 << MOD_CALIB_CFG2_NTH_ITER_RC_ENABLE_SHIFT) |
				       (1 << MOD_CALIB_CFG2_NTH_ITER_AZ_ENABLE_SHIFT) |
				       (1 << MOD_CALIB_CFG2_NTH_ITER_AGC_ENABLE_SHIFT) |
				       (1 << MOD_CALIB_CFG2_RES_ENABLE_AUTO_CALIB_SHIFT) |
				       (0x03) /* reserved value */
				      );

    sensor_set_agc_mode(current_config.agc_mode);

    /* als on mod0, 1, 2.  fd on mod0 */
    shadow_regs[REG_MOD_FIFO_DATA_CFG0] = ((1 << MOD_ALS_FIFO_DATA0_WRITE_EN_SHIFT)   |
                                           (1 << MOD_FD_FIFO_DATA0_COMPRESS_EN_SHIFT) |
                                           (1 << MOD_FD_FIFO_DATA0_DIFF_EN_SHIFT)     |
                                           (5 << MOD_FD_FIFO_DATA0_WIDTH_EN_SHIFT)
                                          );
    shadow_regs[REG_MOD_FIFO_DATA_CFG1] = ((1 << MOD_ALS_FIFO_DATA1_WRITE_EN_SHIFT)   |
                                           (0 << MOD_FD_FIFO_DATA1_COMPRESS_EN_SHIFT) |
                                           (0 << MOD_FD_FIFO_DATA1_DIFF_EN_SHIFT)     |
                                           (0 << MOD_FD_FIFO_DATA1_WIDTH_EN_SHIFT)
                                          );

    shadow_regs[REG_MOD_FIFO_DATA_CFG2] = ((1 << MOD_ALS_FIFO_DATA2_WRITE_EN_SHIFT)   |
                                           (0 << MOD_FD_FIFO_DATA2_COMPRESS_EN_SHIFT) |
                                           (0 << MOD_FD_FIFO_DATA2_DIFF_EN_SHIFT)     |
                                           (0 << MOD_FD_FIFO_DATA2_WIDTH_EN_SHIFT)
                                          );

    shadow_regs[REG_FIFO_THR]           = ((current_config.fifo_threshold >> FIFO_THRESH1_SHIFT) & FIFO_THRESH1_MASK);

    sensor_flush_regs();

    return;
}

static ams_errno_t sensor_config_base(ams_sensor_config_t *cfg)
{
    uint8_t i2c_reg_enable = 0, i2c_data = 0;

    /* current features that are enabled */
    sensor_read(REG_ENABLE, &i2c_reg_enable, 1);

    /* Make sure all features are off */
    sensor_modify(REG_ENABLE, sh, DEVICE_EN_ALL, 0);

    /* sample time */
    i2c_data = (uint8_t)((cfg->sample_time & SAMPLE_TIME0_MASK));
    sensor_write(REG_SAMPLE_TIME0, sh, i2c_data);

    i2c_data = (uint8_t)((cfg->sample_time >> SAMPLE_TIME1_SHIFT) & SAMPLE_TIME1_MASK);
    sensor_modify(REG_SAMPLE_TIME1, sh, SAMPLE_TIME1_MASK, i2c_data);

    /* agc_nr_samples */
    i2c_data = (uint8_t)(cfg->agc_nr_samples & AGC_NR_SAMPLES0_MASK);
    sensor_write(REG_AGC_NR_SAMPLES0, sh, i2c_data);

    i2c_data = (uint8_t)((cfg->agc_nr_samples >> AGC_NR_SAMPLES1_SHIFT) & AGC_NR_SAMPLES1_MASK);
    sensor_modify(REG_AGC_NR_SAMPLES1, sh, AGC_NR_SAMPLES1_MASK, i2c_data);

    /* mod_trigger_timing */
    i2c_data = (uint8_t)((cfg->mod_trigger & TRIGGER_MODE_MASK));
    sensor_modify(REG_TRIGGER_MODE, sh, TRIGGER_MODE_MASK, i2c_data);

    /* wait_time */
    i2c_data = (uint8_t)cfg->wait_time;
    sensor_write(REG_WTIME, sh, i2c_data);

    sensor_set_agc_mode(cfg->agc_mode);

    /* update the current values locally */
    current_config.sample_time     = cfg->sample_time;
    current_config.agc_mode        = cfg->agc_mode;
    current_config.agc_nr_samples  = cfg->agc_nr_samples;

    current_config.mod_trigger     = cfg->mod_trigger;
    current_config.wait_time       = cfg->wait_time;

    /* update sampling frequency if sample time changes */
    current_state.fd.sample_freq =  DEFAULT_MODULATOR_CLOCK_HZ/(current_config.sample_time + 1);

    /* Return reg enable to original state */
    sensor_write(REG_ENABLE, sh, i2c_reg_enable);

    return(AMS_SUCCESS);
}

static ams_errno_t sensor_config_als(ams_sensor_config_t *cfg)
{
    uint8_t i2c_reg_enable = 0, i2c_data = 0;

    /* current features that are enabled */
    sensor_read(REG_ENABLE, &i2c_reg_enable, 1);

    /* Make sure all features are off but leave PON */
    sensor_modify(REG_ENABLE, sh, DEVICE_EN_ALL, 0);

    /* als_nr_samples */
    i2c_data = cfg->als_nr_samples & ALS_NR_SAMPLES0_MASK;
    sensor_write(REG_ALS_NR_SAMPLES0, sh, i2c_data);

    i2c_data = ((cfg->als_nr_samples >> ALS_NR_SAMPLES1_SHIFT) & ALS_NR_SAMPLES1_MASK);
    sensor_modify(REG_ALS_NR_SAMPLES1, sh, ALS_NR_SAMPLES1_MASK, i2c_data);
    current_config.als_nr_samples  = cfg->als_nr_samples;

    /* Return system to original state */
    sensor_write(REG_ENABLE, sh, i2c_reg_enable);

    return(AMS_SUCCESS);
}

static ams_errno_t sensor_config_fd(ams_sensor_config_t *cfg)
{
    uint8_t i2c_reg_enable = 0, i2c_data = 0;

    /* current features that are enabled */
    sensor_read(REG_ENABLE, &i2c_reg_enable, 1);

    /* Make sure all features are off */
    sensor_modify(REG_ENABLE, sh, DEVICE_EN_ALL, 0);

    /* fd_nr_samples */
    i2c_data = cfg->fd_nr_samples & FD_NR_SAMPLES0_MASK;
    sensor_write(REG_FD_NR_SAMPLES0, sh, i2c_data);

    i2c_data = ((cfg->fd_nr_samples >> FD_NR_SAMPLES1_SHIFT) & FD_NR_SAMPLES1_MASK);
    sensor_modify(REG_FD_NR_SAMPLES1, sh, FD_NR_SAMPLES1_MASK, i2c_data);

    current_config.fd_nr_samples  = cfg->fd_nr_samples;
    current_state.fd.fd_nr_samples = current_config.fd_nr_samples;

    /* Return system to original state */
    sensor_write(REG_ENABLE, sh, i2c_reg_enable);

    return(AMS_SUCCESS);
}

static ams_errno_t sensor_config_fifo(ams_sensor_config_t *cfg)
{
    uint8_t i2c_reg_enable = 0, i2c_data = 0;
    ams_errno_t ret_val = AMS_SUCCESS;

    /* current features that are enabled */
    sensor_read(REG_ENABLE, &i2c_reg_enable, 1);

    /* Make sure all features are off */
    sensor_modify(REG_ENABLE, sh, DEVICE_EN_ALL, 0);

    i2c_data = cfg->fifo_threshold & CFG2_FIFO_THRESH0_MASK;
    sensor_modify(REG_CFG2, sh, CFG2_FIFO_THRESH0_MASK, i2c_data);

    i2c_data = ((cfg->fifo_threshold >> FIFO_THRESH1_SHIFT) & FIFO_THRESH1_MASK);
    sensor_write(REG_FIFO_THR, sh, i2c_data);

    current_config.fifo_threshold  = cfg->fifo_threshold;

    /* Determine if fifo_reset is needed */
    if (cfg->fifo_reset)
    {
        ret_val = sensor_fifo_reset();
    }

    /* Return system to original state */
    sensor_write(REG_ENABLE, sh, i2c_reg_enable);

    return(ret_val);
}

/* =================================================================================== */

/*
 * Global Functions
 */

void sensor_blk_write(uint8_t reg, uint8_t *buffer, uint8_t bytes)
{
    (void)ams_i2c_block_write(SLAVE_ADDR_0, reg, buffer, bytes);
    return;
}

void sensor_write(uint8_t reg, uint8_t *sh, uint8_t val)
{
    (void)ams_i2c_write(SLAVE_ADDR_0, sh, reg, val);
    return;
}

void sensor_read(uint8_t reg, uint8_t *buffer, uint8_t bytes)
{
    (void)ams_i2c_block_read(SLAVE_ADDR_0, reg, buffer, bytes);
    return;
}

void sensor_modify(uint8_t reg, uint8_t *sh, uint8_t mask, uint8_t val)
{
    (void)ams_i2c_modify(SLAVE_ADDR_0, sh, reg, mask, val);
    return;
}

bool sensor_get_validated(void)
{
    return(current_state.validated);
}

volatile ams_current_state_t *sensor_get_current_state(void)
{
    return(&current_state);
}

ams_errno_t sensor_status(void *data)
{
    ams_device_status_t *stat = (ams_device_status_t *)data;
    uint8_t i2c_buff;

    memset(stat, 0, sizeof(ams_device_status_t));

    stat->pon        = current_state.pon;
    stat->log_irq    = irq_log_enable;
    stat->als_en     = current_state.features[AMS_FEATURE_ALS];
    stat->fd_en      = current_state.features[AMS_FEATURE_FLICKER];

    sensor_read(REG_STATUS4, &i2c_buff, 1);
    current_state.sai.active = (i2c_buff & STATUS4_SAI_ACTIVE) >> STATUS4_SAI_ACTIVE_SHIFT;
    stat->sai        = current_state.sai;

    stat->als        = current_state.als;
    stat->fd         = current_state.fd;
    stat->fifo       = current_state.fifo;

    return(AMS_SUCCESS);
}

ams_errno_t sensor_setup(void *data)
{
    ams_sensor_config_t *cfg = (ams_sensor_config_t *)data;

    cfg->sample_time     = current_config.sample_time;
    cfg->mod_trigger     = current_config.mod_trigger;
    cfg->wait_time       = current_config.wait_time;
    cfg->agc_mode        = current_config.agc_mode;
    cfg->agc_nr_samples  = current_config.agc_nr_samples;
    cfg->als_nr_samples  = current_config.als_nr_samples;
    cfg->fd_nr_samples   = current_config.fd_nr_samples;
    cfg->fifo_threshold  = current_config.fifo_threshold;

    return(AMS_SUCCESS);
}

/* During init, the fifo will be reset for the first pass */
ams_errno_t sensor_enable(ams_feature_t feature, ams_feature_enable_t enable)
{
    ams_errno_t ret = AMS_SUCCESS;
    uint8_t  i2c_data = 0;
    bool valid_feature = false;

    /* If SAI is active, reg_enable is not properly updated */
    /* When in Sleep mode, some registers are not accessible */
    switch (feature)
    {
        case AMS_FEATURE_ALS:
        {
            if ((current_state.features[AMS_FEATURE_ALS] != enable) ||
                (current_state.pon == false))
            {
                valid_feature = true;
                current_state.features[AMS_FEATURE_ALS] = enable;
                current_state.pon = enable;
                if (enable == AMS_FEATURE_ENABLE)
                {
                    i2c_data = DEVICE_AEN | DEVICE_PON;
                }
                else
                {
                    i2c_data = 0;
                }
                sensor_modify(REG_ENABLE, sh, DEVICE_AEN | DEVICE_PON, i2c_data);
            }
            break;
        }
        case AMS_FEATURE_FLICKER:
        {
            if ((current_state.features[AMS_FEATURE_FLICKER] != enable) ||
               (current_state.pon == false))
            {
                valid_feature = true;
                current_state.features[AMS_FEATURE_FLICKER] = enable;
                current_state.pon = enable;
                if (enable == AMS_FEATURE_ENABLE)
                {
                    i2c_data = DEVICE_FDEN| DEVICE_PON;
                }
                else
                {
                    i2c_data = 0;
                }
                sensor_modify(REG_ENABLE, sh, DEVICE_FDEN | DEVICE_PON, i2c_data);
            }
            break;
        }
        case AMS_FEATURE_ALL:
        {
            /* allow user to power off device - do not protect delta states */
            valid_feature = true;
            current_state.features[AMS_FEATURE_ALS]     = enable;
            current_state.features[AMS_FEATURE_FLICKER] = enable;
            current_state.pon= enable;
            if (enable == AMS_FEATURE_ENABLE)
            {
                i2c_data = DEVICE_EN_ALL;
            }
            else
            {
                /* turn off chip if turning off all features */
                i2c_data = 0;
            }
            sensor_modify(REG_ENABLE, sh, DEVICE_EN_ALL, i2c_data);
            break;
        }
        default:
        {
            ret = AMS_CLI_FAILURE;
            AMS_LOG_PRINTF(LOG_ERROR, "Feature not available on this device");
            break;
        }
    }

    /* if a valid feature is passed and disabled, clear the fifo for the next time */
    if (valid_feature && (enable == AMS_FEATURE_DISABLE))
    {
        sensor_fifo_reset();
    }
    return(ret);
}

ams_errno_t sensor_pon(bool on_off)
{
    uint8_t i2c_data = 0;

    if (on_off)
    {
        i2c_data = DEVICE_PON;
    }

    sensor_modify(REG_ENABLE, sh, DEVICE_PON, i2c_data);
    current_state.pon = on_off;

    return(AMS_SUCCESS);
}

ams_errno_t sensor_log_irq(bool on_off)
{
    irq_log_enable = on_off;

    return(AMS_SUCCESS);
}

/*
 *  It is up to the user to disable ALS and Flicker before changing these
 *
 *  If SAI is active, changes are not reflected in the enable register.
 *  SAI must be cleared in order to modify the enable bits
 */
ams_errno_t sensor_sai(ams_sai_state_t state)
{
    ams_errno_t ret = AMS_SUCCESS;

    switch (state)
    {
        case AMS_SAI_ENABLE:
        {
            if (current_state.sai.sai != AMS_SAI_ENABLE)
            {
                sensor_write(REG_CFG0, sh, CFG0_SAI_MASK);
                current_state.sai.sai = AMS_SAI_ENABLE;
            }
            break;
        }
        case AMS_SAI_DISABLE:
        {
            if (current_state.sai.sai != AMS_SAI_DISABLE)
            {
                sensor_write(REG_CFG0, sh, 0);
                current_state.sai.sai = AMS_SAI_DISABLE;
            }
            break;
        }
        case AMS_SAI_CLEAR:
        {
            sensor_clear_sai();
            break;
        }
        default:
        {
            AMS_LOG_PRINTF(LOG_ERROR, "State not available for SAI [enable|disable|clear]");
            ret = AMS_CLI_FAILURE;
            break;
        }
    }
    return(ret);
}

/* Used by the CLI - the most common parameters to adjust*/
ams_errno_t sensor_config(ams_config_feature_t cfg_type, void *cfg)
{
    ams_sensor_config_t *cfg_data = (ams_sensor_config_t *)cfg;
    ams_errno_t ret = AMS_SUCCESS;
    switch (cfg_type)
    {
        case AMS_CONFIG_BASE:
        {
            ret = sensor_config_base(cfg_data);
            break;
        }
        case AMS_CONFIG_ALS:
        {
            ret = sensor_config_als(cfg_data);
            break;
        }
        case AMS_CONFIG_FD:
        {
            ret = sensor_config_fd(cfg_data);
            break;
        }
        case AMS_CONFIG_FIFO:
        {
            ret = sensor_config_fifo(cfg_data);
            break;
        }
        default:
        {
            ret = AMS_CLI_FAILURE;
            break;
        }
    }
    return(ret);
}

void sensor_clear_sai(void)
{
    AMS_LOG_PRINTF_IRQ(LOG_INFO, "Clearing SAI_ACTIVE");
    sensor_modify(REG_CONTROL, sh, CONTROL_CLR_SAI_ACTIVE_MASK,
                  CONTROL_CLR_SAI_ACTIVE_MASK);
    return;
}

uint16_t sensor_get_als_nr_samples(void)
{
    return(current_config.als_nr_samples);
}

uint16_t sensor_get_sample_time(void)
{
    return(current_config.sample_time);
}

/*
 * Entry point for all interrupts - tcs3410_irq.c module handles
 * the processing of all interrupts.
 */
void ams_sensor_irq_handler(__attribute__((unused)) uint32_t pin)
{
    sensor_process_irq(sh);

    return;
}

ams_errno_t ams_sensor_init(struct ams_device *device)
{
    uint8_t i2c_buff;
    ams_errno_t ret_val = AMS_SUCCESS;

    device->version = VERSION;
    current_state.validated = false;

    if (!validate_device_id())
    {
        AMS_LOG_PRINTF(LOG_ERROR, "Sensor failed to validate");
        return(AMS_DEVICE_VALIDATE_ERROR);
    }

    device->write     = sensor_write;
    device->read      = sensor_read;
    device->enable    = sensor_enable;
    device->sai       = sensor_sai;
    device->pon       = sensor_pon;
    device->log_irq   = sensor_log_irq;
    device->configure = sensor_config;
    device->status    = sensor_status;
    device->setup     = sensor_setup;

    i2c_buff = 0;
    /* disable all features - als and flicker */
    sensor_write(REG_ENABLE, sh, i2c_buff);

    /* Zero out irq enables - added for polling mode */
    sensor_write(REG_INTENAB, sh, i2c_buff);

    current_config = default_config;
    memset(shadow_regs, 0, MAX_REGS*sizeof(uint8_t));

    /* Get a copy of the hardware registers */
    sensor_init_shadow_regs();

    /* Update with the wanted configuration */
    sensor_init_config((void *)&default_config);

    /* update needed for CLI status commands */
    current_state.fd.fd_nr_samples = current_config.fd_nr_samples;
    current_state.fd.sample_freq =  DEFAULT_MODULATOR_CLOCK_HZ/(current_config.sample_time + 1);

    AMS_LOG_PRINTF(LOG_INFO, "Sensor initialization complete.");

    return(ret_val);
}
