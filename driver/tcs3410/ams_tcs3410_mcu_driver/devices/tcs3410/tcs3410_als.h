/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __TCS3410_ALS_H__
#define __TCS3410_ALS_H__

#define ALS_STATUS_MOD0_GAIN_SHIFT (0)
#define ALS_STATUS_MOD0_GAIN_MASK  (0x0F)

#define ALS_STATUS_MOD1_GAIN_SHIFT (4)
#define ALS_STATUS_MOD1_GAIN_MASK  (0xF0)

#define ALS_STATUS_MOD2_GAIN_SHIFT (0)
#define ALS_STATUS_MOD2_GAIN_MASK (0x0F)

/* sequence step */
#define ALS_STATUS_MEAS_SEQ_STEP_SHIFT   (6)
#define ALS_STATUS_MEAS_SEQ_STEP_MASK    (3 << ALS_STATUS_MEAS_SEQ_STEP_SHIFT)

/* analog status on modulators */
#define ALS_STATUS_ANA_SAT_MOD0_SHIFT   (5)
#define ALS_STATUS_ANA_SAT_MOD0_MASK    (1 << ALS_STATUS_ANA_SAT_MOD0_SHIFT)

#define ALS_STATUS_ANA_SAT_MOD1_SHIFT   (4)
#define ALS_STATUS_ANA_SAT_MOD1_MASK    (1 << ALS_STATUS_ANA_SAT_MOD1_SHIFT)

#define ALS_STATUS_ANA_SAT_MOD2_SHIFT   (3)
#define ALS_STATUS_ANA_SAT_MOD2_MASK    (1 << ALS_STATUS_ANA_SAT_MOD2_SHIFT)


ams_errno_t process_als_data(volatile ams_current_state_t *pcurr_state, uint8_t *pfifo);


#endif /* __TCS3410_ALS_H__ */

