#ifndef _CEC_H_
#define _CEC_H_

#include <stdbool.h>
#include <stdint.h>

#define CEC_MAX_MSG_SIZE 16

#define CEC_MIN_RETRANS 1
#define CEC_MAX_RETRANS 5

typedef enum {
  WAIT_BUS_FREE_TIME = 0,
  SEND_START_BIT_LOW = 1,
  SEND_START_BIT_HIGH = 2,
  SEND_DATA_BYTE = 3,
  SEND_DATA_BIT_LOW = 3,
  SEND_DATA_BIT_HIGH = 4,
  SEND_EOM_LOW = 5,
  SEND_EOM_HIGH = 6,
  SEND_ACK_LOW = 7,
  SEND_ACK_HIGH = 8,
  WAIT_FOR_ACK = 9,
  SEND_ACK = 10
} CEC_TRANSMIT_STATUS;

typedef enum { RECEIVE_ACK_BIT = 0 } CEC_RECEIVE_STATUS;

struct cec_msg {
  uint32_t msg_len;
  // uint32_t timeout;
  // uint32_t sequence;
  // uint32_t flags;
  uint8_t msg[CEC_MAX_MSG_SIZE];
  // uint8_t reply;
  uint8_t retransmit;
  CEC_RECEIVE_STATUS rx_status;
  CEC_TRANSMIT_STATUS tx_status;
  // uint8_t tx_arb_lost_cnt;
  // uint8_t tx_nack_cnt;
  // uint8_t tx_low_drive_cnt;
  // uint8_t tx_error_cnt;
};

typedef enum {
  CEC_ADDR_TV = 0,
  CEC_ADDR_RECORD_1 = 1,
  CEC_ADDR_RECORD_2 = 2,
  CEC_ADDR_TUNER_1 = 3,
  CEC_ADDR_PLAYBACK_1 = 4,
  CEC_ADDR_AUDIO = 5,
  CEC_ADDR_TUNER_2 = 6,
  CEC_ADDR_TUNER_3 = 7,
  CEC_ADDR_PLAYBACK_2 = 8,
  CEC_ADDR_RECORD_3 = 9,
  CEC_ADDR_TUNER_4 = 10,
  CEC_ADDR_PLAYBACK_3 = 11,
  CEC_ADDR_RSVD_1 = 12,
  CEC_ADDR_RSVD_2 = 13,
  CEC_ADDR_FREE = 14,
  CEC_ADDR_BROADCAST = 15,
  CEC_ADDR_ALL = 16,
  CEC_ADDR_NONE = 17,
  CEC_ADDR_LAST = 18
} CEC_logical_addr_t;

typedef enum {
  CEC_OWNADDR_MAIN = 0,
  CEC_OWNADDR_1 = 1,
  CEC_OWNADDR_2 = 2,
  CEC_OWNADDR_LAST = 3
} CEC_own_addr_t;

typedef enum {
  CEC_NO_STATUS_IND,
  CEC_RCV_UNI_IND,
  CEC_RCV_BC_IND,
  CEC_TX_DONE_IND,
  CEC_TX_NACK_IND,
  CEC_TX_LINE_ERR_IND,
  CEC_WAKE_UP_IND,
  CEC_IND_LAST
} CEC_status_ind_t;

typedef void (*CEC_callback_t)(CEC_status_ind_t op_status, uint8_t info);

// void CEC_init(CEC_callback_t operation_done);
// void CEC_receive_config(uint8_t* rcv_data, uint8_t* bc_data,
//                         bool rcv_overrun_en, bool bc_overrun_en);
// void CEC_receive_enable(bool rcv_enable, bool bc_enable);
// bool CEC_start_transmit(CEC_logical_addr_t initiator, CEC_logical_addr_t
// dest,
//                         uint8_t len, uint8_t* write_data, uint8_t
//                         retransmit);
// bool CEC_reset(void);
//
// void CEC_handler(void);

#endif  // _CEC_H_
