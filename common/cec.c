#include "cec.h"

#include "assert.h"
#include "mock_cec.h"

// #define CEC_IS_TX_BUSY false
#define CEC_IS_RCV_BROADCAST true
#define CEC_DST_ADDR TURE

// #define set_tx_low()      \
//   SET_BIT(NPCX_PDIR(3), 6); \
//   CLEAR_BIT(NPCX_DEVALT(11), 2);
// #define set_tx_high()       \
//   CLEAR_BIT(NPCX_PDIR(3), 6); \
//   SET_BIT(NPCX_DEVALT(11), 2);
// #define cec_capture_low_to_high() SET_BIT(NPCX_TMCTRL(1), 3);
// #define cec_capture_high_to_low() CLEAR_BIT(NPCX_TMCTRL(1), 3);

#define CEC_MAX_MESSAGE_SIZE 16
#define MFT_CLK 32768
#define DATA_BIT_LENGTH (2400 * MFT_CLK / 1000000)       // 2.4ms
#define DATA_BIT_MIN_LOW_TIME (400 * MFT_CLK / 1000000)  // 0.4ms
#define DATA_BIT_MIN_TIME (2050 * MFT_CLK / 1000000)     // 2.05ms
#define DATA_BIT_MAX_TIME (2750 * MFT_CLK / 1000000)     // 2.75ms

// Start bit: 3.7ms + 0.8ms = 4.5ms.
#define START_BIT_LOW_TIME (3700 * MFT_CLK / 1000000)  // 3.7ms
#define START_BIT_HIGH_TIME (800 * MFT_CLK / 1000000)  // 0.8ms

#define START_BIT_MIN_LOW_TIME (3500 * MFT_CLK / 1000000)   // 3.5ms
#define START_BIT_MAX_LOW_TIME (3900 * MFT_CLK / 1000000)   // 3.9ms
#define START_BIT_MIN_TIME (4300 * MFT_CLK / 1000000)       // 4.3ms
#define START_BIT_MAX_TIME (4700 * MFT_CLK / 1000000)       // 4.7ms
#define LOGICAL_ONE_LOW_TIME (600 * MFT_CLK / 1000000)      // 0.6ms
#define LOGICAL_ONE_MAX_LOW_TIME (800 * MFT_CLK / 1000000)  // 0.8ms

#define LOGICAL_ONE_HIGH_TIME \
  (1800 * MFT_CLK / 1000000)  // 1.8ms ==> 1.8 + 0.6 = 2.4ms
#define LOGICAL_ZERO_LOW_TIME (1500 * MFT_CLK / 1000000)  // 1.5ms
#define LOGICAL_ZERO_HIGH_TIME \
  (900 * MFT_CLK / 1000000)  // 0.9ms ==> 1.5 + 0.9 = 2.4ms
#define LOGICAL_ZERO_MIN_LOW_TIME (1300 * MFT_CLK / 1000000)  // 1.3ms
#define LOGICAL_ZERO_MAX_LOW_TIME (1700 * MFT_CLK / 1000000)  // 1.7ms

// Global variables.

static bool CEC_IS_TX_BUSY = false;

struct cec_msg CEC_msg;

static CEC_callback_t operation_handler; /* Pointer to the Callback function */
// static int CEC_msg.tx_status; /* State of the transmit state machine */
// static int CEC_msg.rx_status; /* State of the recieve state machine */
static int
    cec_tx_message_size; /* Number of bytes to transmit including header byte.
                          */
static uint8_t cec_rx_enable; /* 1 = receive enabled, 0 = receive disabled */
static uint8_t cec_own_addr;  /* logical device address */
static uint8_t cec_bus_free_time;
static int time_before_tx;    /* number of time before next transmition
                               * measured in length of data bits. */
static uint16_t rx_low_time;  /* Next or current bit Low time */
static uint16_t tx_low_time;  /* Next or current bit Low time */
static uint16_t tx_high_time; /* Next or current bit High time */
static int tx_bit_count;      /* Current bit in byte */
static int tx_byte_count;     /* Current byte in message */
static int rx_bit_count;      /* Current bit in received byte */
static int rx_byte_count;     /* Current byte in recieved message */
static uint8_t tx_byte;       /* holds the data for the current byte*/
static uint8_t rx_byte,
    rx_bit; /* holds the data for the current received bit,byte*/
static uint8_t cec_retransmit; /* Number of remaining retransmissions */
static uint8_t acknowledge_received;
static uint8_t
    cec_broadcast; /* 1 = receiving broadcast msg, 0 = receive regular msg */
