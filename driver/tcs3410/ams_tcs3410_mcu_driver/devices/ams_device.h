/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __AMS_DEVICE_H__
#define __AMS_DEVICE_H__

#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "ams_errno.h"
#include "math_util.h"

#define NUM_PDS              (3) /* actually number of ADC - each ADC has 2 PDs muxed */
#define NUM_STATUS_BYTES     (3)

#define FLCKR_BUFFER_SZ      (1024)
#define ALS_FIFO_DATA_LENGTH (36)
#define ALS_NUM_STEPS        (3)
#define ALS_NUM_STATUS_REGS  (3)
#define ALS_STATUS_REG_INDEX      (0)  /* saturation info, scaled status  */
#define ALS_STATUS2_REG_INDEX     (1)  /* mod 0 and 1 gain */
#define ALS_STATUS3_REG_INDEX     (2)  /* mod 2 gain */

typedef enum
{
    AMS_FEATURE_ALS,
    AMS_FEATURE_FLICKER,
    AMS_FEATURE_ALL,
    AMS_FEATURES_END
} ams_feature_t;

typedef enum
{
    AMS_SAI_ENABLE,
    AMS_SAI_DISABLE,
    AMS_SAI_CLEAR,
    AMS_SAI_END
} ams_sai_state_t;

typedef enum
{
    AMS_FEATURE_DISABLE   = 0,
    AMS_FEATURE_ENABLE    = 1,
} ams_feature_enable_t;

typedef enum
{
    AMS_OFF    = 0,
    AMS_ON     = 1,
} ams_on_off_t;

typedef enum
{
    AMS_CONFIG_BASE,
    AMS_CONFIG_ALS,
    AMS_CONFIG_FD,
    AMS_CONFIG_FIFO,
} ams_config_feature_t;

typedef enum
{
    PD_RED,
    PD_GREEN,
    PD_BLUE,
    PD_CLEAR,
    PD_WB,
    PD_END
} ams_pd_type_t;

typedef enum
{
    MODULATOR_0    ,
    MODULATOR_1    ,
    MODULATOR_2    ,
    NUM_MODULATORS ,
} ams_modulator_t;

typedef enum
{
   STEP_0    ,
   STEP_1    ,
   STEP_2    ,
   STEP_3    ,
   NUM_STEPS ,
} ams_step_t;

typedef enum
{
   RED_PD      ,
   GREEN_PD    ,
   BLUE_PD     ,
   WIDE_BAND_PD,
   CLEAR_PD    ,
   NUM_PD      ,
} ams_pd_filters_t;

typedef enum
{
   RED_COEFF  ,
   GREEN_COEFF,
   BLUE_COEFF ,
   WB_COEFF   ,
   CLEAR_COEFF,
   DGF        ,
} ams_als_coeff_t;

typedef enum
{
   COEFA  ,
   CTOFFSET,
} ams_cct_coeffs_t;

typedef enum
{
   N_LO ,
   N_MED,
   N_HI ,
   N_MAX,
} ams_ircomp_c_t;

/* mapped to names of als _status registers within the device */
typedef enum
{
    ALS_STATUS,
    ALS_STATUS2,
    ALS_STATUS3,
    ALS_STATUS_END
} ams_als_status_reg_t;

typedef struct _ams_als_info_t
{
    uint32_t mod_counts[NUM_MODULATORS];              /* raw counts from the device */
    fp_t   mod_normalized_counts[NUM_MODULATORS];   /* normalized to a specific gain */
    uint8_t  status[ALS_STATUS_END];                  /* status registers from the device */
    uint8_t  gains[NUM_MODULATORS];                   /* register value of the gains */
    fp_t   mod_gains[NUM_MODULATORS];               /* numeric equiv of gains - 128x, 256x, etc */
    fp_t   mod_normalized_gains[NUM_MODULATORS];    /* normalized to the clear channel in step 0 mod 0*/
    fp_t   matching_factors;                        /* compare clear channel from each step: 0 -> 0, 1 -> 0 , 2 -> 0 */
    uint16_t als_fifo_data[ALS_FIFO_DATA_LENGTH];     /* store als fifo data */
    fp_t   lux;
    fp_t   cct;
} ams_als_info_t;

typedef struct _ams_fd_info_t
{
    uint16_t          fd_nr_samples;
    fp_t            sample_freq; /* nyquist sampling frequency */
    uint8_t           gain_reg;    /* actual register value - only 1 modulator for flicker */
    uint16_t          gain_mod;    /* actual gain.. 2x, 4x, 8x, 16x... */
    bool              end_marker;  /* 3 bytes 0f 0x00 */
    uint16_t          len;         /* length of the flicker data not including end or gain */
    fp_t            freq;        /* calculated frequency */
} ams_fd_info_t;

typedef enum
{
    AMS_FIFO_IDLE,
    AMS_FIFO_IN_PROGRESS,
} ams_fifo_state_t;

