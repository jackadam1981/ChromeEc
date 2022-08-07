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
#include "ams_errno.h"
#include "tcs3410_hwdef.h"
#include "tcs3410_irq.h"
#include "tcs3410.h"
#include "tcs3410_fifo.h"
#include "tcs3410_als.h"
#include "tcs3410_fd.h"

#include "ams_device.h"

static void mint_irq(uint8_t *shadow_regs)
{
    uint8_t status2;

    AMS_LOG_PRINTF_IRQ(LOG_INFO, "Processing a MINT IRQ");

    sensor_read(REG_STATUS2, &status2, 1);
    shadow_regs[REG_STATUS2] = status2;


    if (status2 & STATUS2_ALS_DATA_VALID)
    {
        AMS_LOG_PRINTF_IRQ(LOG_INFO, "ALS Data Valid\n");
    }

    if (status2 & STATUS2_ALS_DIG_SAT)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "ALS Digital Saturation Occurred");
    }

    if (status2 & STATUS2_FD_DIG_SAT)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Flicker Digital Saturation Occurred");
    }

    if (status2 & STATUS2_MOD0_ANA_SAT)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Modulator_0 Analog Saturation Occurred");
    }

    if (status2 & STATUS2_MOD1_ANA_SAT)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Modulator_1 Analog Saturation Occurred");
    }

    if (status2 & STATUS2_MOD2_ANA_SAT)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Modulator_2 Analog Saturation Occurred");
    }

    /* datasheet indicates read only - should not hurt to clear status2 */
    sensor_write(REG_STATUS2, sh, status2);

    return;
}

/*
 * The fifo level met/exceeded the set threshold.   The design was meant to fit
 * an entire flicker sample set within the fifo for an atomic read (assuming
 * difference and compression settings).
 */
static void fint_irq(uint8_t *shadow_regs)
{
    volatile ams_current_state_t *pcurr_state = sensor_get_current_state();

    AMS_LOG_PRINTF_IRQ(LOG_INFO, "Processing a FINT IRQ");


    /* Update this routine if a sequence round exceeds 511 bytes - max size of fifo */
    /* fifo state should be IDLE */
    sensor_read_fifo(shadow_regs, pcurr_state);
    pcurr_state->fifo.fifo_state = AMS_FIFO_IN_PROGRESS;

    return;
}

/*
 * Need to consult status4 to see if measured data is corrupt: STATUS4:b3
 */
static void sint_irq(uint8_t *shadow_regs)
{
    uint8_t status5;
    volatile ams_current_state_t *pcurr_state = sensor_get_current_state();

    AMS_LOG_PRINTF_IRQ(LOG_INFO, "Processing a SINT IRQ");

    sensor_read(REG_STATUS5, &status5, 1);
    shadow_regs[REG_STATUS5] = status5;


    if (status5 & STATUS5_SINT_VSYNC)
    {
        AMS_LOG_PRINTF_IRQ(LOG_INFO, "VSync Lost/Changed Interrupt Occurred");
    }

    /* End of a Sequence Round - Read the Data out of the device FIFO */
    if (status5 & STATUS5_SINT_MEAS_SEQR)
    {
        AMS_LOG_PRINTF_IRQ(LOG_INFO, "Measurement Sequencer Event Occurred");
        if (shadow_regs[REG_STATUS4] & STATUS4_MOD_SAMPLE_TRIG_ERR)
        {
            /* Indicates data is corrupt - log it and throw it away */
            AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Measurement Data is Corrupt - Discarding");
        }
        else
        {
            sensor_read_fifo(shadow_regs, pcurr_state);
            sensor_process_fifo(shadow_regs, pcurr_state);
        }
    }

    /* Clear the SINT interrupts - Only 2 bits are writeable */
    sensor_write(REG_STATUS5, sh, status5);
    return;
}

/*
 * Since tcs3410 driver is not using ALS Threshold interrupts, this interrupt should
 * not be generated.  ALS threshold interrupts (and some others), can only be generated
 * if the AIEN is set.  The HI or LOW Thresholds  plus persistance must be met.
 */