static uint8_t* cec_rx_msg_buffer;
static uint8_t*
    rcv_data_buf; /* Buffer where received unicast data should be placed */
static uint8_t*
    bc_data_buf; /* Buffer where received braoadcast data should be placed */
static uint8_t cec_msg_buffer[CEC_MAX_MESSAGE_SIZE + 1];

void CEC_callback(CEC_status_ind_t op_status, uint8_t info) {
  printf("CEC callback.\n");
}

void CEC_handler(void);

void CEC_init(CEC_callback_t operation_done) {
  assert(operation_done != NULL);
  operation_handler = operation_done;

  time_before_tx = 5;
  acknowledge_received = 0;
  cec_rx_enable = 0;
  cec_broadcast = 0;

  init_registers();

  // Enable interrupt here.
  declare_interrupt_handler(CEC_handler);
}

static void set_tx_low(void) { set_tx_pin_low(); }

static void set_tx_high(void) { set_tx_pin_high(); }

static void start_cec_rx_capture(void) { start_rx_capture(); }

static void start_cec_tx_timer(int time) { start_tx_timer(time); }

static void cec_capture_low_to_high(void) { capture_low_to_high(); }

static void cec_capture_high_to_low(void) { capture_high_to_low(); }

static void enable_receive(void) {
  cec_rx_enable = true;
  CEC_msg.rx_status = 0;
  cec_capture_high_to_low();
  start_cec_rx_capture();
}

static void check_acknowledge(void) {
  acknowledge_received = false;
  CEC_msg.tx_status = 9;
  start_cec_tx_timer(LOGICAL_ZERO_LOW_TIME);
}

static void set_low_high_time(int bit) {
  if (bit) {
    tx_low_time = LOGICAL_ONE_LOW_TIME;
    tx_high_time = LOGICAL_ONE_HIGH_TIME;
  } else {
    tx_low_time = LOGICAL_ZERO_LOW_TIME;
    tx_high_time = LOGICAL_ZERO_HIGH_TIME;
  }
}

static void find_next_bit_low_high_time(void) {
  printf("tx bit count: %d \n", tx_bit_count);
  if (tx_bit_count < 8) {
    set_low_high_time(tx_byte & 0x80);
    tx_bit_count++;
    tx_byte = tx_byte << 1;
  } else {
    tx_byte_count++;
    tx_byte = CEC_msg.msg[tx_byte_count];
    tx_bit_count = 0;
    find_next_bit_low_high_time();
  }
}

bool CEC_start_transmit(CEC_logical_addr_t initiator, CEC_logical_addr_t dest,
                        uint8_t len, uint8_t* command, uint8_t retransmit) {
  if (CEC_IS_TX_BUSY) {
    printf("cec tx line is busy.\n");
    return false;
  }
  if (len > CEC_MAX_MSG_SIZE) {
    printf("message length exceed limit.\n");
    return false;
  }

  if (retransmit < CEC_MIN_RETRANS) retransmit = CEC_MIN_RETRANS;
  if (retransmit > CEC_MAX_RETRANS) retransmit = CEC_MAX_RETRANS;

  CEC_msg.msg[0] = (initiator << 4) | (dest & 0x0F);

  int i;

  for (i = 0; i < len; ++i) {
    CEC_msg.msg[i + 1] = command[i];
  }
  tx_bit_count = 0;
  tx_byte_count = 0;
  tx_byte = CEC_msg.msg[0];
  if (dest == 0xF) {
    cec_broadcast = 1;
  }

  cec_tx_message_size = len + 1;

  CEC_msg.tx_status = 0;
  start_cec_tx_timer(DATA_BIT_LENGTH);

  cec_retransmit = 0;

  CEC_IS_TX_BUSY = true;

  return true;
}

