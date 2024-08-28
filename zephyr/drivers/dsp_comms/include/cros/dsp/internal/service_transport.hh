/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <pb_encode.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include "cros/dsp/service/transport_state_machine.hh"
#include "proto/ec_ish.pb.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_preprocessor/util.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace cros::dsp::service {

template <typename StatusType,
          uint8_t kStatusFlagHasResponse,
          size_t kResponseBufferSize,
          size_t kStatusBufferSize,
          typename StatusFieldType,
          StatusFieldType StatusType::*flags_field,
          typename ResponseSizeFieldType,
          ResponseSizeFieldType StatusType::*response_size_field>
class Transport : public TransportStateMachine {
 public:
  template <typename T>
  using SerializeFn = pw::Function<pw::StatusWithSize(const T& data, pw::ByteSpan out)>;

  explicit constexpr Transport(SerializeFn<StatusType>&& serialize_status_fn)
      : status_(),
        status_flags_(),
        serialize_status_fn_(std::move(serialize_status_fn)),
        notify_client_fn_(nullptr) {
    status_flags_ = pw::ByteSpan(reinterpret_cast<std::byte*>(&(status_.*flags_field)),
                                 sizeof(std::remove_reference_t<decltype(status_.*flags_field)>));
  }

  virtual ~Transport() = default;

  void SetNotifyClientCallback(pw::Function<void(bool has_data)>&& notify_client_fn) {
    notify_client_fn_ = std::move(notify_client_fn);
  }

  template <typename T>
  pw::Status StageResponse(const T& response, SerializeFn<T> serialize) {
    // Must be either IDLE or HAS_STAGED_STATUS
    if (!StateAnyOf(
            {TransportStateMachine::State::IDLE, TransportStateMachine::State::HAS_STATUS})) {
      return pw::Status::ResourceExhausted();
    }

    // Serialize the result
    auto serialize_result = serialize(response, response_buffer_.FullSpan());
    if (!serialize_result.ok()) {
      return serialize_result.status();
    }
    response_buffer_.SetSize(serialize_result.size());

    // Update the size field
    if (auto rc = UpdateResponseSize(serialize_result.size()); !rc.ok()) {
      return rc;
    }

    // Update the status flag to signal there's data
    if (auto rc = DoSetStatusBit(kStatusFlagHasResponse); !rc.ok()) {
      return rc;
    }

    printf("Setting new state to HAS_STATUS_AND_RESPONSE\n");

    return SetNewStateAndNotify(TransportStateMachine::State::HAS_STATUS_AND_RESPONSE);
  }

  pw::Status SetStatusBit(uint8_t bit_num) {
    if (auto status = DoSetStatusBit(bit_num); !status.ok()) {
      return status;
    }

    auto state = current_state();
    if (state == TransportStateMachine::State::IDLE) {
      return SetNewStateAndNotify(TransportStateMachine::State::HAS_STATUS);
    }
    return pw::OkStatus();
  }

  pw::Result<pw::ConstByteSpan> ReadNextMessage() {
    if (StateAnyOf({TransportStateMachine::State::HAS_STATUS,
                    TransportStateMachine::State::HAS_STATUS_AND_RESPONSE})) {
      auto serialize_result = serialize_status_fn_(status_, status_buffer_.FullSpan());
      if (!serialize_result.ok()) {
        return serialize_result.status();
      }
      status_ = {};
      status_buffer_.SetSize(serialize_result.size());
      if (current_state() == TransportStateMachine::State::HAS_STATUS) {
        SetNewStateAndNotify(TransportStateMachine::State::IDLE);
      } else {
        SetNewStateAndNotify(TransportStateMachine::State::HAS_RESPONSE);
      }
      printf("Returning status\n");
      return status_buffer_.DataSpan();
    }

    if (current_state() == TransportStateMachine::State::HAS_RESPONSE) {
      // At this point we may have gotten a new status bit since we sent the
      // last one. Check to see if the status is empty.
      StatusType empty_status = {};
      if (std::memcmp(&status_, &empty_status, sizeof(StatusType)) == 0) {
        printf("Returning response, moving to IDLE\n");
        SetNewStateAndNotify(TransportStateMachine::State::IDLE);
      } else {
        printf("Returning response, moving to HAS_STATUS\n");
        SetNewStateAndNotify(TransportStateMachine::State::HAS_STATUS);
      }
      return response_buffer_.DataSpan();
    }

    return pw::Status::NotFound();
  }

 private:
  using TransportStateMachine::SetNewState;

  pw::Status SetNewStateAndNotify(TransportStateMachine::State new_state) {
    auto old_state = current_state();
    if (auto rc = SetNewState(new_state); !rc.ok()) {
      return rc;
    }
    if (new_state == old_state) {
      printf("State didn't change, returning OK\n");
      return pw::OkStatus();
    }
    if (new_state == TransportStateMachine::State::HAS_STATUS ||
        new_state == TransportStateMachine::State::HAS_STATUS_AND_RESPONSE) {
      notify_client_fn_(true);
    } else if ((new_state == TransportStateMachine::State::IDLE &&
                old_state == TransportStateMachine::State::HAS_STATUS) ||
               (new_state == TransportStateMachine::State::HAS_RESPONSE &&
                old_state == TransportStateMachine::State::HAS_STATUS_AND_RESPONSE)) {
      notify_client_fn_(false);
    }
    return pw::OkStatus();
  }

  pw::Status DoSetStatusBit(uint8_t bit_num) {
    if (bit_num / 8 >= status_flags_.size()) {
      return pw::Status::OutOfRange();
    }
    reinterpret_cast<uint8_t*>(status_flags_.data())[bit_num / 8] |= (1 << (bit_num % 8));
    return pw::OkStatus();
  }

  pw::Status UpdateResponseSize(size_t new_size) {
    if (new_size > std::numeric_limits<ResponseSizeFieldType>::max()) {
      return pw::Status::InvalidArgument();
    }
    (status_.*response_size_field) = new_size;
    return pw::OkStatus();
  }

  StatusType status_ = {};
  pw::ByteSpan status_flags_;
  SerializeFn<StatusType> serialize_status_fn_;
  pw::Function<void(bool has_data)> notify_client_fn_;

  template <size_t kCapacity>
  class Buffer {
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

}  // namespace cros::dsp::service
