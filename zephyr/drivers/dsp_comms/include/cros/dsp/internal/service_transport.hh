/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <algorithm>
#include <cstddef>

#include "proto/ec_ish.pb.h"

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_preprocessor/util.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

#include <pb_encode.h>
#include <type_traits>

namespace cros::dsp::service {

using GetStatusResponse = cros_dsp_comms_GetStatusResponse;

template <typename StatusType, auto StatusType:: *FlagsField,
          auto StatusType:: *ResponseSizeField, uint8_t StatusFlagHasResponse,
          size_t kResponseBufferSize, size_t kStatusBufferSize>
class Transport {
public:
  template <typename T>
  using SerializeFn =
      pw::Function<pw::StatusWithSize(const T *data, pw::ByteSpan out)>;

  Transport(SerializeFn<StatusType> &&serialize_status_fn,
            pw::Function<void(bool has_data)> &&notify_client_fn)
      : read_state_(ReadState::IDLE),
        serialize_status_fn_(std::move(serialize_status_fn)),
        notify_client_fn_(std::move(notify_client_fn)) {
    notify_client_fn_(false);
  }

  template <typename T>
  pw::Status StageResponse(const T *response, SerializeFn<T> serialize) {
    // Must be either IDLE or HAS_STAGED_STATUS
    if (ReadStateNoneOf({ReadState::IDLE, ReadState::HAS_STAGED_STATUS})) {
      return pw::Status::ResourceExhausted();
    }

    // Serialize the result
    auto serialize_result = serialize(response, response_buffer_.FullSpan());
    if (!serialize_result.ok()) {
      return serialize_result.status();
    }
    response_buffer_.SetSize(serialize_result.size());

    // Update the status
    if (auto rc = SetStatusFlag(StatusFlagHasResponse); !rc.ok()) {
      return rc;
    }
    status_.*ResponseSizeField = serialize_result.size();
    SetNewState(ReadState::HAS_STAGED_DATA_AND_STATUS);

    return pw::OkStatus();
  }

  constexpr size_t FlagsSize() const {
    return sizeof(std::remove_pointer_t<decltype((std::declval<StatusType &>().*
                                                  FlagsField))>);
  }

  pw::Status SetStatusFlag(uint8_t bit_num) {
    printf("bit_num   = %u\n", bit_num);
    printf("FlagsSize = %zu\n", FlagsSize());
    if (bit_num / 8 >= FlagsSize()) {
      return pw::Status::OutOfRange();
    }
    static_cast<uint8_t *>(status_.*FlagsField)[bit_num / 8] |=
        (1 << (bit_num % 8));

    if (read_state_ == ReadState::IDLE) {
      SetNewState(ReadState::HAS_STAGED_STATUS);
    } else if (read_state_ == ReadState::READ_STATUS_HAS_DATA) {
      // TODO somehow add this logic to SetNewState
      // We can't change the state otherwise the next read will return a status
      // again, but we should let the client know that there is a new status
      notify_client_fn_(true);
    }
    return pw::OkStatus();
  }

  pw::Result<pw::ConstByteSpan> GetNextResponse() {
    if (ReadStateAnyOf({ReadState::HAS_STAGED_STATUS,
                        ReadState::HAS_STAGED_DATA_AND_STATUS})) {
      auto serialize_result =
          serialize_status_fn_(&status_, status_buffer_.FullSpan());
      if (!serialize_result.ok()) {
        return serialize_result.status();
      }
      status_ = {};
      status_buffer_.SetSize(serialize_result.size());
      if (read_state_ == ReadState::HAS_STAGED_STATUS) {
        SetNewState(ReadState::IDLE);
      } else {
        SetNewState(ReadState::READ_STATUS_HAS_DATA);
      }
      return status_buffer_.DataSpan();
    }

    if (read_state_ == ReadState::READ_STATUS_HAS_DATA) {
      // At this point we may have gotten a new status bit since we sent the
      // last one. Check to see if the status is empty.
      StatusType empty_status = {};
      if (std::memcmp(&status_, &empty_status, sizeof(StatusType)) == 0) {
        SetNewState(ReadState::IDLE);
      } else {
        SetNewState(ReadState::HAS_STAGED_STATUS);
      }
      return response_buffer_.DataSpan();
    }

    return pw::Status::NotFound();
  }

private:
  enum ReadState {
    IDLE,
    HAS_STAGED_STATUS,
    HAS_STAGED_DATA_AND_STATUS,
    READ_STATUS_HAS_DATA,
  };

