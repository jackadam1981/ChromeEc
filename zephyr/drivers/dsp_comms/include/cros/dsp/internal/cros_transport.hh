/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <pb_encode.h>

#include <cstdint>

#include "cros/dsp/internal/service_transport.hh"
#include "proto/ec_ish.pb.h"

namespace cros::dsp::service {

using CrosTransportParent =
    Transport<cros_dsp_comms_GetStatusResponse,
              cros_dsp_comms_GetStatusResponse_Flag_RESPONSE_READY,
              CONFIG_PLATFORM_EC_DSP_SERVICE_RESPONSE_BUFFER_SIZE,
              cros_dsp_comms_GetStatusResponse_size + 4,
              decltype(cros_dsp_comms_GetStatusResponse::flags),
              &cros_dsp_comms_GetStatusResponse::flags,
              decltype(cros_dsp_comms_GetStatusResponse::response_length),
              &cros_dsp_comms_GetStatusResponse::response_length>;

class CrosTransport : public CrosTransportParent {
 public:
  CrosTransport()
      : CrosTransportParent(
            [](const auto& status, pw::ByteSpan out) -> pw::StatusWithSize {
              printf("Serializing status flags=[0x%02x, 0x%02x], size=%u\n",
                     status.flags[0],
                     status.flags[1],
                     status.response_length);
              pb_ostream_t response_ostream = pb_ostream_from_buffer(
                  reinterpret_cast<pb_type_t*>(out.data()), out.size());
              bool response_encode_success =
                  pb_encode_delimited(&response_ostream,
                                      cros_dsp_comms_GetStatusResponse_fields,
                                      &status);
              if (!response_encode_success) {
                return pw::StatusWithSize(pw::Status::ResourceExhausted(), 0);
              }

              return pw::StatusWithSize(response_ostream.bytes_written);
            }) {}

  pw::Status StageResponse(const cros_dsp_comms_GetCbiFlagsResponse& response) {
    return this->template StageResponse<cros_dsp_comms_GetCbiFlagsResponse>(
        response,
        [](const auto& response, pw::ByteSpan out) -> pw::StatusWithSize {
          pb_ostream_t response_ostream = pb_ostream_from_buffer(
              reinterpret_cast<pb_type_t*>(out.data()), out.size());
          bool response_encode_success =
              pb_encode(&response_ostream,
                        cros_dsp_comms_GetCbiFlagsResponse_fields,
                        &response);
          if (!response_encode_success) {
            return pw::StatusWithSize(pw::Status::ResourceExhausted(), 0);
          }

          return pw::StatusWithSize(response_ostream.bytes_written);
        });
  }

  using CrosTransportParent::SetNotifyClientCallback;

 protected:
  using CrosTransportParent::StageResponse;

  pw::Status UpdateResponseSize(cros_dsp_comms_GetStatusResponse& status,
                                size_t new_size) {
    if (new_size > UINT8_MAX) {
      return pw::Status::OutOfRange();
    }
    status.response_length = new_size;
    return pw::OkStatus();
  }
};

}  // namespace cros::dsp::service