static void CEC_receive_handler(void) {
  // Time interval between two events.
  uint16_t time_interval = get_capture_time();

  switch (CEC_msg.rx_status) {
    // Captured high-to-low event.
    case 0: {
      if (cec_rx_enable) {
        // Receive enabled, next state.
        CEC_msg.rx_status = 1;
        cec_capture_low_to_high();
      }
      break;
    }
    // Captured low-to-high event.
    case 1: {
      cec_capture_high_to_low();
      // Check start bit low time.
      if ((time_interval >= START_BIT_MIN_LOW_TIME) &&
          (time_interval <= START_BIT_MAX_LOW_TIME)) {
        rx_low_time = time_interval;
        // Valid low time, next state.
        CEC_msg.rx_status = 2;
      } else {
        // If invalid, reset state machine.
        CEC_msg.rx_status = 0;
      }
      break;
    }
    // Check start bit high time.
    case 2: {
      cec_capture_low_to_high();
      // Check total start cycle time.
      if ((rx_low_time + time_interval >= START_BIT_MIN_TIME) &&
          (rx_low_time + time_interval <= START_BIT_MAX_TIME)) {
        // Start bit confirmed, next state.
        CEC_msg.rx_status = 3;
        rx_bit_count = 0;
        rx_byte_count = 0;
        rx_byte = 0;
        cec_broadcast = 0;
        time_before_tx = 5;
      } else {
        // Not start bit, capture high-to-low, to stat 1.
        CEC_msg.rx_status = 1;
      }
      break;
    }
    // Check data bit low time.
    case 3: {
      if (time_interval < DATA_BIT_MIN_LOW_TIME) {
        // TODO: decide how to handle invalid bit.
        CEC_msg.rx_status = 0;
      } else if (time_interval <= LOGICAL_ONE_MAX_LOW_TIME) {
        // Assume logical 1 received.
        rx_bit = 1;
        // Next state.
        CEC_msg.rx_status = 4;
      } else if (time_interval < LOGICAL_ZERO_MIN_LOW_TIME) {
        // TODO: decide how to handle invalid bit.
        CEC_msg.rx_status = 0;
      } else if (time_interval <= LOGICAL_ZERO_MAX_LOW_TIME) {
        // Assume logical 0 received.
        rx_bit = 0;
        // Next state.
        CEC_msg.rx_status = 4;
      } else {
        // TODO: decide how to handle invalid bit.
        CEC_msg.rx_status = 0;
      }
      rx_low_time = time_interval;
      cec_capture_high_to_low();
      break;
    }
    // Check data bit high time.
    case 4: {
      if (rx_low_time + time_interval < DATA_BIT_MIN_TIME) {
        // TODO: decide how to handle invalid bit.
        CEC_msg.rx_status = 0;
        // Start looking for a new start bit.
        cec_capture_high_to_low();
      } else if (rx_low_time + time_interval <= DATA_BIT_MAX_TIME) {
        // EOM bit.
        if (rx_bit_count == 8) {
          if (!cec_broadcast) {
            send_acknowledge();
          }
          cec_rx_msg_buffer[rx_byte_count] = rx_byte;
          // If there are more data blocks follow.
          if (rx_bit == 0) {
            // Capture next data bit.
            CEC_msg.rx_status = 3;
            ++rx_byte_count;
          } else {  // It is the end of current message.
            operation_handler(CEC_RCV_UNI_IND, rx_byte_count);
            // Re-start looking for a new message.
            CEC_msg.rx_status = 0;
            cec_capture_high_to_low();
          }
        } else if (rx_bit_count == 9) {  // ACK bit.
          rx_bit_count = 0;
          rx_byte = 0;
          ++rx_byte_count;
          CEC_msg.rx_status = 3;
        } else {
          rx_byte = (rx_byte << 1) | rx_bit;
          ++rx_bit_count;
          // Receive next bit.
          CEC_msg.rx_status = 3;
          // Header byte.
          if ((rx_byte_count == 0) && (rx_bit_count == 8)) {
            uint8_t dst_addr = rx_byte & 0x0f;
            // Broadcase dst address.
            if (dst_addr == 0x0f) {
              cec_broadcast = 1;
              cec_rx_msg_buffer = bc_data_buf;
            } else if (dst_addr == cec_own_addr) {  // My dst address
              cec_rx_msg_buffer = rcv_data_buf;
            } else {  // Wrong dst addr.
              // Reset cec state.
              CEC_msg.rx_status = 0;
              cec_capture_high_to_low();
            }
          }
        }
      } else {  // Else, invalid bit.
        CEC_msg.rx_status = 0;
        cec_capture_high_to_low();
      }
      break;
    }
    // Check for acknowledge.
    case 8: {
      // Disable receive timer.
      stop_rx_timer();
      if (cec_broadcast && (time_interval <= LOGICAL_ONE_MAX_LOW_TIME)) {
        acknowledge_received = true;
      } else if ((time_interval > LOGICAL_ONE_MAX_LOW_TIME) &&
                 (time_interval < LOGICAL_ZERO_MAX_LOW_TIME)) {
        acknowledge_received = true;
      }
      break;
    }
  }
}