  ReadState read_state_;

  void SetNewState(ReadState new_state) {
    if (read_state_ == new_state) {
      return;
    }
    bool notify_client = false;
    bool client_msg;
    if (read_state_ == ReadState::IDLE &&
        (new_state == ReadState::HAS_STAGED_STATUS ||
         new_state == ReadState::HAS_STAGED_DATA_AND_STATUS)) {
      // Moved from an idle state to having data.
      notify_client = true;
      client_msg = true;
    }
    if ((read_state_ == ReadState::HAS_STAGED_STATUS ||
         read_state_ == ReadState::HAS_STAGED_DATA_AND_STATUS) &&
        (new_state == ReadState::IDLE ||
         new_state == ReadState::READ_STATUS_HAS_DATA)) {
      // moved from having a status to not having a status.
      notify_client = true;
      client_msg = false;
    }
    read_state_ = new_state;
    if (notify_client) {
      notify_client_fn_(client_msg);
    }
  }

  bool ReadStateAnyOf(std::vector<ReadState> options) const {
    return std::any_of(
        options.cbegin(), options.cend(),
        [this](ReadState other) -> bool { return read_state_ == other; });
  }

  bool ReadStateNoneOf(std::vector<ReadState> options) const {
    return std::none_of(
        options.cbegin(), options.cend(),
        [this](ReadState other) -> bool { return read_state_ == other; });
  }

  StatusType status_ = {};
  SerializeFn<StatusType> serialize_status_fn_;
  pw::Function<void(bool has_data)> notify_client_fn_;

  template <size_t kCapacity> class Buffer {
  public:
    void Clear() { memset(buffer_, 0, kCapacity); }
    void SetSize(size_t size) {
      PW_ASSERT(size <= kCapacity);
      size_ = size;
    }
    pw::ByteSpan FullSpan() { return pw::ByteSpan(buffer_, capacity_); }
    pw::ByteSpan DataSpan() { return pw::ByteSpan(buffer_, size_); }

  private:
    std::byte buffer_[kCapacity] = {};
    const size_t capacity_ = kCapacity;
    size_t size_ = 0;
  };

  Buffer<kResponseBufferSize> response_buffer_;
  Buffer<kStatusBufferSize> status_buffer_;
};

class CrosTransport {
public:
  CrosTransport();
  pw::Status StageResponse(const void *response, const pb_msgdesc_t *fields);
  pw::Status SetStatusBit(uint8_t bit);

  pw::Result<pw::ConstByteSpan> GetNextResponse();

private:
  enum ReadState {
    IDLE,
    HAS_STAGED_STATUS,
    HAS_STAGED_DATA_AND_STATUS,
    READ_STATUS_HAS_DATA,
  };

  ReadState read_state_;

  template <size_t kCapacity> struct Buffer {
    std::byte buffer[kCapacity] = {};
    const size_t capacity = kCapacity;
    size_t size = 0;
  };
  Buffer<CONFIG_PLATFORM_EC_DSP_SERVICE_RESPONSE_BUFFER_SIZE> response_buffer_;

  cros_dsp_comms_GetStatusResponse status_;
  Buffer<cros_dsp_comms_GetStatusResponse_size + 4> status_buffer_;
};

} // namespace cros::dsp::service
