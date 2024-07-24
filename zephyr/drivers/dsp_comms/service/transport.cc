#include "cros/dsp/internal/service_transport.hh"

#include <pb_encode.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(dsp_service, CONFIG_DSP_COMMS_LOG_LEVEL);

namespace cros::dsp::service {

namespace {
constexpr const size_t kStatusFieldLen =
    ARRAY_SIZE(((GetStatusResponse *)0)->flags);
static_assert(kStatusFieldLen * 8 < UINT8_MAX);

constexpr const GetStatusResponse kDefaultStatusResponseValue =
    cros_dsp_comms_GetStatusResponse_init_zero;

constexpr const pb_msgdesc_t *kGetStatusResponseFields =
    cros_dsp_comms_GetStatusResponse_fields;
} // namespace

CrosTransport::CrosTransport() : read_state_(CrosTransport::ReadState::IDLE) {}

pw::Status CrosTransport::StageResponse(const void *response,
                                    const pb_msgdesc_t *fields) {
  PW_ASSERT(read_state_ == ReadState::IDLE ||
            read_state_ == ReadState::HAS_STAGED_STATUS);

  // Attempt to encode the response
  pb_ostream_t ostream = pb_ostream_from_buffer(
      reinterpret_cast<pb_type_t *>(response_buffer_.buffer),
      response_buffer_.capacity);
  bool encode_success = pb_encode(&ostream, fields, response);

  if (!encode_success) {
    LOG_ERR("Failed to encode");
    return pw::Status::ResourceExhausted();
  }

  response_buffer_.size = ostream.bytes_written;
  status_.response_length = ostream.bytes_written;
  if (!SetStatusBit(cros_dsp_comms_GetStatusResponse_Flag_RESPONSE_READY)
           .ok()) {
    LOG_ERR("Failed to set RESPONSE_READY bit");
    return pw::Status::Internal();
  }

  read_state_ = ReadState::HAS_STAGED_DATA_AND_STATUS;
  return pw::OkStatus();
}

pw::Status CrosTransport::SetStatusBit(uint8_t bit) {
  PW_ASSERT(bit / 8 < kStatusFieldLen);

  status_.flags[bit / 8] |= BIT(bit % 8);
  LOG_DBG("Set bit [%d]::%d", bit / 8, bit % 8);

  if (read_state_ == ReadState::IDLE) {
    read_state_ = ReadState::HAS_STAGED_STATUS;
  }
  return pw::OkStatus();
}

pw::Result<pw::ConstByteSpan> CrosTransport::GetNextResponse() {
  // Check for a pending status
  if (read_state_ == ReadState::HAS_STAGED_STATUS ||
      read_state_ == ReadState::HAS_STAGED_DATA_AND_STATUS) {
    pb_ostream_t ostream = pb_ostream_from_buffer(
        reinterpret_cast<pb_type_t *>(status_buffer_.buffer),
        status_buffer_.capacity);
    bool encode_success =
        pb_encode_delimited(&ostream, kGetStatusResponseFields, &status_);
    if (!encode_success) {
      LOG_ERR("Failed to encode current Status");
      return pw::Status::ResourceExhausted();
    }
    status_ = kDefaultStatusResponseValue;
    status_buffer_.size = ostream.bytes_written;
    if (read_state_ == ReadState::HAS_STAGED_STATUS) {
      read_state_ = ReadState::IDLE;
    } else {
      read_state_ = ReadState::READ_STATUS_HAS_DATA;
    }
    return pw::ConstByteSpan(status_buffer_.buffer, status_buffer_.size);
  }

  // Check for pending response
  if (read_state_ == ReadState::READ_STATUS_HAS_DATA) {
    // At this point we may have gotten a new status bit since we sent the last
    // one. Check to see if the status is empty.
    if (memcmp(&status_, &kDefaultStatusResponseValue,
               sizeof(kDefaultStatusResponseValue)) == 0) {
      // The status is empty
      read_state_ = ReadState::IDLE;
    } else {
      read_state_ = ReadState::HAS_STAGED_STATUS;
    }
    return pw::ConstByteSpan(response_buffer_.buffer, response_buffer_.size);
  }

  return pw::Status::NotFound();
}

} // namespace cros::dsp::service