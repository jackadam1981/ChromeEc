#include "console.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#include "cec.h"

#define CEC_MAX_MESSAGE_SIZE 16
#define MFT_CLK 32768

#define DATA_BIT_LENGTH 60000       // 2.4ms
#define DATA_BIT_MIN_LOW_TIME 60000   // 0.4ms
#define DATA_BIT_MIN_TIME 60000     // 2.05ms
#define DATA_BIT_MAX_TIME 60000     // 2.75ms

// Start bit: 3.7ms + 0.8ms = 4.5ms.
#define START_BIT_LOW_TIME 60000  // 3.7ms
#define START_BIT_HIGH_TIME 60000  // 0.8ms

#define START_BIT_MIN_LOW_TIME 60000   // 3.5ms
#define START_BIT_MAX_LOW_TIME 60000   // 3.9ms
#define START_BIT_MIN_TIME 60000       // 4.3ms
#define START_BIT_MAX_TIME 60000       // 4.7ms
#define LOGICAL_ONE_LOW_TIME 30000    // 0.6ms
#define LOGICAL_ONE_MAX_LOW_TIME 40000  // 0.8ms

#define LOGICAL_ONE_HIGH_TIME 60000  // 1.8ms ==> 1.8 + 0.6 = 2.4ms
#define LOGICAL_ZERO_LOW_TIME 60000  // 1.5ms
#define LOGICAL_ZERO_HIGH_TIME 30000  // 0.9ms ==> 1.5 + 0.9 = 2.4ms
#define LOGICAL_ZERO_MIN_LOW_TIME (1300 * MFT_CLK / 1000000)  // 1.3ms
#define LOGICAL_ZERO_MAX_LOW_TIME (1700 * MFT_CLK / 1000000)  // 1.7ms

static bool CEC_IS_TX_BUSY = false;

static uint8_t cec_bus_free_time;
static uint8_t time_before_tx;
static int tx_byte_cnt;
static int tx_bit_cnt;
static bool acknowledge_received;
static uint8_t cec_broadcast;

static struct cec_msg test_msg;

// int test_ec(int argc, char** argv) {
//   ccprintf("test ec.\n");
//   SET_BIT(NPCX_PDOUT(3), 6);
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(test_ec, test_ec, "test", "test ec");
//

