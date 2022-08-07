/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __TCS3410_H__
#define __TCS3410_H__

#include "ams_device.h"
#include "ams_errno.h"
#include "math_util.h"


/* pointer to shadow registers */
extern uint8_t * const sh;

extern bool irq_log_enable;
/* Provides more detailed information in the logging */
/* ... However, does not work well from INT context  */
#define TURN_ON_IRQ_LOG

/* tcs3410 specific */
#define DEFAULT_MODULATOR_PERIOD_SEC    (1.388889E-06)
#define DEFAULT_MODULATOR_CLOCK_HZ      (720000) /* inverse of 1.388889e-06 sec */

/* Define a macro for ISR context logging.  In addition, irq logging can be */
/* enabled/disabled via cli */
#if defined(TURN_ON_IRQ_LOG)
#define AMS_LOG_PRINTF_IRQ(...)                          \
                                if (irq_log_enable)      \
                                {                        \
                                    AMS_LOG_PRINTF(__VA_ARGS__);\
                                }
#else
#define AMS_LOG_PRINTF_IRQ(...)
#endif

#define  NUM_REGISTERS_READ                (16)
#define  BASE_REGISTER                    (0x80)

/*
 * Instances of this structure must be declared volatile
 */
typedef struct _current_state
{
    bool              validated;
    bool              pon;
    ams_feature_enable_t features[AMS_FEATURES_END];
    ams_sai_info_t    sai;
    ams_als_info_t    als;
    ams_fd_info_t     fd;
    ams_fifo_info_t   fifo;
} ams_current_state_t;

/* does not have to match the ams_config structure - this will store not changeable */
/* values. */
typedef struct _config
{
    /* Base Configuration */
    uint16_t     sample_time;
    uint16_t     mod_trigger;
    uint16_t     wait_time;
    agc_mode     agc_mode;
    uint16_t     agc_nr_samples;
    /* ALS Configuration */
    uint16_t     als_nr_samples;
    /* FD Configuration */
    uint16_t     fd_nr_samples;
    /* FIFO Configuration */
    uint16_t     fifo_threshold;
} ams_registers_t;

typedef enum
{
    ALS_STATUS_SAT,
    ALS_STATUS_SCALED,
    ALS_STATUS_STEP,
} ams_als_status_t;

typedef enum
{
    MOD_TRG_TIME_OFF,
    MOD_TRG_TIME_NORM,
    MOD_TRG_TIME_LONG,
    MOD_TRG_TIME_FAST,
    MOD_TRG_TIME_FASTLONG,
    MOD_TRG_TIME_VSYNC,
} ams_mod_trigger_timing_t;

#define DEFAULT_SAMPLE_TIME_REG                  (179)
#define DEFAULT_MOD_TRIGGER_REG    (MOD_TRG_TIME_NORM)
#define DEFAULT_WAIT_TIME_REG                    (108)
#define DEFAULT_AGC_NR_SAMPLES_REG                (19)
#define DEFAULT_ALS_NR_SAMPLES_REG               (399)
#define DEFAULT_FD_NR_SAMPLES_REG                (511)
#define DEFAULT_FIFO_THRESH_REG                  (499)


extern const fp_t gain_reg_2_gain[];

void sensor_write(uint8_t reg, uint8_t *sh, uint8_t val);
void sensor_blk_write(uint8_t reg, uint8_t *buffer, uint8_t bytes);
void sensor_read(uint8_t reg, uint8_t *buffer, uint8_t bytes);
void sensor_modify(uint8_t reg, uint8_t *sh, uint8_t mask, uint8_t val);

ams_errno_t sensor_enable(ams_feature_t feature, ams_feature_enable_t enable);
void sensor_clear_sai(void);
ams_errno_t sensor_sai(ams_sai_state_t state);
ams_errno_t sensor_config(ams_config_feature_t cfg_type, void *cfg);

ams_errno_t sensor_status(void *data);
ams_errno_t sensor_setup(void *data);
bool sensor_get_validated(void);
ams_errno_t sensor_log_irq(bool on_off);
ams_errno_t sensor_pon(bool on_off);
uint16_t sensor_get_als_nr_samples(void);
uint16_t sensor_get_sample_time(void);
volatile ams_current_state_t *sensor_get_current_state(void);

#endif /* __TCS3410_H__ */