static void aint_irq(uint8_t *shadow_regs)
{
    uint8_t status3;

    AMS_LOG_PRINTF_IRQ(LOG_INFO, "Processing a AINT IRQ");

    sensor_read(REG_STATUS3, &status3, 1);
    shadow_regs[REG_STATUS3] = status3;

    if (status3 & STATUS3_HYST_STATE_VALID)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "ALS interrupt hysteresis Valid");
    }

    if (status3 & STATUS3_HYST_STATE_RD)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "ALS hysteresis possible");
    }

    if (status3 & STATUS3_AIHT)
    {
         AMS_LOG_PRINTF_IRQ(LOG_ERROR, "ALS data exceeded HIGH Threshold");
    }

    if (status3 & STATUS3_AILT)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "ALS data exceeded LOW Threshold");
    }

    if (status3 & STATUS3_VSYNC_LOST)
    {
          AMS_LOG_PRINTF_IRQ(LOG_ERROR, "VSYNC Lost");
    }

    if (status3 & STATUS3_OSC_CALIB_SAT)
    {
          AMS_LOG_PRINTF_IRQ(LOG_ERROR, "OSC Calibration is Out Of Range");
    }

    if (status3 & STATUS3_OSC_CALIB_FINISHED)
    {
         AMS_LOG_PRINTF_IRQ(LOG_ERROR, "OSC Calibration Complete");
    }

    /* Clear the AINT interrupts - Only AIHT and AILT can be cleared */
    sensor_write(REG_STATUS3, sh, status3);

    return;
}

/*
 * Some status register bits do not directly generate an interrupt.  Check to see
 * if the event represented by the bit occurred.
 */
static void misc_irq(uint8_t status4)
{

    if (status4 & STATUS4_MOD_SAMPLE_TRIG_ERR)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Measured Data is Corrupt");
    }

    if (status4 & STATUS4_MOD_TRIG_ERR)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "WTIME too short");
    }

    /* SAI Active handled outside this routine */

    if (status4 & STATUS4_INIT_BUSY)
    {
        AMS_LOG_PRINTF_IRQ(LOG_ERROR, "Initialization Busy");
    }

    /******************************************************/
    /*                        SAI                         */
    /* If SAI_ACTIVE(b1) is set in STATUS4, OSC is turned */
    /* off now                                            */
    /*                                                    */
    /******************************************************/
    /* This functionality has been moved to the CLI.  The user can reset  */
    /* the SAI Active bit when ready from the CLI - Therefore, the system */
    /* is setup to stop after one sequence round.                         */
    if (status4 & STATUS4_SAI_ACTIVE)
    {
         AMS_LOG_PRINTF_IRQ(LOG_INFO, "SAI is Active");
    }

    /* Clear the bits than can be cleared - SAI is not cleared here.  See datasheet */
    sensor_write(REG_STATUS4, sh, status4);

    return;
}

/******************************************************************************/
/*                            Public APIs                                     */
/******************************************************************************/

void sensor_process_irq(uint8_t *shadow_regs)
{
    uint8_t status, status4;

    /* Main status interrupt register */
    sensor_read(REG_STATUS, &status, 1);
    shadow_regs[REG_STATUS] = status;

    /* items in status4 do not directly initiate an irq */
    /* but the info in status4 can be used by other ints - ie SINT */
    sensor_read(REG_STATUS4, &status4, 1);
    shadow_regs[REG_STATUS4]= status4;

    if (status == 0)
    {
        /* not tcs3410 interrupt */
        return;
    }

    /* Read all status registers */

    /******************************************************/
    /*                        MINT                        */
    /* If MINT (b7)is set, check STATUS2 register for     */
    /* details.                                           */
    /******************************************************/
    if ((shadow_regs[REG_INTENAB] & INTENAB_MIEN_MASK) &&
        (status & STATUS_MINT))
    {
        mint_irq(shadow_regs);
    }

    /******************************************************/
    /*                        AINT                        */
    /* if AINT (b3)is set, an ALS event that met the      */
    /* programmed ALS thresholds (AILT aor AIHT)          */
    /* and persistance occurred. Check Status3 for        */
    /* details.                                           */
    /******************************************************/
    if ((shadow_regs[REG_INTENAB] & INTENAB_AIEN_MASK) &&
        (status & STATUS_AINT))
    {
        aint_irq(shadow_regs);
    }

    /******************************************************/
    /*                        FIFO                        */
    /* if FINT (b2) is set, the data level in the FIFO    */
    /* met the programmed FIFO thresholds. This IRQ       */
    /* is automatically asserted depending on the         */
    /* programmed thresholds.                             */
    /******************************************************/
    if ((shadow_regs[REG_INTENAB] & INTENAB_FIEN_MASK) &&
        (status & STATUS_FINT))
    {
        fint_irq(shadow_regs);
    }

    /******************************************************/
    /*                        SINT                        */
    /* if SINT (b0)is set, a system interrupt has         */
    /* occurred.  See Status5 for details of the possible */
    /* events related to this interrupt.                  */
    /******************************************************/
    if ((shadow_regs[REG_INTENAB] & INTENAB_SIEN_MASK) &&
        (status & STATUS_SINT))
    {
        sint_irq(shadow_regs);
    }

    /* Some status bits need to be checked that do not generate a physical IRQ */
    /* carry-over from status4 register and SAI */
    misc_irq(status4);

    /* Clear the Interrupts in the STATUS register */
    sensor_write(REG_STATUS, sh, status);

    return;  /* handled the interrupt */
}


