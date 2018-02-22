#include "host_command.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#include "cec.h"

#define CPRINTS(format, args...) cprints(CC_CEC, format, ## args)
#define CPUTS(outstr) cputs(CC_CEC, outstr)

/* Multi-function timer settings */
#define TMR_INT_EN 0x09
#define TMCTRL_INIT 0x21
#define TCKC_INIT 0x80
#define MAX_BYTE 5

/* Counter settings */
#define TCKC_CNT1_OFF 0xF8 //ANDed
#define TCKC_CNT1_SLOW 0x04 //ORed
#define TCKC_CNT2_OFF 0xC7 //ANDed
#define TCKC_CNT2_SLOW 0x20 //ORed
#define TCKC_CNT2_APB 0x08 //ORed

/* CEC messages are highly time sensitive, therefore we use macros to pull
 * the bus in a given direction. */
#define BUS_OUT(LEVEL) LEVEL ? \
		       CLEAR_BIT(NPCX_PDIR(3), 6) : \
		       SET_BIT(NPCX_PDIR(3), 6)
#define BUS_LOW IS_BIT_SET(NPCX_PDIR(3), 6)

/* Bit timing */
#define APB_FREQ 3750000 
#define TO_CLOCK_TICK(MILLIS) (MILLIS * APB_FREQ / 10000)
#define CEC_FREE_TIME TO_CLOCK_TICK(24)

#define CEC_FREE_TIME_RS (3 * CEC_FREE_TIME) // Resend
#define CEC_FREE_TIME_NI (5 * CEC_FREE_TIME) // New initiator
#define CEC_FREE_TIME_PI (7 * CEC_FREE_TIME) // Present initiator

#define CEC_START_BIT_L TO_CLOCK_TICK(37)
#define CEC_START_BIT_H TO_CLOCK_TICK(8)

#define CEC_BIT_0_L TO_CLOCK_TICK(15)
#define CEC_BIT_0_H TO_CLOCK_TICK(9)

#define CEC_BIT_1_L TO_CLOCK_TICK(6)
#define CEC_BIT_1_H TO_CLOCK_TICK(18)

#define MAX_STAGES MAX_BYTE * 20 + 2
volatile uint16_t num_stages = -1;
volatile uint16_t current_stage = -1;
volatile uint16_t wait_times[MAX_STAGES];

void timer_handler(void)
{
	// Clear interrupt request.
	SET_BIT(NPCX_TECLR(0), 3);

	// If last bit was sent, turn off timer and return.
	if (num_stages == current_stage){
		NPCX_TCKC(0) &= TCKC_CNT2_OFF;
		current_stage = -1;
		num_stages = -1;
		return;
	}

	// Continue message flow.
	BUS_OUT(current_stage % 2);
	NPCX_TCNT2(0) = wait_times[current_stage++];
}

int cec_send(uint8_t data[], int8_t byte_len)
{
	uint8_t nbyte;
	int8_t nbit;

	num_stages = 20 * byte_len + 2;
	current_stage = 0;
	
	wait_times[current_stage++] = CEC_START_BIT_L;
	wait_times[current_stage++] = CEC_START_BIT_H;

	for (nbyte = 0; nbyte < byte_len; nbyte++) {
		for(nbit = 7; nbit >= 0; nbit--) {
			if (data[nbyte] & (1 << nbit)) {
				wait_times[current_stage++] = CEC_BIT_1_L;
				wait_times[current_stage++] = CEC_BIT_1_H;
			}
			else {
				wait_times[current_stage++] = CEC_BIT_0_L;
				wait_times[current_stage++] = CEC_BIT_0_H;
			}
		}
		if (nbyte == byte_len - 1) {
			wait_times[current_stage++] = CEC_BIT_1_L;
			wait_times[current_stage++] = CEC_BIT_1_H;
			
		}
		else {
			wait_times[current_stage++] = CEC_BIT_0_L;
			wait_times[current_stage++] = CEC_BIT_0_H;
		}
		
		wait_times[current_stage++] = CEC_BIT_1_L;
		wait_times[current_stage++] = CEC_BIT_1_H;
	}
	
	current_stage = 0;
	NPCX_TCNT2(0) = CEC_FREE_TIME_NI;
	NPCX_TCKC(0) |= TCKC_CNT2_APB;
	
	return 0;
}

int tv_on(int argc, char **argv)
{
	uint8_t data[2];
	data[0] = 0x40;
	data[1] = 0x04;
	return cec_send(data, 2);
}

DECLARE_SAFE_CONSOLE_COMMAND(on_tv, tv_on, NULL, NULL);

int tv_off(int argc, char **argv)
{
	uint8_t data[2];
	data[0] = 0x40;
	data[1] = 0x36;
	return cec_send(data, 2);
}

DECLARE_SAFE_CONSOLE_COMMAND(off_tv, tv_off, NULL, NULL);

int hc_display_power(struct host_cmd_handler_args *args)
{
	const struct ec_params_display_power *p = args->params;
	
	if(p->turn_on_display)
		tv_on(0, NULL);
	else 
		tv_off(0, NULL);
	
	return EC_RES_SUCCESS;
	
}

DECLARE_HOST_COMMAND(EC_CMD_DISPLAY_POWER, hc_display_power, EC_VER_MASK(0));

int cec_init(void)
{
	// Ensure Multi-Function timer is powered up.
 	SET_FIELD(NPCX_PWDWN_CTL(0), FIELD(5, 1), 0);
 
 	// Enable multifunction timer interrupt
 	task_enable_irq(NPCX_IRQ_MFT_1);
 
 	// Set timer controls
 	NPCX_TMCTRL(0) = TMCTRL_INIT;
 	NPCX_TCKC(0) = TCKC_INIT;
 	NPCX_TIEN(0) = TMR_INT_EN;
 
 	// Reset clock counters.
 	NPCX_TCNT1(0) = 0;
 	NPCX_TCNT2(0) = 0;
 
 	// Open-drain output buffer is cleared. Polarity controlled by flipping
 	// direction.
 	CLEAR_BIT(NPCX_PDOUT(3), 6);
 	BUS_OUT(1);
 	CPRINTS("CEC enabled.");
 	return 0;
}

DECLARE_IRQ(NPCX_IRQ_MFT_1, timer_handler, 1);