static void CEC_transmit_handler(void) {
  printf("tx status: %d\n", CEC_msg.tx_status);
  switch (CEC_msg.tx_status) {
    // Wait for signal free time and then generate start bit low time.
    case 0: {
      cec_bus_free_time++;
      printf("cec bus has been free for %d bit time\n", cec_bus_free_time);
      if (cec_bus_free_time >= time_before_tx) {
        // Stop rx timer.
        stop_rx_timer();
        set_tx_low();
        // Generate start bit low time.
        CEC_msg.tx_status = 1;
        start_cec_tx_timer(START_BIT_LOW_TIME);
        break;
      } else {
        start_cec_tx_timer(DATA_BIT_LENGTH);
      }
      break;
    }
    // Generate start bit high time.
    case 1: {
      set_tx_high();
      CEC_msg.tx_status = 2;
      find_next_bit_low_high_time();
      start_cec_tx_timer(START_BIT_HIGH_TIME);
      break;
    }
    // Generate data bit low time.
    case 2: {
      set_tx_low();
      CEC_msg.tx_status = 3;
      start_cec_tx_timer(tx_low_time);
      break;
    }
    // Generate data bit high time.
    case 3: {
      set_tx_high();
      printf("current bit count %d\n", tx_bit_count);
      // There are more bits in current byte.
      if (tx_bit_count < 8) {
        set_low_high_time(tx_byte & 0x80);
        ++tx_bit_count;
        tx_byte = tx_byte << 1;
        // Next Tx state.
        CEC_msg.tx_status = 2;
      } else {
        ++tx_byte_count;
        if (tx_byte_count == cec_tx_message_size) {
          // EOM
          set_low_high_time(1);
        } else {
          tx_byte = cec_msg_buffer[tx_byte_count];
          tx_bit_count = 0;
          set_low_high_time(0);
        }
        // Generate EOM bit.
        CEC_msg.tx_status = 4;
      }
      start_cec_tx_timer(tx_high_time);
      break;
    }
    // Generate EOM bit low time.
    case 4: {
      set_tx_low();
      start_cec_tx_timer(tx_low_time);
      CEC_msg.tx_status = 5;
      break;
    }
    // Generate EOM bit high time.
    case 5: {
      set_tx_high();
      CEC_msg.tx_status = 6;
      start_cec_tx_timer(tx_high_time);
      break;
    }
    // Generate acknowledge bit(1) low time.
    case 6: {
      set_tx_low();
      // Enable checking for ackonwledge.
      check_acknowledge();
      CEC_msg.tx_status = 7;
      start_cec_tx_timer(LOGICAL_ONE_LOW_TIME);
      break;
    }
    // Generate acknowledge bit high time.
    case 7: {
      set_tx_low();
      set_low_high_time(tx_byte & 0x80);
      ++tx_bit_count;
      tx_byte = tx_byte << 1;
      CEC_msg.tx_status = 8;
      start_cec_tx_timer(LOGICAL_ONE_HIGH_TIME);
      break;
    }
    // Check acknowledge returned.
    case 8: {
      if (acknowledge_received) {
        // There more blocks to transmit.
        if (tx_byte_count < cec_tx_message_size) {
          set_tx_low();
          start_cec_tx_timer(tx_low_time);
          // Start next Tx state.
          CEC_msg.tx_status = 3;
        } else {  // Transmission successful.
          // Stop Tx time 2.
          stop_tx_timer();
          operation_handler(CEC_TX_DONE_IND, cec_retransmit);
          // Time to wait before next Tx, why?
          time_before_tx = 7;
          enable_receive();
        }
      } else {  // No acknoledge.
        // Try retransmit.
        if (cec_retransmit > 0) {
          --cec_retransmit;
          start_cec_tx_timer(DATA_BIT_LENGTH);
          time_before_tx = 3;  // Why?

          cec_bus_free_time = 0;
          tx_byte_count = 0;
          tx_bit_count = 0;
          tx_byte = cec_msg_buffer[0];
          CEC_msg.tx_status = 0;
          start_cec_rx_capture();
        } else {  // Transmission failed.
          // Stop tx timer 2.
          stop_tx_timer();
          operation_handler(CEC_TX_NACK_IND, 0);
        }
        // Enable receiving.
        enable_receive();
      }
      break;
    }
  }
}

void CEC_handler(void) {
  if (check_event_source(0)) {
    printf("get TAPND.\n");
    clear_event_pending_bit(0);
    CEC_receive_handler();
  }
  if (check_event_source(3)) {
    printf("get TDPND.\n");
    clear_event_pending_bit(3);
    CEC_transmit_handler();
  }
}

int main() {
  CEC_init(CEC_callback);
  uint8_t command = 0x37;
  CEC_start_transmit(0, 15, 1, &command, 2);
  return 0;
}