int set_output_high(int argc, char** argv) {
  SET_BIT(NPCX_PDOUT(3), 6);
  return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(out_high, set_output_high, NULL, NULL);

int set_output_low(int argc, char** argv) {
  CLEAR_BIT(NPCX_PDOUT(3), 6);
  return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(out_low, set_output_low, NULL, NULL);

//
// int set_input_high(int argc, char** argv) {
//   SET_BIT(NPCX_PDOUT(4), 0);
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(in_high, set_input_high, NULL, NULL);
//
// int set_input_low(int argc, char** argv) {
//   CLEAR_BIT(NPCX_PDOUT(4), 0);
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(in_low, set_input_low, NULL, NULL);
//
// int print_io_pin(int argc, char** argv) {
//   if (IS_BIT_SET(NPCX_PDOUT(3), 6)) {
//     ccprintf("output 1\n");
//   } else {
//     ccprintf("output 0\n");
//   }
//   if (IS_BIT_SET(NPCX_PDOUT(4), 0)) {
//     ccprintf("input 1\n");
//   } else {
//     ccprintf("input 0\n");
//   }
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(print_io, print_io_pin, NULL, NULL);
//
// int start_tx_timer(int argc, char** argv) {
//   int time = strtoi(argv[1], NULL, 0);
//   NPCX_TCNT2(0) = time;  // Init count down timer.
//   NPCX_TCKC(0) &= 0xC7;  // Clear current clock.
//   NPCX_TCKC(0) |= 0x20;  // Select LFCLK for counter 1.
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(start_tx_time, start_tx_timer, NULL, NULL);
//
// int stop_tx_timer(int argc, char** argv) {
//   NPCX_TCKC(0) &= 0xC7;
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(stop_tx_time, stop_tx_timer, NULL, NULL);

// int init_tx_timer(int argc, char** argv) {
//   int time = strtoi(argv[1], NULL, 0);
//   NPCX_TCNT2(1) = time;
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(init_tx_time, init_tx_timer, NULL, NULL);

// int print_tx_timer(int argc, char** argv) {
//   ccprintf("TCNT2: %d\n", NPCX_TCNT2(1));
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(print_tx_time, print_tx_timer, NULL, NULL);

// static int start_rx_capture(int argc, char** argv) {
//   NPCX_TCNT1(0) = 0xFFFF;  // Init receive timer.
//   NPCX_TCKC(0) &= 0xF8;    // Clear current clock.
//   NPCX_TCKC(0) |= 0x04;    // Select LFCLK for counter 1.
//   return 0;
// }
// DECLARE_SAFE_CONSOLE_COMMAND(start_rx, start_rx_capture, NULL, NULL);
//
static int check_event_source(int idx) {
  if (IS_BIT_SET(NPCX_TECTRL(0), idx)) {
    return 1;
  } else {
    return 0;
  }
}

// TODO(frankhu): add handler to deal with different return values.
void operation_handler(void) { return; }

void set_tx_high(void) { SET_BIT(NPCX_PDOUT(3), 6); }

void set_tx_low(void) { CLEAR_BIT(NPCX_PDOUT(3), 6); }

void capture_low_to_high(void) { SET_BIT(NPCX_TMCTRL(0), 3); }

void capture_high_to_low(void) { CLEAR_BIT(NPCX_TMCTRL(0), 3); }

void start_rx_capture(void) {
  // Init reveive timer.
  NPCX_TCNT1(0) = 0xFFFF;
  // Clear current cunter 1 clock.
  NPCX_TCKC(0) &= 0xF8;
  // Select LFCLK for counter 1.
  NPCX_TCKC(0) |= 0x04;
}

void enable_receive(struct cec_msg *CEC_msg) {
  CEC_msg->rx_status = 0;
  capture_high_to_low();
  start_rx_capture();
}

void start_tx_timer(int time) {
  NPCX_TCNT2(0) = time;  // Init count down timer.
  NPCX_TCKC(0) &= 0xC7;  // Clear current clock.
  NPCX_TCKC(0) |= 0x20;  // Select LFCLK for counter 2.
}

void stop_tx_timer(void) {
  // Clear clock for counter 1.
  NPCX_TCKC(0) &= 0xC7;
}

int get_initiator_addr(struct cec_msg *CEC_msg) {
  int cec_initiator = (CEC_msg->msg[0]) & 0x0F;
  return cec_initiator;
}

int get_dest_addr(struct cec_msg *CEC_msg) {
  int cec_dest = ((CEC_msg->msg[0]) >> 4) & 0x0F;
  return cec_dest;
}

void check_acknowledge(struct cec_msg *CEC_msg) {
  // acknowledge_received = false;
  acknowledge_received = true;
  // capture_low_to_high();
  // CEC_msg->rx_status = RECEIVE_ACK_BIT;
  // start_rx_capture();
}

void stop_rx_timer(void) { NPCX_TCKC(0) &= 0xF8; }

void send_start_bit(struct cec_msg *CEC_msg) {
  switch (CEC_msg->tx_status) {
    case (SEND_START_BIT_LOW): {
      set_tx_low();
      start_tx_timer(START_BIT_LOW_TIME);
      CEC_msg->tx_status = SEND_START_BIT_HIGH;
      break;
    }
    case (SEND_START_BIT_HIGH): {
      set_tx_high();
      start_tx_timer(SEND_START_BIT_HIGH);
      CEC_msg->tx_status = SEND_DATA_BYTE;
      break;
    }
    default:
      return;
  }
}

void send_data_bit_low(struct cec_msg *CEC_msg) {
  int data_bit = ((CEC_msg->msg[tx_byte_cnt]) >> (7 - tx_bit_cnt)) & 1;
  set_tx_low();
  if (data_bit) {
    start_tx_timer(LOGICAL_ONE_LOW_TIME);
  } else {
    start_tx_timer(LOGICAL_ZERO_LOW_TIME);
  }
  CEC_msg->tx_status = SEND_DATA_BIT_HIGH;
}

void send_data_bit_high(struct cec_msg *CEC_msg) {
  int data_bit = ((CEC_msg->msg[tx_byte_cnt]) >> (7 - tx_bit_cnt++)) & 1;
  set_tx_high();
  if (data_bit) {
    start_tx_timer(LOGICAL_ONE_HIGH_TIME);
  } else {
    start_tx_timer(LOGICAL_ZERO_HIGH_TIME);
  }
  if (tx_bit_cnt >= 8) {
    tx_bit_cnt = 0;
    tx_byte_cnt++;
    CEC_msg->tx_status = SEND_EOM_LOW;
  } else {
    CEC_msg->tx_status = SEND_DATA_BIT_LOW;
  }
}

void send_eom_low(struct cec_msg *CEC_msg) {
  set_tx_low();
  if (tx_byte_cnt == CEC_msg->msg_len) {
    start_tx_timer(LOGICAL_ONE_LOW_TIME);
  } else {
    start_tx_timer(LOGICAL_ZERO_LOW_TIME);
  }
  CEC_msg->tx_status = SEND_EOM_HIGH;
}

void send_eom_high(struct cec_msg *CEC_msg) {
  set_tx_high();
  if (tx_byte_cnt == CEC_msg->msg_len) {
    start_tx_timer(LOGICAL_ONE_HIGH_TIME);
  } else {
    start_tx_timer(LOGICAL_ZERO_HIGH_TIME);
  }
  CEC_msg->tx_status = SEND_ACK_LOW;
}

void send_ack_low(struct cec_msg *CEC_msg) {
  // set_tx_low();
  check_acknowledge(CEC_msg);
  start_tx_timer(LOGICAL_ONE_LOW_TIME);
  CEC_msg->tx_status = SEND_ACK_HIGH;
}

void send_ack_high(struct cec_msg *CEC_msg) {
  set_tx_high();
  start_tx_timer(LOGICAL_ONE_HIGH_TIME);
  CEC_msg->tx_status = WAIT_FOR_ACK;
}

bool start_transmit(struct cec_msg *CEC_msg) {
  if (CEC_IS_TX_BUSY || (CEC_msg->msg_len > CEC_MAX_MSG_SIZE)) {
    // TODO(frankhu): need logs here.
    return false;
  }

  test_msg = *CEC_msg;
  NPCX_TCNT1(0) = 0x0000;

  if (CEC_msg->retransmit > CEC_MAX_RETRANS)
    CEC_msg->retransmit = CEC_MAX_RETRANS;
  if (CEC_msg->retransmit < CEC_MIN_RETRANS)
    CEC_msg->retransmit = CEC_MIN_RETRANS;

  if (get_dest_addr(CEC_msg) == CEC_ADDR_BROADCAST) {
    cec_broadcast = 1;
  }

  tx_bit_cnt = 0;
  tx_byte_cnt = 0;
  CEC_msg->tx_status = WAIT_BUS_FREE_TIME;
  start_tx_timer(DATA_BIT_LENGTH);
  return true;
}

int CEC_init(int argc, char **argv) {
  // Initialize MFT.
  /* Set MFT to mode 2 Dual Capture;
   * Enable TA1 to function as preset input;
   * Init to capture High-to-low event. */
  // NPCX_TMCTRL(1) = 0x21;
  // NPCX_TMCTRL(1) = 0x61;
  task_enable_irq(NPCX_IRQ_MFT_1);
  SET_FIELD(NPCX_PWDWN_CTL(0), FIELD(5, 1), 0);
  NPCX_TMCTRL(0) = 0x21;
  // Disable NPCX_TCNT1(0) first.
  NPCX_TCKC(0) = 0;
  /* Enable TAIEN for Rx;
   * Enable TDIEN for Tx. */
  NPCX_TIEN(0) = 0x09;
  ccprintf("TIEN: %d\n", NPCX_TIEN(0));

  CLEAR_BIT(NPCX_PDIR(4), 0);   // Input.
  SET_BIT(NPCX_PDIR(3), 6);     // Output.
  CLEAR_BIT(NPCX_PDOUT(3), 6);  // Init output to 0.
  SET_BIT(NPCX_DEVALT(3), 4);   // Select TA1 on pin 40.
  CLEAR_BIT(NPCX_DEVALT(12), 4);
  SET_BIT(NPCX_PPULL(4), 0);   // Enable Pull-Up.
  CLEAR_BIT(NPCX_PPUD(4), 0);  // Select Pull-Up.

  time_before_tx = 5;

  // declare_interrupt_handler(CEC_hanlder);
  return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(CEC_init, CEC_init, "None", "init CEC");

int test_transmit(int argc, char **argv) {
  struct cec_msg msg;
  msg.msg_len = 1;
  msg.msg[0] = 0x01;
  msg.retransmit = CEC_MIN_RETRANS;
  start_transmit(&msg);
  return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(test_tx, test_transmit, NULL, NULL);

void transmit_handler(struct cec_msg *CEC_msg) {
  switch (CEC_msg->tx_status) {
    // Wait for signal free time.
    case (WAIT_BUS_FREE_TIME): {
      ++cec_bus_free_time;
      if (cec_bus_free_time >= time_before_tx) {
        // Disable reveiving.
        stop_rx_timer();
        CEC_msg->tx_status = SEND_START_BIT_LOW;
        send_start_bit(CEC_msg);
      } else {
        start_tx_timer(DATA_BIT_LENGTH);
      }
      break;
    }
    case (SEND_START_BIT_LOW): {
      send_start_bit(CEC_msg);
      NPCX_TCNT1(0) += 1;
      break;
    }
    case (SEND_START_BIT_HIGH): {
      send_start_bit(CEC_msg);
      NPCX_TCNT1(0) += 1;
      break;
    }
    // Send data bytes.
    case (SEND_DATA_BIT_LOW): {
      send_data_bit_low(CEC_msg);
      NPCX_TCNT1(0) += 1;
      break;
    }
    case (SEND_DATA_BIT_HIGH): {
      send_data_bit_high(CEC_msg);
      break;
    }
    case (SEND_EOM_LOW): {
      NPCX_TCNT1(0) += 1;
      send_eom_low(CEC_msg);
      break;
    }
    case (SEND_EOM_HIGH): {
      send_eom_high(CEC_msg);
      break;
    }
    case (SEND_ACK_LOW): {
      NPCX_TCNT1(0) += 1;
      send_ack_low(CEC_msg);
      break;
    }
    case (SEND_ACK_HIGH): {
      NPCX_TCNT1(0) += 1;
      send_ack_high(CEC_msg);
      break;
    }
    case (WAIT_FOR_ACK): {
      if (acknowledge_received) {
        if (tx_byte_cnt >= CEC_msg->msg_len) {
          stop_tx_timer();
          break;
          operation_handler();
          enable_receive(CEC_msg);
        } else {
          // More bytes in CEC_msg.
          CEC_msg->tx_status = SEND_DATA_BIT_LOW;
          // Send next byte.
          send_data_bit_low(CEC_msg);
        }
      } else {
        // Data byte not acknowledged therefore need re-transmision.
        if (CEC_msg->retransmit > 0) {
          CEC_msg->retransmit--;
          cec_bus_free_time = 0;
          time_before_tx = 3;
          start_tx_timer(DATA_BIT_LENGTH);
          tx_byte_cnt = 0;
          tx_bit_cnt = 0;
          CEC_msg->tx_status = WAIT_BUS_FREE_TIME;
        }
      }
      break;
    }
    case (SEND_ACK): {
      set_tx_high();
      stop_tx_timer();
      break;
    }
  }
}

void CEC_hanlder(void) {
  if (check_event_source(3)) {
    SET_BIT(NPCX_TECLR(0), 3);
    // NPCX_TCKC(0) &= 0xC7;
    transmit_handler(&test_msg);
  }
  if (check_event_source(0)) {
    SET_BIT(NPCX_TECLR(0), 0);
    NPCX_TCKC(0) &= 0xF8;
  }
}

DECLARE_IRQ(NPCX_IRQ_MFT_1, CEC_hanlder, 1);