typedef struct _ams_fifo_info_t
{
    uint16_t total_len;
    uint16_t level;
    uint16_t threshold;
    uint32_t overflow;
    uint32_t underflow;
    ams_fifo_state_t fifo_state;
} ams_fifo_info_t;

typedef struct _ams_sai_info_t
{
    ams_sai_state_t      sai;  /* enabled or disables */
    bool                 active; /* active or not active */
} ams_sai_info_t;

typedef struct _ams_sensor_config_t
{
    /* Base Device */
    uint16_t sample_time;
    uint16_t mod_trigger;
    uint16_t wait_time;
    uint16_t agc_mode;
    uint16_t agc_nr_samples;

    /* ALS */
    uint16_t als_nr_samples;

    /* FLICKER */
    uint16_t fd_nr_samples;

    /* FIFO */
    uint16_t fifo_threshold;
    uint16_t fifo_reset;
} ams_sensor_config_t;

typedef struct _device_status
{
    bool                 pon;
    bool                 log_irq;
    ams_feature_enable_t fd_en;
    ams_feature_enable_t als_en;
    ams_sai_info_t       sai;
    ams_als_info_t       als;
    ams_fd_info_t        fd;
    ams_fifo_info_t      fifo;
} ams_device_status_t;

typedef void (*device_write)(uint8_t reg, uint8_t *sh, uint8_t val);
typedef void (*device_read)(uint8_t reg, uint8_t *buffer, uint8_t bytes);
typedef ams_errno_t (*device_enable)(ams_feature_t feature, ams_feature_enable_t enable);
typedef ams_errno_t (*device_sai)(ams_sai_state_t state);
typedef ams_errno_t (*device_pon)(bool state);
typedef ams_errno_t (*device_log_irq)(bool state);
typedef ams_errno_t (*device_configure)(ams_config_feature_t cfg_type, void *cfg);
typedef void (*device_log)(int level, const char *message);
typedef ams_errno_t (*device_status)(void *data);
typedef ams_errno_t (*device_setup)(void *data);

struct ams_device
{
    device_write       write;
    device_read        read;
    device_enable      enable;
    device_sai         sai;
    device_pon         pon;
    device_log_irq     log_irq;
    device_configure   configure;
    device_log         log;
    device_status      status;
    device_setup       setup;
    char const        *version;
};

ams_errno_t ams_device_init(void);
void ams_device_write(uint8_t reg, uint8_t val);
void ams_device_read( uint8_t reg, uint8_t *buffer, uint8_t bytes);
ams_errno_t ams_device_enable(ams_feature_t feature, ams_feature_enable_t enable);
ams_errno_t ams_device_sai(ams_sai_state_t state);
ams_errno_t ams_device_log_irq(bool state);
ams_errno_t ams_device_pon(bool state);
ams_errno_t ams_device_configure(ams_config_feature_t cfg_type, void *cfg);
ams_errno_t ams_device_isUP(bool *pOK);
ams_errno_t ams_device_status(void *data);
ams_errno_t ams_device_setup(void *data);
void ams_device_get_version(void);
ssize_t ams_registers_get(char *buf, int bufsiz);

typedef enum
{
    LOG_INFO,
    LOG_WARNING,
    LOG_DEBUG,
    LOG_ERROR,
    LOG_APPLICATION
} ams_error_levels_t;

#define MAX_REGS 256

/* Used to condition the log statements */
#define LOG_FILE_NAME_LEN            (40)
#define LOG_FUNC_NAME_LEN            (20)
#define LOG_LINE_NUM_LEN             (3)
#define MIN_LOG_LEN                  (28)
#define MAX_LOG_LEN              MIN_LOG_LEN
#define LINE_NUM_LOG_LEN             (3)
#define FORMAT_LOG_MSG_SIZE         (400)
#define LOG_MSG_SIZE                (256)
#define AMS_LOG_PRINTF(level, format, args...) cprintf(CC_ACCEL, format, ##args)

extern struct ams_device device;


/* Magic to print floats via the Nordic log facility.  These overrides the Nordic defaults */
/* defaults */
#define AMS_NRF_LOG_FLOAT8_MARKER                                "%2d.%08d"
#define AMS_NRF_LOG_FLOAT8(val)                                                  \
                       (int32_t)(val),                                           \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)             \
                                            : (int32_t)(val) - (val))*100000000)

#define AMS_NRF_LOG_FLOAT4_MARKER                                "%1d.%04d"
#define AMS_NRF_LOG_FLOAT4(val)                                                  \
                       (int32_t)(val),                                           \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)             \
                                            : (int32_t)(val) - (val))*10000)

#define AMS_NRF_LOG_FLOAT1_MARKER                                "%2d.%01d"
#define AMS_NRF_LOG_FLOAT1(val)                                                  \
                       (int32_t)(val),                                           \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)             \
                                            : (int32_t)(val) - (val))*10)

#endif /* __AMS_DEVICE_H__ */
